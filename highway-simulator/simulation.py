import os
from dataclasses import dataclass
from random import choice, randint, random
from time import sleep, time
from typing import Optional

import rpc
from models import Highway, Vehicle, VehiclePosition


def clamp(value, min_value, max_value):
    return max(min(value, max_value), min_value)


@dataclass
class SimulationParams:
    new_vehicle_probability: float
    change_lane_probability: float
    collision_probability: float
    collision_duration: int
    max_speed: int
    min_speed: int
    max_acceleration: int
    min_acceleration: int
    cycle_duration: float


class Simulation:
    params: SimulationParams
    highway: Highway
    cycle: int
    num_files: int = 5
    silent: bool = True
    rpc_stub = None

    def __init__(
        self,
        highway: Highway,
        params: SimulationParams,
        silent: bool = True,
        rpc_stub=None,
    ):
        self.highway = highway
        self.params = params
        self.cycle = 0
        self.silent = silent
        self.rpc_stub = rpc_stub

    def run(self):
        while True:
            self.__generate_vehicles()
            self.__move_vehicles()
            self.__remove_collisions()
            self.__print_status()
            self.__report_cycle()
            sleep(self.params.cycle_duration)

            self.cycle += 1

    def __generate_vehicles(self):

        for vehicles in [
            self.highway.incoming_vehicles,
            self.highway.outgoing_vehicles,
        ]:

            for lane in range(self.highway.lanes):

                if any(v.pos.lane == lane and v.pos.dist <= 1 for v in vehicles):
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

                avg_speed = sum(
                    v.speed / (1 + (v.pos.dist - vehicle.pos.dist) ** 2)
                    for v in sorted_vehicles[: i + 1]
                ) / sum(
                    1 / (1 + (v.pos.dist - vehicle.pos.dist) ** 2)
                    for v in sorted_vehicles[: i + 1]
                )

                is_fast = vehicle.speed > avg_speed

                vehicle.acceleration = clamp(
                    vehicle.acceleration
                    + (choice([-1, -1, 0, 0, 1]) if is_fast else choice([-1, 0, 0, 1])),
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
                    if (
                        random()
                        < self.params.collision_probability
                        * (vehicle.speed / self.params.max_speed) ** 2
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
                            vehicle.speed = collision.speed
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
        if self.silent:
            return

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

        print(f"Highway:\t{self.highway.name}")
        print(f"Cycle:\t\t{self.cycle:04d}")
        print(f"Vehicles:\t{len(self.highway.vehicles):04d}")
        print(f"Moving:\t\t{moving_count:04d}")
        print(f"Collisions:\t{collisions_count:04d}")

    def __report_cycle(self):
        if self.rpc_stub:
            rpc.report_cycle(
                self.rpc_stub,
                self.cycle,
                self.highway,
            )
