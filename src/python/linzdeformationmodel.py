#!/usr/bin/env python3
"""Converts an ASCII deformation model definition to a binary LINZ deformation model (.ldm) file."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from lib.deformationmodel import LinzDefFormat, build_and_write


def main(argv: list[str] | None = None) -> int:
    """Parses arguments and builds the output deformation model file from the input definition."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-i", "--input", required=True, type=Path, help="ASCII deformation model definition to read")
    parser.add_argument("-o", "--output", required=True, type=Path, help="deformation model file to write")
    parser.add_argument(
        "-f",
        "--format",
        metavar="FORMAT",
        help=f"binary output format ({', '.join(fmt.name for fmt in LinzDefFormat)}), overriding "
        "the source's own required FORMAT header. Only the endianness may differ from that "
        "header - the major format version must still match",
    )
    args = parser.parse_args(argv)

    try:
        build_and_write(args.input, args.output, args.format)
    except (ValueError, OSError) as error:
        print(f"linzdeformationmodel.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
