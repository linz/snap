"""
Reads the settings of a suite's test.config, which the scripts that run and check the suite's tests share.

Each line of a test.config is a setting, a name then a colon then a value, such as "program: snap". Lines
that are blank, or start with #, are ignored. {configdir} in a value is replaced by the directory of the
test.config. The same setting can be given more than once, and then it is up to whatever reads it whether
the last one wins, or they are all used.

These are the settings, and the match_ ones, which say how to compare output, are read by compare_output.py:

    test_dir, check_dir, out_dir     the directories of the test definitions and check data
    testre, configre                 patterns for the names of the tests, and for settings in a test definition
    filere, ignorere                 patterns for the files that a test definition names, and that it ignores
    program, command, discard        what to run, and the files to discard from the output
    output, error_output             the files for the standard output and error of the commands
    env, set                         environment variables of the commands, and variables to use in them
    match_*                          see compare_output.py
"""

from __future__ import annotations

import re
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

# How text files that are read as text, such as a test.config and test definitions, are decoded. Errors are
# kept as they are, so that a file name that is not valid UTF-8 survives being read.
FILE_ENCODING = {"encoding": "utf-8", "errors": "surrogateescape"}

SETTING_LINE = re.compile(r"^\s*(\w+):\s+(\S.*?)?\s*$")
IGNORED_LINE = re.compile(r"^\s*(#|$)")

MATCH_PREFIX = "match_"
SETTING_ITEMS = frozenset(
    {
        "test_dir",
        "check_dir",
        "out_dir",
        "testre",
        "configre",
        "filere",
        "ignorere",
        "program",
        "command",
        "discard",
        "output",
        "error_output",
        "env",
        "set",
    }
)


class SuiteError(Exception):
    """A problem with a suite's configuration or files, as opposed to a test failing."""


@dataclass(frozen=True)
class Setting:
    """One setting of a test.config."""

    item: str  # the name, in lower case
    value: str  # empty if there is none
    location: str  # the file and line number, for messages


def split_pair(value: str, item: str) -> tuple[str, str]:
    """Splits the value of an env or set setting into a name and the rest."""
    parts = value.split(None, 1)
    if not parts:
        raise SuiteError(f"{item} needs a name")
    return parts[0], parts[1] if len(parts) > 1 else ""


def read_settings(path: Path) -> list[Setting]:
    """Reads the settings of a test.config, in the order they are given."""
    config_dir = path.resolve().parent.as_posix()
    settings: list[Setting] = []
    for number, line in enumerate(path.read_text(**FILE_ENCODING).splitlines(), start=1):
        if IGNORED_LINE.match(line):
            continue
        match = SETTING_LINE.match(line)
        if match is None:
            raise SuiteError(f"{path}:{number}: invalid line: {line}")
        item = match.group(1).lower()
        if item not in SETTING_ITEMS and not item.startswith(MATCH_PREFIX):
            raise SuiteError(f"{path}:{number}: invalid configuration item {item}")
        settings.append(Setting(item, (match.group(2) or "").replace("{configdir}", config_dir), f"{path}:{number}"))
    return settings


def settings_of(settings: Sequence[Setting], *items: str) -> tuple[Setting, ...]:
    """Returns every setting of any of the items, in the order they are given."""
    return tuple(setting for setting in settings if setting.item in items)


def values_of(settings: Sequence[Setting], item: str) -> tuple[str, ...]:
    """Returns the values of every setting of an item, in the order they are given."""
    return tuple(setting.value for setting in settings_of(settings, item))


def last_value(settings: Sequence[Setting], item: str, default: str) -> str:
    """Returns the value of an item's last setting, or the default if it has none."""
    values = values_of(settings, item)
    return values[-1] if values else default


def pairs_of(settings: Sequence[Setting], item: str) -> dict[str, str]:
    """Returns the names and values given by every setting of an item, where a later one replaces an earlier."""
    return dict(split_pair(value, item) for value in values_of(settings, item))
