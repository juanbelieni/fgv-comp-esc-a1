from args import args
from models import Highway
from simulation import Simulation, SimulationParams
import rpc

if __name__ == "__main__":
    highway = Highway(
        name=args.name,
        lanes=args.lanes,
        size=args.size,
        speed_limit=args.speed_limit,
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
        cycle_duration=args.duration,
    )

    rpc_stub = rpc.connect()

    simulation = Simulation(
        highway,
        params,
        silent=not args.print,
        rpc_stub=rpc_stub
    )

    simulation.run()
