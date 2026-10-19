#!/usr/bin/env python3
"""
Runs one regression test of a suite, in a work directory, as described by the suite's test.config.

A suite is a directory with a test.config that names the program to run, where the test definitions
(test_dir) and the expected output (check_dir) are, and how to find the files each test needs. A test
is run by copying the files it names into the work directory and running the configured commands
there. The files they leave behind (text files that were not copied in or discarded) are the output
to check, and are compared with the files in the suite's check_dir for the test, by compare_output.py.

The exit code is 0 if the output matches, 1 if it does not, and 2 if the suite could not be run.

A test runs in OUTPUT_DIR/<path of the suite from the repository root>/<test>/output. If the test fails, or
--keep is given, that is left behind, with links in ../test to the test's definition and check data, and its
path is printed. Otherwise it is removed. Each run starts by removing the test's own directory.

To run a suite more than once, with parameters put in front of those of every test (--extra-parameters), give
each run a --suite-name, which is used in place of the suite's own folder name in that path.

Usage:
    run_regression_test.py --config snap/test.config --list
    run_regression_test.py --config snap/test.config --check-unused
    run_regression_test.py --config snap/test.config --test test1 [--keep] [--build-dir DIR] [--output-dir DIR]
                           [--extra-parameters=PARAMETERS --suite-name NAME]
"""

from __future__ import annotations

import argparse
import contextlib
import os
import re
import shlex
import shutil
import subprocess
import sys
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import IO

from compare_output import MatchRules, compare_directories, format_report, read_match_rules
from suite_config import FILE_ENCODING, Setting, SuiteError, last_value, pairs_of, read_settings, split_pair, values_of

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parent

# Default build output directory, per BUILD.md: Linux's build.py places a
# release build in build-release/; the Windows CMake preset places one in
# build/windows-release/ instead.
if sys.platform == "win32":
    DEFAULT_BUILD_DIR = HERE.parent / "build" / "windows-release" / "src"
else:
    DEFAULT_BUILD_DIR = HERE.parent / "build-release" / "src"

# What a program's file name can end with. A program built for Windows ends with .exe, wherever it is run from.
EXE_SUFFIXES = ("", ".exe")

# Where the output of the tests that fail, or are asked to be kept, is left. The build's test target empties all
# of it, the setup test of a suite (see cmake/SnapTests.cmake) empties the suite's directory, and each test
# empties its own directory when it runs.
DEFAULT_OUTPUT_DIR = DEFAULT_BUILD_DIR.parent / "test_output"

PLACEHOLDER = re.compile(r"\{(\w+(?::\w+)?)\}")
SIMPLE_PLACEHOLDER = re.compile(r"\{\w+\}")
DEFAULT_TEST_PATTERN = re.compile(r"(\w+).test")
DEFAULT_SETTING_PATTERN = re.compile(r"^\s*(\w+):\s+(\S.*?)\s*$")
DEFAULT_FILE_PATTERN = re.compile(r"^\s*file:\s+(\S+)(?:\s+(\S+))?(?:\s+(save))?\s*$", re.IGNORECASE)
DEFAULT_COMMAND = '{debug}"{program}" {parameters}'

# A command can be defined in terms of variables that are defined in terms of others, so placeholders
# are replaced repeatedly, up to this many times
MAX_EXPANSIONS = 9

# Where the standard error of a command goes when it is to be appended to its standard output afterwards
ERROR_TEMP_NAME = "error_output.tmp"

# How much of the start of a file is looked at to decide whether it is text
TEXT_SAMPLE_SIZE = 512
TEXT_CONTROL_CHARACTERS = b"\b\t\n\r\f\x1b"


@dataclass(frozen=True)
class FileRules:
    """How the tests of a suite, and the files each one needs, are found."""

    test_dir: str
    check_dir: str
    test_pattern: re.Pattern[str]
    setting_pattern: re.Pattern[str]
    files: tuple[tuple[re.Pattern[str], str], ...]
    ignores: tuple[re.Pattern[str], ...]


@dataclass(frozen=True)
class RunRules:
    """How a test of a suite is run."""

    programs: tuple[str, ...]
    commands: tuple[str, ...]
    discards: tuple[str, ...]
    output: str
    error_output: str
    variables: Mapping[str, str]
    environment: Mapping[str, str]


@dataclass(frozen=True)
class SuiteConfig:
    """The parts of a test.config that say how to find and run the tests."""

    config_dir: Path
    rules: FileRules
    run: RunRules
    matching: MatchRules


@dataclass(frozen=True)
class TestFile:
    """A file a test needs, copied from the test directory into the work directory."""

    source: str
    target: str
    is_input: bool  # an input is not part of the output, unless the test says to save it


@dataclass(frozen=True)
class TestSetup:
    """What one test's definition file says about the files, parameters and variables of the test."""

    files: tuple[TestFile, ...]
    variables: Mapping[str, str]
    parameters: str


@dataclass(frozen=True)
class TestRun:
    """What running a test left behind."""

    work_dir: Path
    commands: tuple[str, ...]
    outputs: tuple[str, ...]  # the names, relative to the work directory, of the files to check


def last_pattern(settings: Sequence[Setting], item: str, default: re.Pattern[str]) -> re.Pattern[str]:
    """Returns an item's last setting as a pattern that must match all of a name, or the default if it has none."""
    values = values_of(settings, item)
    return re.compile(f"^{values[-1]}$") if values else default


def file_rule(value: str) -> tuple[re.Pattern[str], str]:
    """Splits a filere setting into the pattern that finds a file's name and the extension to try adding to it."""
    parts = value.split(None, 1)
    if not parts:
        raise SuiteError("filere needs a pattern")
    return re.compile(parts[0]), parts[1] if len(parts) > 1 else ""


def read_config(path: Path) -> SuiteConfig:
    """Reads the settings of a test.config that say how to find and run the tests."""
    config_dir = path.resolve().parent
    settings = read_settings(path)
    programs = values_of(settings, "program")
    if not programs:
        raise SuiteError(f"{path} does not define a program")
    try:
        rules = FileRules(
            test_dir=last_value(settings, "test_dir", "test"),
            check_dir=last_value(settings, "check_dir", "check"),
            test_pattern=last_pattern(settings, "testre", DEFAULT_TEST_PATTERN),
            setting_pattern=last_pattern(settings, "configre", DEFAULT_SETTING_PATTERN),
            files=tuple(file_rule(value) for value in values_of(settings, "filere")) or ((DEFAULT_FILE_PATTERN, ""),),
            ignores=tuple(re.compile(value) for value in values_of(settings, "ignorere")),
        )
    except re.error as error:
        raise SuiteError(f"{path}: invalid regular expression: {error}") from error
    run = RunRules(
        programs=programs,
        commands=values_of(settings, "command") or (DEFAULT_COMMAND,),
        discards=values_of(settings, "discard"),
        output=last_value(settings, "output", ""),
        error_output=last_value(settings, "error_output", ""),
        variables=pairs_of(settings, "set"),
        environment=pairs_of(settings, "env"),
    )
    return SuiteConfig(config_dir, rules, run, read_match_rules(settings))


def find_tests(config: SuiteConfig) -> dict[str, str]:
    """Returns the name of each test of the suite, with the name of the file that defines it."""
    test_dir = config.config_dir / config.rules.test_dir
    if not test_dir.is_dir():
        raise SuiteError(f"invalid test directory {test_dir}")
    tests: dict[str, str] = {}
    for entry in sorted(test_dir.iterdir()):
        match = config.rules.test_pattern.search(entry.name)
        if entry.is_file() and match:
            tests[match.group(1)] = entry.name
    return dict(sorted(tests.items()))


def group(match: re.Match[str], number: int) -> str:
    """Returns a group of a match, or an empty string if the pattern has no such group or it did not take part."""
    if number > match.re.groups:
        return ""
    return match.group(number) or ""


def test_file(test_dir: Path, name: str, match: re.Match[str], extension: str) -> TestFile:
    """Returns the file that a line of a test definition names, where the pattern that matched it gives the name."""
    source = group(match, 1).replace("{test}", name)
    target = group(match, 2).replace("{test}", name)
    if (test_dir / (source + extension)).is_file() and not (test_dir / source).is_file():
        source += extension
    return TestFile(source, target or source, group(match, 3) == "")


def read_test_setup(config: SuiteConfig, name: str, filename: str) -> TestSetup:
    """Reads a test's definition file for the files it needs, and any parameters and variables it sets."""
    test_dir = config.config_dir / config.rules.test_dir
    files = [TestFile(filename, filename, True)]
    variables = dict(config.run.variables)
    parameters = ""
    ignored: set[str] = set()
    for line in (test_dir / filename).read_text(**FILE_ENCODING).splitlines(keepends=True):
        for pattern in config.rules.ignores:
            match = pattern.search(line)
            if match:
                ignored.add(group(match, 1))
        for pattern, extension in config.rules.files:
            match = pattern.search(line)
            if match and group(match, 1).replace("{test}", name) not in ignored:
                files.append(test_file(test_dir, name, match, extension))
        match = config.rules.setting_pattern.search(line)
        if match and match.group(1) == "parameters":
            parameters += " " + group(match, 2)
        elif match and match.group(1) == "set":
            variable, setting = split_pair(group(match, 2), "set")
            variables[variable] = setting
    return TestSetup(tuple(files), variables, parameters)


def find_program(build_dir: Path, program: str) -> Path:
    """Returns the path of a built program, which is in the build directory with or without the suffix of Windows."""
    for suffix in EXE_SUFFIXES:
        path = (build_dir / (program + suffix)).resolve()
        if path.is_file() and os.access(path, os.X_OK):
            return path
    raise SuiteError(f"program {build_dir / program} is missing")


def resolve_programs(config: SuiteConfig, build_dir: Path) -> dict[str, str]:
    """Returns the path of each program the suite tests, by the names that commands refer to it by."""
    programs: dict[str, str] = {}
    for program in config.run.programs:
        path = find_program(build_dir, program)
        programs[f"program:{program}"] = path.as_posix()
        programs["program"] = path.as_posix()
    return programs


def expand(text: str, lookups: Sequence[Mapping[str, str]]) -> str:
    """Replaces the {name} placeholders in text, with the first non-empty value from the lookups."""

    def value(match: re.Match[str]) -> str:
        return next((found[match.group(1)] for found in lookups if found.get(match.group(1))), "")

    for _ in range(MAX_EXPANSIONS):
        if SIMPLE_PLACEHOLDER.search(text) is None:
            break
        text = PLACEHOLDER.sub(value, text)
    return text


def run_command(argv: list[str], run: RunRules, work_dir: Path, names: tuple[str, str]) -> None:
    """Runs one command in the work directory, with its standard output and error going to the named files.

    A name of "" means to discard it, and standard error going to the same file as standard output is merged
    with it. Standard error that is to follow the standard output instead is put in a temporary file here,
    and appended to the output file afterwards.
    """
    out_name, err_name = names
    with contextlib.ExitStack() as stack:
        stdout: IO[bytes] | int = subprocess.DEVNULL
        stderr: IO[bytes] | int = subprocess.DEVNULL
        if out_name:
            stdout = stack.enter_context((work_dir / out_name).open("wb"))
        if err_name == out_name:
            stderr = subprocess.STDOUT
        elif err_name:
            stderr = stack.enter_context((work_dir / err_name).open("wb"))
        environment = dict(os.environ, **run.environment)
        subprocess.run(
            argv, cwd=work_dir, env=environment, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr, check=False
        )
    if err_name == ERROR_TEMP_NAME:
        if out_name:
            with (work_dir / out_name).open("ab") as output:
                output.write((work_dir / err_name).read_bytes())
        (work_dir / err_name).unlink()


def redirect_names(run: RunRules, lookups: Sequence[Mapping[str, str]]) -> tuple[str, str]:
    """Returns the names of the files the standard output and error of a command go to, "" for neither."""
    err_name = ERROR_TEMP_NAME if run.error_output == "output" else run.error_output
    out_name = "" if run.output in ("", "null") else expand(run.output, lookups)
    err_name = "" if err_name in ("", "null") else expand(err_name, lookups)
    return out_name, err_name


def run_commands(config: SuiteConfig, lookups: Sequence[Mapping[str, str]], work_dir: Path) -> tuple[str, ...]:
    """Runs the suite's commands in the work directory. Returns the command lines that were run."""
    names = redirect_names(config.run, lookups)
    command_lines: list[str] = []
    for template in config.run.commands:
        command_line = expand(template, lookups)
        if not command_line:
            continue
        command_lines.append(command_line)
        run_command(shlex.split(command_line), config.run, work_dir, names)
    return tuple(command_lines)


def is_text(path: Path) -> bool:
    """Whether a file looks like text: its start has no null bytes, and is valid UTF-8 or has few odd characters."""
    with path.open("rb") as stream:
        sample = stream.read(TEXT_SAMPLE_SIZE)
    if not sample:
        return True
    if b"\0" in sample:
        return False
    with contextlib.suppress(UnicodeDecodeError):
        sample.decode("utf-8")
        return True
    odd = sum(1 for byte in sample if byte > 127 or (byte < 32 and byte not in TEXT_CONTROL_CHARACTERS))
    return odd * 3 <= len(sample)


def find_outputs(
    config: SuiteConfig, setup: TestSetup, lookups: Sequence[Mapping[str, str]], work_dir: Path
) -> tuple[str, ...]:
    """Returns the names of the files in the work directory that are the test's output to check."""
    inputs = {Path(item.target).as_posix() for item in setup.files if item.is_input}
    discards = {Path(expand(name, lookups)).as_posix() for name in config.run.discards}
    outputs: list[str] = []
    for path in sorted(work_dir.rglob("*")):
        name = path.relative_to(work_dir).as_posix()
        if path.is_file() and name not in inputs and name not in discards and is_text(path):
            outputs.append(name)
    return tuple(outputs)


def run_test(
    config: SuiteConfig, name: str, filename: str, build_dir: Path, work_dir: Path, extra_parameters: str = ""
) -> TestRun:
    """Runs a test in the work directory, which is emptied first. Returns what the test left behind.

    The extra parameters (for example -k) go in front of the parameters that the test's definition gives.
    """
    setup = read_test_setup(config, name, filename)
    test_dir = config.config_dir / config.rules.test_dir
    replacements = {
        "test": name,
        "testfile": filename,
        "configdir": config.config_dir.as_posix(),
        "debug": "",
        "parameters": extra_parameters + setup.parameters,
    }
    lookups = [setup.variables, replacements, resolve_programs(config, build_dir)]

    missing = [item.source for item in setup.files if not (test_dir / item.source).exists()]
    if missing:
        raise SuiteError("cannot run the test, its files are missing: " + ", ".join(missing))
    if work_dir.exists():
        shutil.rmtree(work_dir)
    work_dir.mkdir(parents=True)
    for item in setup.files:
        target = work_dir / item.target
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(test_dir / item.source, target)

    command_lines = run_commands(config, lookups, work_dir)
    return TestRun(work_dir, command_lines, find_outputs(config, setup, lookups, work_dir))


def test_output_dir(config: SuiteConfig, name: str, output_root: Path, suite_name: str | None = None) -> Path:
    """Returns the directory for a test, which is <output_root>/<suite>/<name>.

    The <suite> components are the suite's path relative to the repository root.
    For {suite: snapspec} it is regression_tests/snapspec. Then for {name: snapspec3}
    the directory is <output_root>/regression_tests/snapspec/snapspec3.

    If a suite_name is provided, for example snapspec_k, it takes the place of the last folder of suite,
    so the directory would become <output_root>/regression_tests/snapspec_k/snapspec3. This is useful for a
    suite that runs more than once with different parameters, so that the runs do not leave their output in
    the same place. The output root and the test's name do not change.
    """
    try:
        suite = config.config_dir.relative_to(REPO_ROOT)
    except ValueError:
        suite = Path(config.config_dir.name)
    if suite_name:
        suite = suite.with_name(suite_name)
    return output_root / suite / name


def link_test(test_output: Path, definition: Path, check_dir: Path) -> None:
    """Puts links in a test's output directory to the files that define the test, so they can be found from it.

    On Windows, where making a link takes a privilege that is often not held, the paths are written to a file instead.
    """
    links = test_output / "test"
    links.mkdir()
    targets = {"definition": definition, "check": check_dir}
    try:
        for name, target in targets.items():
            (links / name).symlink_to(target, target_is_directory=target.is_dir())
    except OSError:
        if sys.platform != "win32":
            raise
        listing = "".join(f"{name}: {target}\n" for name, target in targets.items())
        (links / "locations.txt").write_text(listing, encoding="utf-8")


def check_unused(config: SuiteConfig, tests: Mapping[str, str]) -> int:
    """Reports the check data that no test of the suite uses. Returns 1 if there is any."""
    check_dir = config.config_dir / config.rules.check_dir
    unused = sorted(path for path in check_dir.iterdir() if path.name not in tests) if check_dir.is_dir() else []
    for path in unused:
        print(f"check data that no test uses: {path}")
    return 1 if unused else 0


def parse_arguments() -> argparse.Namespace:
    """Reads the command line."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", type=Path, required=True, help="the test.config of the suite")
    parser.add_argument("--list", action="store_true", help="list the names of the tests of the suite and stop")
    parser.add_argument(
        "--check-unused", action="store_true", help="check that all the check data belongs to a test, and stop"
    )
    parser.add_argument("--test", help="the name of the test to run")
    parser.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD_DIR, help="where the built programs are")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help="where the output of the tests that fail, or are kept, is left, under the path of the suite",
    )
    parser.add_argument("--keep", action="store_true", help="keep the output of the test even if it passes")
    parser.add_argument(
        "--extra-parameters",
        default="",
        help="parameters to put in front of those of every test, for example --extra-parameters=-k. The = form "
        "works for any value. Without the =, a value that starts with - is taken to be an option, unless it "
        'contains a space, as in --extra-parameters "-k -x"',
    )
    parser.add_argument(
        "--suite-name",
        help="the folder name for the suite in the output path, instead of the suite's own (snapspec_k instead of "
        "snapspec, for example), for a suite that is run more than once",
    )
    options = parser.parse_args()
    if not (options.list or options.check_unused or options.test):
        parser.error("--test is needed unless --list or --check-unused is given")
    return options


def main() -> int:
    """Runs the test, returning a process exit code."""
    options = parse_arguments()
    try:
        config = read_config(options.config)
        tests = find_tests(config)
        if options.list:
            print("\n".join(tests))
            return 0
        if options.check_unused:
            return check_unused(config, tests)
        if options.test not in tests:
            raise SuiteError(f"{options.test} is not a test of {options.config}")
        test_output = test_output_dir(config, options.test, options.output_dir.resolve(), options.suite_name)
        if test_output.exists():
            shutil.rmtree(test_output)
        result = run_test(
            config,
            options.test,
            tests[options.test],
            options.build_dir,
            test_output / "output",
            options.extra_parameters,
        )
    except SuiteError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2
    definition = config.config_dir / config.rules.test_dir / tests[options.test]
    check_dir = config.config_dir / config.rules.check_dir / options.test
    comparison = compare_directories(result.work_dir, check_dir, result.outputs, config.matching, f"{options.test}/")
    if comparison.passed and not options.keep:
        shutil.rmtree(test_output)
        print(f"PASS {options.test}")
        return 0
    link_test(test_output, definition, check_dir)
    if comparison.passed:
        print(f"PASS {options.test}: output kept in {result.work_dir}")
        return 0
    for command_line in result.commands:
        print(f"Ran in {result.work_dir}: {command_line}")
    print(format_report(comparison, result.work_dir, check_dir))
    print(f"FAIL {options.test}: output kept in {result.work_dir}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
