from dataclasses import dataclass, field
from random import choice
from typing import Optional


def new_id():
    chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    return "".join(choice(chars) for _ in range(7))


@dataclass
class VehiclePosition:
    lane: int = 0
    dist: int = 0


@dataclass
class Vehicle:
    id: str = field(default_factory=lambda: str(new_id()))
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
    lanes: int
    size: int
    speed_limit: int
    outgoing_vehicles: list[Vehicle] = field(default_factory=list)
    incoming_vehicles: list[Vehicle] = field(default_factory=list)

    # Propriedade que retorna todos os veículos da rodovia
    @property
    def vehicles(self):
        return self.incoming_vehicles + self.outgoing_vehicles
