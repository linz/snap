#!/usr/bin/env python3
"""
Compares the output of a regression test with the check data, by the match_ settings of the suite's test.config.

Files are compared line by line, ignoring blank lines and whitespace at the end of a line. Before two lines
that differ are compared, each is changed by the match_replace_re settings, which hide what is expected to
change from run to run, such as dates and versions. Lines that still differ are compared number by number
instead of as text: the text around the numbers must be the same, and each pair of numbers must be within
the absolute or relative tolerance. These settings are used:

    match_replace_re: ~file pattern~line pattern~replacement
        Applies to the files whose name matches the file pattern, or to all files if that is empty. The first
        character after the colon is the delimiter. {datetime} in the line pattern matches a date and time
        such as 1-JAN-2000 00:00:00, and $1 or \1 and so on in the replacement are the groups of the line pattern.
    match_absolute_tolerance: value, match_relative_tolerance: value
        How close a pair of numbers must be, as a difference, or as a fraction of the check value.
    match_line_absolute_tolerance: value file-pattern line-pattern
    match_line_relative_tolerance: value file-pattern line-pattern
        A looser tolerance for lines that match the pattern, in files whose name matches the file pattern.

Two numbers also match if they differ by 180 (when both are less than 370 in size), or by about one unit
in the last decimal place of the number from the check data. If the text around the numbers of two lines
differs, a 180 in it is first changed to 0, so that angles that differ by 180 can match.

Usage:
    compare_output.py --config snap/test.config OUTPUT_DIR CHECK_DIR [--match-prefix PREFIX]

The names of the files are matched against the patterns of the settings as the names under the directories,
with the prefix, if there is one, in front of them.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path

from suite_config import Setting, SuiteError, read_settings, settings_of

# Files are read as latin-1, so that any bytes can be read, and patterns only know the characters of ASCII
DATA_ENCODING = "latin-1"
WHITESPACE = " \t\n\r\f\v"

DEFAULT_ABSOLUTE_TOLERANCE = 1.0e-8
DEFAULT_RELATIVE_TOLERANCE = 1.0e-10

# Two numbers that differ by this much more or less than 180 are still taken to differ by 180
HALF_TURN_TOLERANCE = 0.00001

# Angles are only taken to differ by 180 if they are less than this
ANGLE_LIMIT = 370

# How much bigger than one unit in the last decimal place of a number a difference may be, and still match
LAST_PLACE_FACTOR = 1.1

DATETIME_PATTERN = r"[0-3]?\d\-(JAN|FEB|MAR|APR|MAY|JUN|JUL|AUG|SEP|OCT|NOV|DEC)\-2\d\d\d\s+[0-2]?\d\:\d\d\:\d\d"
REPLACEMENT_TOKEN = re.compile(r"\$(\d+)|\$\{(\d+)\}|\\(\d)|\\(.)", re.DOTALL)
REPLACEMENT_ESCAPES = {"n": "\n", "t": "\t", "r": "\r"}
NUMBER = re.compile(r"(\-?(?:\d+\.|\.\d)\d*(?:e[\-\+]?\d+)?)", re.IGNORECASE | re.ASCII)
RUN_OF_WHITESPACE = re.compile(r"\s+", re.ASCII)
HALF_TURN = re.compile(r"\b180\b", re.ASCII)
LAST_PLACE = re.compile(r"^\-?\d+\.(\d*)\d", re.ASCII)


@dataclass(frozen=True)
class Fix:
    """A change made to the lines of some files, before they are compared."""

    file_pattern: re.Pattern[str] | None
    pattern: re.Pattern[str]
    replacement: str


@dataclass(frozen=True)
class LineTolerance:
    """A tolerance for the lines that match a pattern, in the files that match another."""

    file_pattern: re.Pattern[str]
    line_pattern: re.Pattern[str]
    is_absolute: bool
    tolerance: float


@dataclass(frozen=True)
class MatchRules:
    """The match_ settings of a test.config."""

    fixes: tuple[Fix, ...]
    absolute_tolerance: float
    relative_tolerance: float
    line_tolerances: tuple[LineTolerance, ...]


@dataclass(frozen=True)
class Comparison:
    """The result of comparing the output of a test with its check data."""

    matched: tuple[str, ...]
    differing: tuple[tuple[str, str], ...]  # the name of a file, and how it differs
    missing_from_output: tuple[str, ...]  # in the check data, but the test did not write it
    not_in_check: tuple[str, ...]  # written by the test, but not in the check data

    @property
    def passed(self) -> bool:
        """Whether every file matched. Comparing no files at all is not a pass."""
        return bool(self.matched) and not (self.differing or self.missing_from_output or self.not_in_check)


def tolerance_value(text: str, setting: Setting) -> float:
    """Reads a tolerance, which must be a number above zero."""
    try:
        value = float(text)
    except ValueError:
        value = 0.0
    if value <= 0.0:
        raise SuiteError(f"{setting.location}: invalid tolerance {text}")
    return value


def last_tolerance(settings: Sequence[Setting], item: str, default: float) -> float:
    """Returns the tolerance of an item's last setting, or the default if it has none."""
    found = settings_of(settings, item)
    return tolerance_value(found[-1].value, found[-1]) if found else default


def compile_pattern(text: str, setting: Setting) -> re.Pattern[str]:
    """Compiles a pattern of a setting, which only knows the characters of ASCII."""
    try:
        return re.compile(text, re.ASCII)
    except re.error as error:
        raise SuiteError(f"{setting.location}: invalid regular expression: {error}") from error


def read_fix(setting: Setting) -> Fix:
    """Reads a match_replace_re, which is a delimiter then the file pattern, line pattern and replacement."""
    if not setting.value or re.match(r"\w", setting.value, re.ASCII):
        raise SuiteError(f"{setting.location}: match_replace_re must start with a delimiter that is not a letter")
    parts = (setting.value[1:].split(setting.value[0]) + ["", "", ""])[:3]
    file_text, pattern_text, replacement = (part.rstrip(WHITESPACE) for part in parts)
    return Fix(
        compile_pattern(file_text, setting) if file_text else None,
        compile_pattern(pattern_text.replace("{datetime}", DATETIME_PATTERN), setting),
        replacement,
    )


def read_line_tolerance(setting: Setting) -> LineTolerance:
    """Reads a match_line_absolute_tolerance or match_line_relative_tolerance."""
    parts = setting.value.split()
    if len(parts) != 3:
        raise SuiteError(f"{setting.location}: expected a tolerance, a file pattern and a line pattern")
    return LineTolerance(
        compile_pattern(parts[1], setting),
        compile_pattern(parts[2], setting),
        setting.item == "match_line_absolute_tolerance",
        tolerance_value(parts[0], setting),
    )


def read_match_rules(settings: Sequence[Setting]) -> MatchRules:
    """Reads the match_ settings out of the settings of a test.config."""
    return MatchRules(
        fixes=tuple(read_fix(setting) for setting in settings_of(settings, "match_replace_re")),
        absolute_tolerance=last_tolerance(settings, "match_absolute_tolerance", DEFAULT_ABSOLUTE_TOLERANCE),
        relative_tolerance=last_tolerance(settings, "match_relative_tolerance", DEFAULT_RELATIVE_TOLERANCE),
        line_tolerances=tuple(
            read_line_tolerance(setting)
            for setting in settings_of(settings, "match_line_absolute_tolerance", "match_line_relative_tolerance")
        ),
    )


def expand_replacement(replacement: str, match: re.Match[str]) -> str:
    """Returns the replacement text of a fix for a match: $1 and so on are groups, and a backslash quotes."""

    def token(found: re.Match[str]) -> str:
        digits = found.group(1) or found.group(2) or found.group(3)
        if digits is not None:
            index = int(digits)
            return (match.group(index) or "") if index <= match.re.groups else ""
        return REPLACEMENT_ESCAPES.get(found.group(4), found.group(4))

    return REPLACEMENT_TOKEN.sub(token, replacement)


def apply_fix(fix: Fix, line: str) -> str:
    """Replaces each part of the line that the fix matches."""
    return fix.pattern.sub(lambda match: expand_replacement(fix.replacement, match), line)


def apply_fixes(name: str, line: str, fixes: Sequence[Fix]) -> str:
    """Applies, in order, each of the fixes that is for the file."""
    for fix in fixes:
        if not fix.file_pattern or fix.file_pattern.search(name):
            line = apply_fix(fix, line)
    return line


def split_numbers(line: str) -> tuple[str, list[str]]:
    """Splits a line into its text, with each number replaced by a space and the spaces tidied, and its numbers."""
    numbers = NUMBER.findall(line)
    if not numbers:
        return line, []
    return RUN_OF_WHITESPACE.sub(" ", NUMBER.sub(" ", line)).strip(WHITESPACE), numbers


def output_precision(text: str) -> float:
    """What is left of a number from the output when its decimal part is replaced by 0, or the number if it has none."""
    match = LAST_PLACE.match(text)
    return float("0" + text[match.end() :]) if match else float(text)


def check_precision(text: str) -> float:
    """One unit in the last decimal place of a number from the check data, or the number if it has no decimal part."""
    match = LAST_PLACE.match(text)
    return float("0." + "0" * len(match.group(1)) + "1" + text[match.end() :]) if match else float(text)


def numbers_match(output_text: str, check_text: str, absolute: float, relative: float) -> bool:
    """Whether a number from the output is within tolerance of the one from the check data."""
    output, check = float(output_text), float(check_text)
    difference = abs(output - check)
    if difference <= absolute or difference <= abs(check) * relative:
        return True
    if abs(output) < ANGLE_LIMIT and abs(check) < ANGLE_LIMIT and abs(difference - 180) < HALF_TURN_TOLERANCE:
        return True
    precision = max(output_precision(output_text), check_precision(check_text))
    return difference < precision * LAST_PLACE_FACTOR


def tolerances_for(name: str, line: str, rules: MatchRules) -> tuple[float, float]:
    """Returns the absolute and relative tolerance for a line, which the settings for specific lines can raise."""
    absolute, relative = rules.absolute_tolerance, rules.relative_tolerance
    for specific in rules.line_tolerances:
        if specific.file_pattern.search(name) and specific.line_pattern.search(line):
            if specific.is_absolute:
                absolute = max(absolute, specific.tolerance)
            else:
                relative = max(relative, specific.tolerance)
    return absolute, relative


def compare_line(name: str, output_line: str, check_line: str, rules: MatchRules) -> str:
    """Compares two lines that differ as text. Returns how they differ, or "" if their numbers are close enough."""
    output_words, output_numbers = split_numbers(output_line)
    check_words, check_numbers = split_numbers(check_line)
    if output_words != check_words:
        output_words, check_words = HALF_TURN.sub("0", output_words, count=1), HALF_TURN.sub("0", check_words, count=1)
    if output_words != check_words or len(output_numbers) != len(check_numbers):
        return "Differ"
    absolute, relative = tolerances_for(name, output_line, rules)
    for output_text, check_text in zip(output_numbers, check_numbers):
        if not numbers_match(output_text, check_text, absolute, relative):
            return f"Values differ: {output_text} {check_text}"
    return ""


def read_lines(path: Path) -> list[str]:
    """Reads the lines of a file, without their line ends."""
    lines = path.read_bytes().decode(DATA_ENCODING).split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    return lines


def is_blank(line: str) -> bool:
    """Whether a line has nothing but whitespace."""
    return not line.strip(WHITESPACE)


def compare_lines(name: str, output_lines: Sequence[str], check_lines: Sequence[str], rules: MatchRules) -> str:
    """Compares the lines of two files. Returns how they first differ, or "" if they match.

    Blank lines are skipped on either side, so a blank line that is in only one file is not a difference.
    """
    out_index = check_index = 0
    while out_index < len(output_lines) or check_index < len(check_lines):
        output = output_lines[out_index].rstrip(WHITESPACE) if out_index < len(output_lines) else ""
        check = check_lines[check_index].rstrip(WHITESPACE) if check_index < len(check_lines) else ""
        for fixed in (False, True):
            if fixed:
                output, check = apply_fixes(name, output, rules.fixes), apply_fixes(name, check, rules.fixes)
            if output == check:
                out_index, check_index = out_index + 1, check_index + 1
                break
            if out_index < len(output_lines) and is_blank(output):
                out_index += 1
                break
            if check_index < len(check_lines) and is_blank(check):
                check_index += 1
                break
        else:
            message = compare_line(name, output, check, rules)
            if message:
                where = f"{out_index + 1}" + (f":{check_index + 1}" if check_index != out_index else "")
                return f"line {where}: {message}\n        {output}\n        {check}"
            out_index, check_index = out_index + 1, check_index + 1
    return ""


def list_files(directory: Path) -> list[str]:
    """Returns the names, relative to the directory, of all the files under it."""
    if not directory.is_dir():
        return []
    return sorted(path.relative_to(directory).as_posix() for path in directory.rglob("*") if path.is_file())


def compare_directories(
    output_dir: Path, check_dir: Path, output_names: Iterable[str], rules: MatchRules, match_prefix: str = ""
) -> Comparison:
    """Compares the named files of the output with the files of the check data.

    The patterns of the settings are matched against the names with match_prefix in front of them.
    """
    output_set = set(output_names)
    check_set = set(list_files(check_dir))
    matched: list[str] = []
    differing: list[tuple[str, str]] = []
    for name in sorted(output_set & check_set):
        message = compare_lines(match_prefix + name, read_lines(output_dir / name), read_lines(check_dir / name), rules)
        if message:
            differing.append((name, message))
        else:
            matched.append(name)
    return Comparison(
        tuple(matched), tuple(differing), tuple(sorted(check_set - output_set)), tuple(sorted(output_set - check_set))
    )


def format_report(comparison: Comparison, output_dir: Path, check_dir: Path) -> str:
    """Describes what is different about the output, giving the paths of the files to compare by hand."""
    lines: list[str] = []
    if comparison.differing:
        lines.append("Files that differ:")
        for name, message in comparison.differing:
            lines.append(f"    {output_dir / name}: {message}")
            lines.append(f"    check data: {check_dir / name}")
    if comparison.missing_from_output:
        lines.append("Files in the check data that the test did not write:")
        lines.extend(f"    {check_dir / name}" for name in comparison.missing_from_output)
    if comparison.not_in_check:
        lines.append("Files the test wrote that are not in the check data:")
        lines.extend(f"    {output_dir / name}" for name in comparison.not_in_check)
    failed = len(comparison.differing) + len(comparison.missing_from_output) + len(comparison.not_in_check)
    lines.append(
        f"Compared {len(comparison.matched) + failed} files: {len(comparison.matched)} matched, {failed} failed"
    )
    if not comparison.matched and not failed:
        lines.append("There were no files to compare")
    return "\n".join(lines)


def parse_arguments() -> argparse.Namespace:
    """Reads the command line."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", type=Path, required=True, help="the test.config with the match_ settings")
    parser.add_argument("--match-prefix", default="", help="put in front of the names the patterns are matched against")
    parser.add_argument("output_dir", type=Path, help="the directory with the output of the test")
    parser.add_argument("check_dir", type=Path, help="the directory with the check data")
    return parser.parse_args()


def main() -> int:
    """Compares every file under the output directory with the check data, returning a process exit code."""
    options = parse_arguments()
    try:
        rules = read_match_rules(read_settings(options.config))
    except SuiteError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2
    comparison = compare_directories(
        options.output_dir, options.check_dir, list_files(options.output_dir), rules, options.match_prefix
    )
    print(format_report(comparison, options.output_dir, options.check_dir))
    return 0 if comparison.passed else 1


if __name__ == "__main__":
    sys.exit(main())
