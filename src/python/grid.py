#!/usr/bin/env python3
"""Converts a grid file to another grid format, ASCII or binary."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from lib.gridfile import GridFile, GridFormat, WriteOptions, parse_grid_format


def main(argv: list[str] | None = None) -> int:
    """Parses arguments and converts the input grid file to the output file."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-i", "--input", required=True, type=Path, help="grid file to read (ASCII or binary)")
    parser.add_argument("-o", "--output", required=True, type=Path, help="grid file to write")
    parser.add_argument(
        "-f",
        "--format_out",
        metavar="FORMAT",
        help=f"output format ({', '.join(fmt.name for fmt in GridFormat)}); if omitted, defaults to the format "
        "named in the input's own FORMAT header, if given; otherwise GRID2L for ASCII input, "
        "or ASCII (a readable dump) for binary input",
    )
    parser.add_argument(
        "-H",
        "--header-only",
        action="store_true",
        help="write only the header, not the row data (ASCII output only)",
    )
    parser.add_argument(
        "-I",
        "--integer",
        action="store_true",
        help="write ASCII values as integers rather than real numbers (ASCII output only)",
    )
    args = parser.parse_args(argv)

    try:
        output_format = parse_grid_format(args.format_out) if args.format_out is not None else None
    except ValueError as error:
        parser.error(str(error))

    try:
        grid = GridFile(args.input)
        resolved_format = output_format or grid.default_output_format
        is_ascii_output = resolved_format is GridFormat.ASCII
        if not is_ascii_output:
            if args.header_only:
                parser.error("--header-only is only valid when the output format is ASCII")
            if args.integer:
                parser.error("--integer is only valid when the output format is ASCII")
        grid.write_to_file(
            args.output,
            WriteOptions(
                output_format=output_format,
                real=not args.integer if is_ascii_output else False,
                header_only=args.header_only,
            ),
        )
    except (ValueError, OSError) as error:
        print(f"grid.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
