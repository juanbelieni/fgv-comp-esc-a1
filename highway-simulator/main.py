from dataclasses import dataclass, field
from random import randint, random, choice
from args import args
from time import sleep


def clamp(value, min_value, max_value):
    return max(min(value, max_value), min_value)


emojis = ["😀", "😁", "😂", "🤣", "😃", "😄", "😅", "😆", "😉", "😊",
          "😋", "😎", "😍", "😘", "😜", "😝", "😛", "🤑", "🤗", "🤔",
          "🤐", "🤨", "😐", "😑", "😶", "😏", "😒", "🙄", "😬", "🤥",
          "😌", "😔", "😪", "🤤", "😴", "😷", "🤒", "🤕", "🤢", "🤮",
          "🤧", "😵", "🤯", "🤠", "🤡", "🤫", "🤭", "🧐", "🤓", "😈",
          "👿", "👹", "👺", "💀", "👻", "👽", "🤖", "💩", "😺", "😸",
          "😹", "😻", "😼", "😽", "🙀", "😿", "😾", "🙈", "🙉", "🙊",
          "💋", "💌", "💘", "💝", "💖", "💗", "💓", "💞", "💕", "❣️",
          "💔", "❤️", "🧡", "💛", "💚", "💙", "💜", "🖤", "💟", "❤️‍🔥",
          "❤️‍🩹", "💯", "🔥", "💥", "💢", "💫", "💦", "💨", "🕳️", "💭"]

@dataclass
class Vehicle:
    # id: int = field(default_factory=lambda: randint(0, 1000000))
    id: str = field(default_factory=lambda: choice(emojis))
    position: float = 0
    speed: float = 0
    acceleration: float = 0
    collided: bool = False


@dataclass
class Lane:
    id: int = field(default_factory=lambda: randint(0, 1000000))
    vehicles: list[Vehicle] = field(default_factory=list)


@dataclass
class Highway:
    name: str
    outgoing_lanes: list[Lane] = field(default_factory=list)
    incoming_lanes: list[Lane] = field(default_factory=list)
    speed_limit: float = 10
    size: int = 200

    @property
    def lanes(self):
        return self.outgoing_lanes + self.incoming_lanes


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
            self.cycle += 1
            self.__generate_vehicles()
            self.__move_vehicles()
            # self.__remove_collisions()
            self.__print_status()
            sleep(0.5)

    def __generate_vehicles(self):
        for lane in self.highway.lanes:
            if random() < self.params.new_vehicle_probability:
                lane.vehicles.append(
                    Vehicle(speed=randint(self.params.min_speed, self.params.max_speed))
                )

    def __move_vehicles(self):
        for lanes in [self.highway.incoming_lanes, self.highway.outgoing_lanes]:
            for i, lane in enumerate(lanes):
                for vehicle in lane.vehicles:
                    vehicle.speed = clamp(
                        vehicle.speed + vehicle.acceleration,
                        self.params.min_speed,
                        self.params.max_speed,
                    )

                    vehicle.position += vehicle.speed

                    if vehicle.position >= self.highway.size:
                        lanes[i].vehicles.remove(vehicle)
                        continue

                    if random() < self.params.change_lane_probability:
                        possible_lanes = []
                        if i > 0:
                            possible_lanes.append(lanes[i - 1])
                        if i < len(lanes) - 1:
                            possible_lanes.append(lanes[i + 1])

                        if possible_lanes:
                            lanes[i].vehicles.remove(vehicle)
                            new_lane: Lane = choice(possible_lanes)
                            new_lane.vehicles.append(vehicle)

    def __print_status(self):
        # remove all previous lines
        print("\033[F" * (self.highway.size + 2))

        for lane in self.highway.incoming_lanes:
            for i in range(self.highway.size, 0, -1):
                vehicle = next((v for v in lane.vehicles if v.position == i), None)

                if vehicle:
                    print(vehicle.id, end="")
                else:
                    print("-", end="")
            print()

        print()

        for lane in self.highway.outgoing_lanes:
            for i in range(0, self.highway.size):
                vehicle = next((v for v in lane.vehicles if v.position == i), None)

                if vehicle:
                    print(vehicle.id, end="")
                else:
                    print("-", end="")
            print()


if __name__ == "__main__":
    highway = Highway(
        name=args.name,
        speed_limit=args.speed_limit,
        incoming_lanes=[Lane() for _ in range(args.lanes)],
        outgoing_lanes=[Lane() for _ in range(args.lanes)],
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
