#!/usr/bin/env python3
"""Converts an ASCII triangulation definition to a binary trig file."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from lib.trigfile import TrigFormat, build_trig_file, parse_trig_format


def main(argv: list[str] | None = None) -> int:
    """Parses arguments and builds the output trig file from the input triangulation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-i", "--input", required=True, type=Path, help="ASCII triangulation definition to read")
    parser.add_argument("-o", "--output", required=True, type=Path, help="trig file to write")
    parser.add_argument(
        "-f",
        "--format",
        metavar="FORMAT",
        help=f"binary output format ({', '.join(fmt.name for fmt in TrigFormat)}); "
        "if omitted, uses the target format named in the source's own FORMAT: header line",
    )
    args = parser.parse_args(argv)

    big_endian = None
    if args.format is not None:
        try:
            big_endian = parse_trig_format(args.format).big_endian
        except ValueError as error:
            parser.error(str(error))

    try:
        build_trig_file(args.input, args.output, big_endian)
    except (ValueError, OSError) as error:
        print(f"trig.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
