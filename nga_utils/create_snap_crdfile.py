"""Creates a SNAP coordinate file from a PositioNZ station coordinate file list.

The PositioNZ station coordinates are created using calc_refstation_coordinates
script.  This script uses snapconv to convert the ITRF2008 coordinates to
NZGD2000, to ensure the conversion is using the same version of the deformation
model as SNAP.
"""

import argparse
import csv
import subprocess
from typing import NamedTuple

EXPECTED_FIELDS = "code scm_version deformation_version calc_date itrf2008_X itrf2008_Y itrf2008_Z".split()


class Station(NamedTuple):
    """The code and ITRF2008 geocentric coordinates of a station, as the text of the PositioNZ file."""

    code: str
    x: str
    y: str
    z: str


def read_stations(pnzfile: str) -> tuple[list[Station], set[str], str]:
    """Returns the stations of a PositioNZ file, its SCM versions, and the calculation date of its last station."""
    stations: list[Station] = []
    scm_versions: set[str] = set()
    calc_date = ""

    with open(pnzfile, encoding="utf-8", newline="") as pnzf:
        reader = csv.DictReader(pnzf)
        missing = [x for x in EXPECTED_FIELDS if x not in (reader.fieldnames or [])]
        if missing:
            raise RuntimeError("Missing fields " + " ".join(missing) + " in " + pnzfile)
        for row in reader:
            scm_versions.add(row["scm_version"].split()[0])
            calc_date = row["calc_date"]
            stations.append(Station(row["code"], row["itrf2008_X"], row["itrf2008_Y"], row["itrf2008_Z"]))

    if len(stations) < 1:
        raise RuntimeError(f"No stations found in {pnzfile}")
    return stations, scm_versions, calc_date


def main() -> None:
    """Writes the ITRF2008 coordinates of the stations, then converts them to the SNAP coordinate file."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("positionz_coordinate_csv", help="PositioNZ station coordinate file")
    parser.add_argument("snap_coordinate_file", help="SNAP coordinate file to create")
    args = parser.parse_args()
    pnzfile = args.positionz_coordinate_csv
    crdfile = args.snap_coordinate_file

    stations, scm_versions, calc_date = read_stations(pnzfile)

    print(f"Found {len(stations)} stations")
    print(f"Calc date: {calc_date}")
    print(f"SCM version(s): {' '.join(sorted(scm_versions))}")

    crditrf = crdfile + ".itrf2008"
    with open(crditrf, "w", encoding="utf-8") as crdf:
        crdf.write(f"PositioNZ-PP coordinates at {calc_date}\n")
        crdf.write("ITRF2008_XYZ\n")
        crdf.write("options no_geoid\n\n")
        for s in stations:
            crdf.write(f"{s.code} {s.x} {s.y} {s.z} {s.code}\n")

    print(f"ITRF2008 coordinates in {crditrf}")
    subprocess.call(["snapconv", "-y", calc_date, "-e", "-d", crditrf, "NZGD2000", crdfile])
    subprocess.call(["snapgeoid", "-h", "NZVD2016", crdfile, crdfile])

    print(f"NZGD2000 coordinates in {crdfile}")


if __name__ == "__main__":
    main()
