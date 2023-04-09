import argparse

parser = argparse.ArgumentParser(description="Highway Simulator")

parser.add_argument(
    "-n",
    "--name",
    type=str,
    help="Highway name",
    required=True,
)
parser.add_argument(
    "-l",
    "--lanes",
    type=int,
    help="Number of lanes in both directions",
    default=2,
)
parser.add_argument(
    "-s",
    "--speed_limit",
    type=int,
    help="Highway speed limit (spaces/cycle)",
    default=10,
)
parser.add_argument(
    "-pv",
    "--new_vehicle_prob",
    type=float,
    help="Probability of a new vehicle entering a lane",
    default=0.1,
)
parser.add_argument(
    "-pl",
    "--change_lane_prob",
    type=float,
    help="Probability of a vehicle changing lanes",
    default=0.2,
)
parser.add_argument(
    "-pc", "--collision_prob", type=float, help="Probability of collision", default=0.05
)
parser.add_argument(
    "-cd",
    "--collision_duration",
    type=int,
    help="Duration (in cycles) for removing vehicles involved in a collision",
    default=5,
)
parser.add_argument(
    "-vmax",
    "--max_speed",
    type=int,
    help="Maximum speed of a vehicle (spaces/cycle)",
    default=10,
)
parser.add_argument(
    "-vmin",
    "--min_speed",
    type=int,
    help="Minimum speed of a vehicle (spaces/cycle)",
    default=1,
)
parser.add_argument(
    "-amax",
    "--max_acceleration",
    type=int,
    help="Maximum acceleration of a vehicle (spaces/cycles^2)",
    default=2,
)
parser.add_argument(
    "-amin",
    "--min_acceleration",
    type=int,
    help="Minimum acceleration of a vehicle (spaces/cycle^2)",
    default=-2,
)


args = parser.parse_args()
