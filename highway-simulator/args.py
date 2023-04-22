import argparse

parser = argparse.ArgumentParser(description="Highway Simulator")

parser.add_argument(
    "-n",
    "--name",
    type=str,
    help="Highway name",
    default="Highway Simulator",
)

parser.add_argument(
    "-l",
    "--lanes",
    type=int,
    help="Number of lanes in both directions",
    default=5,
)

parser.add_argument(
    "-s",
    "--size",
    type=int,
    help="Size of the highway",
    default=200,
)

parser.add_argument(
    "-sl",
    "--speed-limit",
    type=int,
    help="Speed limit of the highway",
    default=5,
)

parser.add_argument(
    "-pv",
    "--new-vehicle-prob",
    type=float,
    help="Probability of a new vehicle entering a lane",
    default=0.05,
)

parser.add_argument(
    "-pl",
    "--change-lane-prob",
    type=float,
    help="Probability of a vehicle changing lanes",
    default=0.01,
)

parser.add_argument(
    "-pc",
    "--collision-prob",
    type=float,
    help="Probability of collision",
    default=0.05,
)

parser.add_argument(
    "-cd",
    "--collision-duration",
    type=int,
    help="Duration (in cycles) for removing vehicles involved in a collision",
    default=20,
)

parser.add_argument(
    "-vmax",
    "--max-speed",
    type=int,
    help="Maximum speed of a vehicle (spaces/cycle)",
    default=3,
)

parser.add_argument(
    "-vmin",
    "--min-speed",
    type=int,
    help="Minimum speed of a vehicle (spaces/cycle)",
    default=0,
)

parser.add_argument(
    "-amax",
    "--max-acceleration",
    type=int,
    help="Maximum acceleration of a vehicle (spaces/cycles^2)",
    default=1,
)

parser.add_argument(
    "-amin",
    "--min-acceleration",
    type=int,
    help="Minimum acceleration of a vehicle (spaces/cycle^2)",
    default=-1,
)

parser.add_argument(
    "-d",
    "--duration",
    type=float,
    help="Duration (in milliseconds) of each cycle",
    default=1,
)

parser.add_argument(
    "-o",
    "--output-dir",
    type=str,
    help="Output directory",
    required=False,
)

args = parser.parse_args()
