from dataclasses import dataclass, field
from random import randint, random, choice
from args import args
from time import sleep
from uuid import uuid4
from typing import Optional


# Função auxiliar para limitar um valor entre um mínimo e um máximo
def clamp(value, min_value, max_value):
    return max(min(value, max_value), min_value)


# Classe que representa a posição de um veículo na rodovia
@dataclass
class VehiclePosition:
    lane: int = 0
    dist: int = 0


# Classe que representa um veículo
@dataclass
class Vehicle:
    id: str = field(default_factory=lambda: str(uuid4()))
    pos: VehiclePosition = field(default_factory=VehiclePosition)
    speed: int = 0
    acceleration: int = 0
    collision_time: Optional[int] = None

    # Método que é chamado quando o veículo colide com outro
    def collide(self, cycle: int):
        self.speed = 0
        self.acceleration = 0
        self.collision_time = cycle


# Classe que representa a rodovia
@dataclass
class Highway:
    name: str
    speed_limit: int
    lanes: int
    size: int = 200
    outgoing_vehicles: list[Vehicle] = field(default_factory=list)
    incoming_vehicles: list[Vehicle] = field(default_factory=list)

    # Propriedade que retorna todos os veículos da rodovia
    @property
    def vehicles(self):
        return self.incoming_vehicles + self.outgoing_vehicles


# Classe que representa os parâmetros de simulação
@dataclass
class SimulationParams:
    new_vehicle_probability: float
    change_lane_probability: float
    collision_probability: float
    collision_duration: int
    max_speed: float
    min_speed: float
    max_acceleration: float
    min_acceleration: float

# Classe que representa a simulação
class Simulation:
    params: SimulationParams
    highway: Highway
    cycle: int

    def __init__(self, highway: Highway, params: SimulationParams):
        self.highway = highway
        self.params = params
        self.cycle = 0

    # Método que inicia a simulação
    def run(self):
        while True:
            self.__generate_vehicles()
            self.__move_vehicles()
            self.__remove_collisions()
            self.__print_status()
            self.__report_vehicles()
            sleep(2)

            self.cycle += 1

    # Método que gera novos veículos na rodovia
    def __generate_vehicles(self):
        # Percorre as duas listas de veículos (veículos entrando e saindo da rodovia)
        for vehicles in [
            self.highway.incoming_vehicles,
            self.highway.outgoing_vehicles,
        ]:
            # Ordena os veículos por distância 
            for lane in range(self.highway.lanes):

                # Se já existir um veículo na mesma faixa e a uma distância menor que 1,
                # não gera um novo veículo
                if any(v.pos.lane == lane and v.pos.dist <= 1 for v in vehicles):
                    continue

                # Gera um novo veículo com uma probabilidade definida nos parâmetros
                if random() < self.params.new_vehicle_probability:
                    new_vehicle = Vehicle(
                        speed=randint(self.params.min_speed, self.params.max_speed),
                        pos=VehiclePosition(lane=lane),
                    )
                    vehicles.append(new_vehicle)

    # Método que move os veículos na rodovia
    def __move_vehicles(self):
        # Percorre as duas listas de veículos (veículos entrando e saindo da rodovia)
        for vehicles in [
            self.highway.incoming_vehicles,
            self.highway.outgoing_vehicles,
        ]:
            sorted_vehicles = list(
                reversed(
                    sorted(
                        vehicles,
                        key=lambda vehicle: vehicle.pos.dist,
                    )
                )
            )
            # Percorre os veículos da lista ordenada
            for i, vehicle in enumerate(sorted_vehicles):

                # Função auxiliar para encontrar uma colisão
                def find_collision(dist, lane):
                    for v in reversed(sorted_vehicles[:i]):
                        if dist >= v.pos.dist and lane == v.pos.lane:
                            return v

                # Se o veículo já colidiu, não faz nada        
                if vehicle.collision_time is not None:
                    continue

                vehicle.acceleration = clamp(
                    vehicle.acceleration + choice([-1, 0, 0, 0, 1, 1]),
                    self.params.min_acceleration,
                    self.params.max_acceleration,
                )

                vehicle.speed = clamp(
                    vehicle.speed + vehicle.acceleration,
                    self.params.min_speed,
                    self.params.max_speed,
                )

                vehicle.pos.dist = vehicle.pos.dist + vehicle.speed
                desired_lane = vehicle.pos.lane

                # Probabilidade de mudar de faixa
                if random() < self.params.change_lane_probability:
                    desired_lane = clamp(
                        vehicle.pos.lane + choice([-1, 0, 1]),
                        0,
                        self.highway.lanes - 1,
                    )

                collision = find_collision(vehicle.pos.dist, desired_lane)

                # Se houver colisão, verifica se o veículo pode mudar de faixa
                if collision:
                    if (
                        random() < self.params.collision_probability
                        and vehicle.speed > 0
                    ):
                        vehicle.pos.dist = collision.pos.dist
                        vehicle.collide(self.cycle)
                        collision.collide(self.cycle)
                        continue
                    else:
                        possible_lanes = [
                            clamp(desired_lane + direction, 0, self.highway.lanes - 1)
                            for direction in [-1, 0, 1]
                        ]

                        possible_lanes = [
                            lane
                            for lane in possible_lanes
                            if find_collision(vehicle.pos.dist, lane) is None
                        ]

                        if possible_lanes:
                            desired_lane = choice(possible_lanes)
                        else:
                            vehicle.pos.dist = collision.pos.dist - 1
                            vehicle.speed = 0
                            vehicle.acceleration = 0

                vehicle.pos.lane = desired_lane

                # Se o veículo saiu da rodovia, remove o veículo da lista
                if vehicle.pos.dist >= self.highway.size:
                    vehicles.remove(vehicle)
                    continue

    # Método que remove os veículos que colidiram
    def __remove_collisions(self):
        # Percorre as duas listas de veículos (veículos entrando e saindo da rodovia)
        for vehicles in [
            self.highway.incoming_vehicles,
            self.highway.outgoing_vehicles,
        ]:
            # Percorre os veículos da lista
            for vehicle in vehicles:
                # Se o veículo colidiu e o tempo de colisão é maior
                # que o tempo de duração da colisão,
                # remove o veículo da lista
                if (
                    vehicle.collision_time
                    and self.cycle - vehicle.collision_time
                    > self.params.collision_duration
                ):
                    vehicles.remove(vehicle)

    # Método que mostra o status da simulação
    def __print_status(self):
        def print_vehicles(vehicles, reverse=False):
            for lane in range(self.highway.lanes):
                r = range(0, self.highway.size)
                r = reversed(r) if reverse else r

                for i in r:
                    count = len(
                        [v for v in vehicles if v.pos.lane == lane and v.pos.dist == i]
                    )

                    print("\033[100m", end="")

                    if count == 1:
                        print("\033[92m", end="")
                    if 2 <= count <= 5:
                        print("\033[93m", end="")
                    elif count > 5:
                        print("\033[91m", end="")

                    match count:
                        case 0:
                            print(" ", end="")
                        case n if n < 10:
                            print(n, end="")
                        case _:
                            print("+", end="")

                    print("\033[0m", end="")

                print()

        print("\033[F" * (self.highway.size + 2))

        print_vehicles(self.highway.incoming_vehicles, reverse=True)
        print("\033[100m" + "─" * self.highway.size + "\033[0m")
        print_vehicles(self.highway.outgoing_vehicles)

        moving_count = len(
            [v for v in self.highway.vehicles if v.collision_time is None]
        )

        collisions_count = len(self.highway.vehicles) - moving_count

        print(f"Cycle:\t\t{self.cycle:4}")
        print(f"Vehicles:\t{len(self.highway.vehicles):4}")
        print(f"Moving:\t\t{moving_count:4}")
        print(f"Collisions:\t{collisions_count:4}")

    # Método que gera o relatório de veículos
    def __report_vehicles(self):
        vehicles = []
        vehicles += [
            (str(v.id), 0, v.pos.lane, v.pos.dist)
            for v in self.highway.incoming_vehicles
        ]
        vehicles += [
            (str(v.id), 1, v.pos.lane, v.pos.dist)
            for v in self.highway.outgoing_vehicles
        ]

        with open(f"vehicles.txt", "w") as f:
            text = f"{self.cycle} {len(vehicles)}"
            text += "\n".join([" ".join([str(x) for x in v]) for v in vehicles])
            f.write(text)


if __name__ == "__main__":
    highway = Highway(
        name=args.name,
        speed_limit=args.speed_limit,
        lanes=args.lanes,
    )

    params = SimulationParams(
        new_vehicle_probability=args.new_vehicle_prob,
        change_lane_probability=args.change_lane_prob,
        collision_probability=args.collision_prob,
        collision_duration=args.collision_duration,
        max_speed=args.max_speed,
        min_speed=args.min_speed,
        max_acceleration=args.max_acceleration,
        min_acceleration=args.min_acceleration,
    )

    simulation = Simulation(highway, params)
    simulation.run()
