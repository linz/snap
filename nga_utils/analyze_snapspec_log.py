#!/usr/bin/python3
"""Analyzes the accuracy tests in a snapspec log, and writes one line for each test to a csv file."""

import argparse
import re
from typing import Iterator, TextIO

# Each pattern with the name of the test that it recognises.
TEST_PATTERNS = [
    (
        r"\s+Station\s+(?P<stn1>\w+)\s+(?P<status>passes)\s+(?:vrt\s+)?rel\s+acc\s+from\s+abs\s+accuracy\s+"
        r"\((?P<v1>[\d\.]+)\s+\<\s+(?P<v2>[\d\.]+)\)",
        "RAFA",
    ),
    (
        r"\s+Station\s+(?P<stn1>\w+)\s+(?P<status>fails)\s+(?:vrt\s+)?abs\s+accuracy\s+"
        r"\((?P<v1>[\d\.]+)\s+\>\s+(?P<v2>[\d\.]+)\)",
        "AA",
    ),
    (
        r"\s+Station\s+(?P<stn1>\w+)\s+(?P<status>fails)\s+on\s+rel\s+(?:vrt\s+)?accuracy\s+to\s+passed\s+station\s+"
        r"(?P<stn2>\w+)\s+\((?P<v1>[\d\.]+)\s+\>\s+(?P<v2>[\d\.]+)\)",
        "RA",
    ),
    (r"\s+Station\s+(?P<stn1>\w+)\s+(?P<status>passes)\s+rel\s+accuracy\s+tests", "RA"),
]

# The lines that are not a test, and are not reported.
SKIP_PATTERNS = [
    r"\s+Setting\s+order\s+of\s+node\s+(?P<stn>\w+)\s+to\s+(\w+)",
]

TESTS = [(re.compile(pattern), test) for pattern, test in TEST_PATTERNS]
SKIPS = [re.compile(pattern) for pattern in SKIP_PATTERNS]

CSV_COLUMNS = ["phase", "order", "stn1", "stn2", "test", "status", "v1", "v2"]

# The most lines that are not recognised that are shown before the analysis stops.
MAX_UNRECOGNISED = 1000


def parse_snapspec_tests(logf: TextIO) -> Iterator[dict[str, str]]:
    """Yields the values of each test in the log, with the phase and order of the test run that it is part of.

    The lines before the first test run are ignored. A line that is neither a test nor one of the skipped
    lines is printed.
    """
    phase = ""
    order = ""
    nphase = 0
    nbad = 0
    phasere = re.compile(r"Running\s+test\s+for\s+order\s+(?P<order>\w+)")
    for line in logf:
        match = phasere.match(line)
        if match:
            nphase += 1
            phase = str(nphase)
            order = match.group("order")
            continue
        if nphase == 0:
            continue
        matched = False
        for pattern, test in TESTS:
            match = pattern.match(line)
            if match:
                result = match.groupdict()
                result["status"] = result["status"][0].upper()
                result["phase"] = phase
                result["order"] = order
                result["test"] = test
                yield result
                matched = True
                break
        for pattern in SKIPS:
            if pattern.match(line):
                matched = True
                break
        if not matched:
            print(line.rstrip())
            nbad += 1
            if nbad > MAX_UNRECOGNISED:
                break


def main() -> None:
    """Writes the tests of the log to the csv file."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapspec_log_file", help="Input snapspec log file")
    parser.add_argument("output_csv_file", help="Output csv file")
    args = parser.parse_args()

    with (
        open(args.snapspec_log_file, encoding="utf-8") as logf,
        open(args.output_csv_file, "w", encoding="utf-8") as csvf,
    ):
        csvf.write(",".join(CSV_COLUMNS))
        csvf.write("\n")
        for result in parse_snapspec_tests(logf):
            data = [result.get(column, "") for column in CSV_COLUMNS]
            csvf.write(",".join(data))
            csvf.write("\n")


if __name__ == "__main__":
    main()
