from dataclasses import dataclass, field
from random import randint, random, choice
from args import args
from time import sleep
from uuid import uuid4
from typing import Optional


def clamp(value, min_value, max_value):
    return max(min(value, max_value), min_value)


@dataclass
class VehiclePosition:
    lane: int = 0
    dist: int = 0


@dataclass
class Vehicle:
    id: str = field(default_factory=lambda: str(uuid4()))
    pos: VehiclePosition = field(default_factory=VehiclePosition)
    speed: int = 0
    acceleration: int = 0
    collision_time: Optional[int] = None

    def collide(self, cycle: int):
        self.speed = 0
        self.acceleration = 0
        self.collision_time = cycle


@dataclass
class Highway:
    name: str
    speed_limit: int
    lanes: int
    size: int = 200
    outgoing_vehicles: list[Vehicle] = field(default_factory=list)
    incoming_vehicles: list[Vehicle] = field(default_factory=list)

    @property
    def vehicles(self):
        return self.incoming_vehicles + self.outgoing_vehicles


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


class Simulation:
    params: SimulationParams
    highway: Highway
    cycle: int

    def __init__(self, highway: Highway, params: SimulationParams):
        self.highway = highway
        self.params = params
        self.cycle = 0

    def run(self):
        while True:
            self.__generate_vehicles()
            self.__move_vehicles()
            self.__remove_collisions()
            self.__print_status()
            sleep(0.2)

            self.cycle += 1

    def __generate_vehicles(self):
        for vehicles in [
            self.highway.incoming_vehicles,
            self.highway.outgoing_vehicles,
        ]:
            for lane in range(self.highway.lanes):
                if any(v.pos.lane == lane and v.pos.dist <= 1 for v in vehicles):
                    # print(f"Vehicle is waiting to enter lane {lane}")
                    continue

                if random() < self.params.new_vehicle_probability:
                    new_vehicle = Vehicle(
                        speed=randint(self.params.min_speed, self.params.max_speed),
                        pos=VehiclePosition(lane=lane),
                    )
                    vehicles.append(new_vehicle)

    def __move_vehicles(self):
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

            for i, vehicle in enumerate(sorted_vehicles):

                def find_collision(dist, lane):
                    for v in reversed(sorted_vehicles[:i]):
                        if dist >= v.pos.dist and lane == v.pos.lane:
                            return v

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

                if random() < self.params.change_lane_probability:
                    desired_lane = clamp(
                        vehicle.pos.lane + choice([-1, 0, 1]),
                        0,
                        self.highway.lanes - 1,
                    )

                collision = find_collision(vehicle.pos.dist, desired_lane)

                if collision:
                    if random() < self.params.collision_probability and vehicle.speed > 0:
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

                if vehicle.pos.dist >= self.highway.size:
                    vehicles.remove(vehicle)
                    continue

    def __remove_collisions(self):
        for vehicles in [
            self.highway.incoming_vehicles,
            self.highway.outgoing_vehicles,
        ]:
            for vehicle in vehicles:
                if (
                    vehicle.collision_time
                    and self.cycle - vehicle.collision_time
                    > self.params.collision_duration
                ):
                    vehicles.remove(vehicle)

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
