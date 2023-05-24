import grpc
import proto.simulation_pb2 as pb2
import proto.simulation_pb2_grpc as pb2_grpc
from typing import List, Tuple
from time import time
from models import Highway
import socket


def connect() -> pb2_grpc.SimulationServiceStub:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    result = sock.connect_ex(("localhost", 50051))
    if result != 0:
        return None

    channel = grpc.insecure_channel("localhost:50051")
    stub = pb2_grpc.SimulationServiceStub(channel)

    return stub


def report_cycle(
    sub: pb2_grpc.SimulationServiceStub,
    cycle: int,
    highway: Highway,
):

    pb2_vehicles = [
        pb2.RawVehicle(
            plate=vehicle.id,
            direction=direction,
            lane=vehicle.pos.lane,
            distance=vehicle.pos.dist,
        )
        for direction, vehicle in enumerate([
            *highway.incoming_vehicles,
            *highway.outgoing_vehicles,
        ])
    ]

    pb2_highway = pb2.Highway(
        name=highway.name,
        lanes=highway.lanes,
        size=highway.size,
        speed_limit=highway.speed_limit,
    )

    pb2_simulation_cycle = pb2.SimulationCycle(
        cycle=cycle,
        timestamp=time(),
        highway=pb2_highway,
        vehicles=pb2_vehicles,
    )

    sub.ReportCycle(pb2_simulation_cycle)
