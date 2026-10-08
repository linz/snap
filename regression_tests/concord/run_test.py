#!/usr/bin/env python3
"""
Tests how concord reads its input, against recorded results.

The regression suite in this directory gives concord an input file and checks
the output file. This test covers what it cannot express: input piped through
standard input or redirected from a file, the keyboard and -a prompts, line
endings, a last line with no newline, separators, error recovery, and the forms
of the command line options.

Each case runs concord in a temporary directory and records the exit code,
standard output, standard error and the output file. The results are compared
with the ones recorded in stream_goldens/, which were recorded from the program
before its input code was changed to read a line at a time.

Usage:
    run_test.py                       compare the built concord with the recorded results
    run_test.py --ref OLD --new NEW   compare two programs with each other
    run_test.py --ref OLD --record    record the results of OLD as the new expected results
"""

from __future__ import annotations

import argparse
import difflib
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
COORDSYSDEF = HERE / "cstest" / "coordsys.def"
GOLDENS = HERE / "stream_goldens"
TIMEOUT_SECONDS = 10

# Default build output directory, per BUILD.md: Linux's build.py places a
# release build in build-release/; the Windows CMake preset places one in
# build/windows-release/ instead. Override with SNAP_BUILD_DIR if needed.
if sys.platform == "win32":
    _DEFAULT_BUILD_DIR = HERE.parent.parent / "build" / "windows-release" / "src"
    EXE_SUFFIX = ".exe"
else:
    _DEFAULT_BUILD_DIR = HERE.parent.parent / "build-release" / "src"
    EXE_SUFFIX = ""
BUILD_DIR = Path(os.environ.get("SNAP_BUILD_DIR", str(_DEFAULT_BUILD_DIR)))


@dataclass
class Case:
    """One run of concord."""

    name: str
    args: list[str]
    files: dict[str, bytes] = field(default_factory=dict)  # written to the working directory
    tail: list[str] = field(default_factory=list)  # file arguments after the options
    stdin: bytes = b""
    stdin_from_file: str | None = None  # name of a file to redirect into standard input instead of a pipe


@dataclass(frozen=True)
class Result:
    """What one run of concord wrote."""

    returncode: int | str
    stdout: bytes
    stderr: bytes
    out_file: bytes | None  # the contents of out.txt, if concord wrote it


def text(lines: list[str], eol: str = "\n", final: bool = True) -> bytes:
    """Joins lines with eol, and ends with eol if final is set."""
    data = eol.join(lines)
    return (data + (eol if final else "")).encode()


LINE_ENDINGS = [("lf", "\n", True), ("crlf", "\r\n", True), ("nofinal", "\n", False), ("crlf_nofinal", "\r\n", False)]

DEC = ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3"]
DMS = ["-iNZGD2000,NEH,H", "-oNZGD2000,NEH,D", "-P4"]
DMS_NE = ["-iNZGD2000,NE,H", "-oNZGD2000,NE,D", "-P4"]
DM = ["-iNZGD2000,NEH,M", "-oNZGD2000,NEH,D", "-P4"]
RAD = ["-iNZGD2000,NE,R", "-oNZTM,EN", "-P3"]

DEC_ID = [
    "A1 -45.25 168.75",
    "B2 -41.25 174.75 ! trailing note",
    "! a comment",
    "",
    "   C3\t -40.5   175.5  ",
    "LONGNAME1 -39 176",
    "D4 -38 177 extra tokens here",
]
DEC_NOID = ["-45.25 168.75", "-41.25 174.75 ! note", "! c", "", "  -40.5\t175.5  "]
DMS_AFTER = [
    "A1 45 17 15.9 S 168 43 26.4 E 100.5",
    "B2 41 16 40.8S 174 43 41.1E 20.1",
    "C3 38 17 2.3 s 181 15 35.0 w 18.4",
    "D4 40 0 0 S 175 0 0 E 0",
    "E5   45   17   15.9   S   168   43   26.4   E   1  ",
]
DMS_BEFORE = ["A1 S 45 17 15.9 E 168 43 26.4 100.5", "B2 s 41 16 40.8 e 174 43 41.1 20.1"]
DMS_NOHEM = ["A1 45 17 15.9 168 43 26.4 10", "B2 45 17 15.9 S 168 43 26.4 10"]
DM_AFTER = ["A1 45 17.265 S 168 43.44 E 10", "B2 41 16.5S 174 43.2E 20"]
RAD_LINES = ["A1 -0.78 2.94", "B2 -0.72 3.05"]
SEP_DEC = [
    "code,lat,lon",
    "A1,-45.25,168.75",
    "B2 , -41.25 , 174.75",
    "C3,,175",
    "D4,-40",
    "E5,-39,176,extra,more",
    "F6,-38,177,",
    "",
]
SEP_DMS = [
    "A1,45,15,00.,S,168,45,00.,E",
    "B2, 41 , 15 ,00.  S , 174 , 45 , 00. ,E",
    "C3,38,15,00.S ,181,15,00.0E",
    "D4,45 15 00 S,168 45 00 E",
    "E5,45,15,00.,S 168,45,00.,E",
]
BAD_DEC = ["A1 -45.25 168.75", "B2 abc 1", "C3 -45 168x", "D4 -45", "E5", "F6 -45.25 168.75 ! after errors"]
BAD_DMS = [
    "A1 45 17 15.9 S 168 43 26.4 E 1",
    "B2 45 61 15.9 S 168 43 26.4 E 1",
    "C3 45 17 61 S 168 43 26.4 E 1",
    "D4 45 17 S 168 43 26 E 1",
    "E5 xx 17 15.9 S 168 43 26.4 E 1",
    "F6 45 17 15.9 S 168 43 26.4 E 1 junk",
    "G7 45 17 15.9 X 168 43 26.4 E 1",
]
BAD_RANGE = ["A1 -95 168", "B2 -45 168", "C3 -45 800", "D4 -45.5 170.5"]
KEYBOARD = ["X -45 168", "Y -41 174 note", "", "Z -40 175"]
HEMISPHERE_FORMS = [
    "A1 45 17 15.9 S 168 43 26.4 E 1",
    "B2 45 17 15.9 S 168 43 26.4 E",
    "C3 45 17 15.9S 168 43 26.4E 1",
    "D4 S 45 17 15.9 E 168 43 26.4 1",
    "E5 45 17 15.9 168 43 26.4 1",
    "F6 45 17 15.9 SS 168 43 26.4 E 1",
    "G7 45 17 15.9 S  168 43 26.4  E  1",
    "H8 45 17 60 S 168 43 60 E 1",
    "I9 45 17 60.1 S 168 43 26 E 1",
    "J0 45 60 15 S 168 61 26 E 1",
    "K1 -45 17 15 S 168 43 26 E 1",
]

# Answers to the program parameter prompts: coordinate systems and orders, the
# angle format, the epoch, the precision and the length of a name, then the file names
ASK_ANSWERS = ["NZGD2000", "NEH", "H", "NZTM", "EN", "", "3", "6", "", ""]
ASK_COORDS = ["A1 45 17 15.9 S 168 43 26.4 E 100", "B2 41 16 40.8 S 174 43 41.1 E 20", ""]

# Command line forms of the coordinate systems, orders, precisions and name length
COMMAND_LINES = [
    ("colon_form", ["-iNZGD2000:NE:D", "-oNZTM:EN", "-P3", "-n6"]),
    ("separate_arg", ["-i", "NZGD2000,NE,D", "-o", "NZTM,EN", "-P3", "-n6"]),
    ("lower_case", ["-inzgd2000,ne,d", "-onztm,en", "-P3", "-n6"]),
    ("no_order", ["-iNZGD2000", "-oNZTM", "-P3", "-n6"]),
    ("leading_comma", ["-i,NZGD2000,NE,D", "-oNZTM,EN", "-P3", "-n6"]),
    ("double_comma", ["-iNZGD2000,,NE,,D", "-oNZTM,,EN", "-P3", "-n6"]),
    ("bad_code", ["-iNOSUCH,NE,D", "-oNZTM,EN", "-P3", "-n6"]),
    ("bad_order", ["-iNZGD2000,XX,D", "-oNZTM,EN", "-P3", "-n6"]),
    ("bad_angle", ["-iNZGD2000,NE,Q", "-oNZTM,EN", "-P3", "-n6"]),
    ("extra_data", ["-iNZGD2000,NE,D,junk", "-oNZTM,EN", "-P3", "-n6"]),
    ("extra_after_order", ["-iNZGD2000,NE,D", "-oNZTM,EN,junk", "-P3", "-n6"]),
    ("trailing_comma", ["-iNZGD2000,NE,D", "-oNZTM,EN,", "-P3", "-n6"]),
    ("trailing_commas", ["-iNZGD2000,NE,D", "-oNZTM,EN,,", "-P3", "-n6"]),
    ("angle_m", ["-iNZGD2000,NE,M", "-oNZTM,EN", "-P3", "-n6"]),
    ("angle_r", ["-iNZGD2000,NE,R", "-oNZGD2000,NE,R", "-P6", "-n6"]),
    ("precision_pair", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3:5", "-n6"]),
    ("precision_pair_comma", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3,5", "-n6"]),
    ("precision_trailing", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3:", "-n6"]),
    ("precision_extra", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3:5:7", "-n6"]),
    ("precision_bad", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-Pabc", "-n6"]),
    ("precision_range", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P25", "-n6"]),
    ("precision_plus", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P+3", "-n6"]),
    ("name_length_bad", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3", "-nabc"]),
    ("name_length_big", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3", "-n99"]),
    ("name_length_zero", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3", "-n0"]),
    ("epoch_bad", ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3", "-n6", "-ynotadate"]),
    ("missing_system", ["-oNZTM,EN", "-P3", "-n6"]),
]


def file_case(name: str, args: list[str], lines: list[str], eol: str = "\n", final: bool = True) -> Case:
    """A case that reads lines from an input file and writes an output file."""
    return Case(name, args, {"in.txt": text(lines, eol, final)}, ["in.txt", "out.txt"])


def pipe_case(name: str, args: list[str], lines: list[str], eol: str = "\n", final: bool = True) -> Case:
    """A case that reads lines from standard input, with - for the input file, and writes an output file."""
    return Case(name, args, stdin=text(lines, eol, final), tail=["-", "out.txt"])


def _file_cases() -> list[Case]:
    """Input files, with whitespace and with separators, and with each ending of the lines."""
    cases: list[Case] = []
    for ending, eol, final in LINE_ENDINGS:
        cases.append(file_case(f"dec_id_{ending}", DEC + ["-n6"], DEC_ID, eol, final))
        cases.append(file_case(f"dec_noid_{ending}", DEC, DEC_NOID, eol, final))
        cases.append(file_case(f"dms_after_{ending}", DMS + ["-n6"], DMS_AFTER, eol, final))
        cases.append(file_case(f"sep_dec_{ending}", DEC + ["-n6", "-s,"], SEP_DEC, eol, final))
        cases.append(file_case(f"bad_dec_{ending}", DEC + ["-n6"], BAD_DEC, eol, final))
        cases.append(pipe_case(f"pipe_bad_dec_{ending}", DEC + ["-n6"], BAD_DEC, eol, final))
    cases.append(file_case("dec_id_default_length", DEC + ["-n"], DEC_ID))
    cases.append(file_case("dec_id_short", DEC + ["-n2"], DEC_ID))
    cases.append(file_case("dms_before", DMS + ["-n6"], DMS_BEFORE))
    cases.append(file_case("dms_nohem", DMS + ["-n6"], DMS_NOHEM))
    cases.append(file_case("dm_after", DM + ["-n6"], DM_AFTER))
    cases.append(file_case("rad", RAD + ["-n6"], RAD_LINES))
    cases.append(file_case("sep_dms", DMS_NE + ["-n6", "-s,"], SEP_DMS))
    cases.append(file_case("sep_dms_three", DMS_NE + ["-n6", "-s,"], SEP_DMS[:3]))
    cases.append(file_case("sep_tab", DEC + ["-n6", "-s\t"], ["A1\t-45.25\t168.75", "B2\t\t175", "C3\t-40\t"]))
    cases.append(file_case("empty_file", DEC, [], final=False))
    cases.append(file_case("only_comments", DEC, ["! one", "", "! two"]))
    cases.append(file_case("single_line_nofinal", DEC + ["-n6"], ["A1 -45.25 168.75"], final=False))
    cases.append(file_case("blank_then_data", DEC + ["-n6"], ["", "", "A1 -45.25 168.75"]))
    cases.append(file_case("include_input", DEC + ["-n6", "-f"], DEC_ID[:2]))
    cases.append(file_case("bad_dec_skip", DEC + ["-n6", "-e"], BAD_DEC))
    cases.append(pipe_case("pipe_bad_dec_skip", DEC + ["-n6", "-e"], BAD_DEC))
    cases.append(pipe_case("pipe_dec_id", DEC + ["-n6"], DEC_ID))
    return cases


def _error_cases() -> list[Case]:
    """Input with errors in it, read with and without skipping the errors, from a file and from a pipe."""
    cases: list[Case] = []
    for name, lines, args in [
        ("bad_dms", BAD_DMS, DMS + ["-n6"]),
        ("bad_range", BAD_RANGE, ["-iNZGD2000,NE,D", "-oNZTM,EN", "-P3", "-n6"]),
        ("bad_sep", SEP_DEC, DEC + ["-n6", "-s,"]),
        ("bad_sep_dms", SEP_DMS, DMS_NE + ["-n6", "-s,"]),
    ]:
        cases.append(file_case(name, args, lines))
        cases.append(file_case(f"{name}_skip", args + ["-e"], lines))
        cases.append(pipe_case(f"pipe_{name}", args, lines))
        cases.append(pipe_case(f"pipe_{name}_skip", args + ["-e"], lines))
    return cases


def _keyboard_cases() -> list[Case]:
    """Coordinates typed at the keyboard, which is also what concord reads when given no files."""
    cases: list[Case] = []
    for ending, eol, final in LINE_ENDINGS:
        cases.append(Case(f"keyboard_{ending}", ["-k", "-n6"] + DEC, stdin=text(KEYBOARD, eol, final)))
    cases.append(Case("keyboard_no_k", DEC + ["-n6"], stdin=text(KEYBOARD)))
    cases.append(
        Case("keyboard_dms", ["-k", "-n6"] + DMS, stdin=text(["A1 45 17 15.9 S 168 43 26.4 E 5", "B2 xx", ""]))
    )
    cases.append(Case("keyboard_to_file", ["-k", "-n6"] + DEC, tail=["out.txt"], stdin=text(KEYBOARD)))
    cases.append(Case("keyboard_leading_blanks", ["-k"] + DEC, stdin=text(["   -45 168", "\t-41 174", ""])))
    cases.append(Case("keyboard_error", ["-k", "-n6"] + DEC, stdin=text(["A1 abc 1", "B2 -45 168", ""])))
    cases.append(Case("keyboard_error_skip", ["-k", "-n6", "-e"] + DEC, stdin=text(["A1 abc 1", "B2 -45 168", ""])))
    cases.append(Case("keyboard_eof", ["-k", "-n6"] + DEC, stdin=b""))
    return cases


def _ask_cases() -> list[Case]:
    """Answers typed to the prompts for the program parameters."""
    answers = ASK_ANSWERS
    coords = ASK_COORDS
    extra_tokens = ["NZGD2000 junk", "NEH extra", "H more", "NZTM x", "EN y", "2020.5 z", "3 q", "6 w", "", ""]
    bad_system = ["NOSUCH", "NZGD2000", "XX", "NEH", "H", "NZTM", "XX", "EN", "", "3", "6", "", ""]
    with_file = ["NZGD2000", "NE", "D", "NZTM", "EN", "", "3", "6"]
    return [
        Case("ask_basic", ["-a"], stdin=text(answers + coords)),
        Case("ask_list_first", ["-a"], stdin=text(["?"] + answers + coords)),
        Case("ask_bad_system", ["-a"], stdin=text(bad_system + coords)),
        Case("ask_extra_tokens", ["-a"], stdin=text(extra_tokens + coords)),
        Case(
            "ask_epoch_bad",
            ["-a"],
            stdin=text(["NZGD2000", "NEH", "H", "NZTM", "EN", "notadate", "2020", "3", "6", "", ""] + coords),
        ),
        Case(
            "ask_number_bad",
            ["-a"],
            stdin=text(["NZGD2000", "NEH", "H", "NZTM", "EN", "", "abc", "3", "xyz", "6", "", ""] + coords),
        ),
        Case(
            "ask_no_names",
            ["-a"],
            stdin=text(["NZGD2000", "NE", "D", "NZTM", "EN", "", "3", "0", "", "", "-45 168", ""]),
        ),
        Case(
            "ask_input_file",
            ["-a"],
            files={"in.txt": text(DEC_ID[:2])},
            stdin=text(with_file + ["in.txt", "", "", "y"]),
        ),
        Case(
            "ask_input_output_file",
            ["-a"],
            files={"in.txt": text(["A1,-45.25,168.75", "B2,-41.25,174.75"])},
            stdin=text(with_file + ["in.txt", ",", "out.txt", "y"]),
        ),
        Case("ask_missing_input_file", ["-a"], stdin=text(with_file + ["nosuch.txt", "", "", "y"])),
        Case("ask_eof_midway", ["-a"], stdin=text(["NZGD2000", "NEH"])),
        Case("ask_crlf", ["-a"], stdin=text(answers + coords, "\r\n")),
    ]


def _redirect_cases() -> list[Case]:
    """Standard input redirected from a file, which unlike a pipe can seek, so an error echoes the whole record."""
    cases: list[Case] = []
    for name, args, lines, final in [
        ("redirect_dec_id", DEC + ["-n6"], DEC_ID, True),
        ("redirect_bad_dec", DEC + ["-n6"], BAD_DEC, True),
        ("redirect_bad_dec_skip", DEC + ["-n6", "-e"], BAD_DEC, True),
        ("redirect_bad_dms", DMS + ["-n6"], BAD_DMS, True),
        ("redirect_bad_dec_nofinal", DEC + ["-n6"], BAD_DEC, False),
    ]:
        files = {"in.txt": text(lines, final=final)}
        cases.append(Case(name, args, files, ["-", "out.txt"], stdin_from_file="in.txt"))
    return cases


def _character_cases() -> list[Case]:
    """Characters that are not blanks to concord, a carriage return in the middle of a line, and angle forms."""
    return [
        file_case("vertical_tab", DEC + ["-n6"], ["A1\v-45.25 168.75", "B2 -41\f 174.75", "C3 -40.5 175.5"]),
        file_case("cr_mid_line", DEC + ["-n6"], ["A1 -45.25\r 168.75", "B2\r -41 174.75", "C3 -40.5 175.5"]),
        file_case("cr_only_line", DEC + ["-n6"], ["A1 -45.25 168.75", "\r", "B2 -41 174.75"]),
        file_case("plus_sign", DEC + ["-n6"], ["A1 +45.25 +168.75", "B2 -41 +174.75", "C3 +x 175"]),
        file_case("hemisphere_forms", DMS + ["-n6"], HEMISPHERE_FORMS),
        file_case(
            "dm_hemisphere",
            DM + ["-n6"],
            ["A1 45 17.5 S 168 43.5 E 1", "B2 45 17.5S 168 43.5E 1", "C3 45 61 S 168 43 E 1"],
        ),
    ]


def _command_line_cases() -> list[Case]:
    """The forms of the options that give coordinate systems, orders, precisions and name lengths."""
    return [file_case(f"cmd_{name}", args, ["A1 -45.25 168.75"]) for name, args in COMMAND_LINES]


def build_cases() -> list[Case]:
    """Every case, in the order they are run."""
    return (
        _file_cases()
        + _error_cases()
        + _keyboard_cases()
        + _ask_cases()
        + _redirect_cases()
        + _character_cases()
        + _command_line_cases()
    )


def normalise(data: bytes) -> bytes:
    """Hides the build date in the program banner, the only expected difference."""
    return re.sub(rb"version [^ ]+ dated [^)]*\)", b"version X dated X)", data)


def run(binary: Path, case: Case) -> Result:
    """Runs concord on a case in a new directory and returns what it wrote."""
    with tempfile.TemporaryDirectory() as work:
        for name, data in case.files.items():
            (Path(work) / name).write_bytes(data)
        env = dict(os.environ, COORDSYSDEF=str(COORDSYSDEF), EF_DISABLE_BANNER="1", LC_ALL="C")
        command = [str(binary)] + case.args + case.tail
        returncode: int | str
        try:
            if case.stdin_from_file:
                with open(Path(work) / case.stdin_from_file, "rb") as redirected:
                    done = subprocess.run(
                        command,
                        cwd=work,
                        env=env,
                        stdin=redirected,
                        capture_output=True,
                        timeout=TIMEOUT_SECONDS,
                        check=False,
                    )
            else:
                done = subprocess.run(
                    command,
                    cwd=work,
                    env=env,
                    input=case.stdin,
                    capture_output=True,
                    timeout=TIMEOUT_SECONDS,
                    check=False,
                )
            returncode, stdout, stderr = done.returncode, done.stdout, done.stderr
        except subprocess.TimeoutExpired as exc:
            returncode, stdout, stderr = "timeout", exc.stdout or b"", exc.stderr or b""
        out = Path(work) / "out.txt"
        return Result(
            returncode, normalise(stdout), normalise(stderr), normalise(out.read_bytes()) if out.exists() else None
        )


def save_result(folder: Path, result: Result) -> None:
    """Writes a result as one file per item, with no out.txt file if there was no output file."""
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "returncode").write_text(str(result.returncode))
    (folder / "stdout").write_bytes(result.stdout)
    (folder / "stderr").write_bytes(result.stderr)
    if result.out_file is not None:
        (folder / "out.txt").write_bytes(result.out_file)


def load_result(folder: Path) -> Result:
    """Reads a result written by save_result."""
    code = (folder / "returncode").read_text()
    out = folder / "out.txt"
    return Result(
        code if code == "timeout" else int(code),
        (folder / "stdout").read_bytes(),
        (folder / "stderr").read_bytes(),
        out.read_bytes() if out.exists() else None,
    )


def describe_difference(expected: Result, actual: Result) -> str:
    """Describes what differs between two results, with up to 40 lines of each difference in the text."""
    lines: list[str] = []
    if expected.returncode != actual.returncode:
        lines.append(f"exit code: expected {expected.returncode!r}, got {actual.returncode!r}\n")
    for name, was, now in [
        ("stdout", expected.stdout, actual.stdout),
        ("stderr", expected.stderr, actual.stderr),
        ("out.txt", expected.out_file, actual.out_file),
    ]:
        if was == now:
            continue
        if was is None or now is None:
            expected_text = "no file" if was is None else "a file"
            actual_text = "no file" if now is None else "a file"
            lines.append(f"{name}: expected {expected_text}, got {actual_text}\n")
            continue
        diff = difflib.unified_diff(
            was.decode(errors="replace").splitlines(True),
            now.decode(errors="replace").splitlines(True),
            f"expected {name}",
            f"actual {name}",
            n=1,
        )
        lines.extend(list(diff)[:40])
    return "".join(lines)


def absolute_path(value: str) -> Path:
    """A path from the command line, made absolute because each case runs in its own directory."""
    return Path(value).resolve()


def parse_arguments() -> argparse.Namespace:
    """Reads the command line."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--new", type=absolute_path, default=BUILD_DIR / ("concord" + EXE_SUFFIX), help="concord to test"
    )
    parser.add_argument(
        "--ref", type=absolute_path, help="reference concord, to compare with instead of the recorded results"
    )
    parser.add_argument("--goldens", type=absolute_path, default=GOLDENS, help="where the recorded results are")
    parser.add_argument("--record", action="store_true", help="record the results of --ref as the expected results")
    parser.add_argument("--only", help="run only cases whose name contains this text")
    parser.add_argument("--list", action="store_true", help="list the cases and stop")
    parser.add_argument("-v", "--verbose", action="store_true", help="show every case, and the differences")
    return parser.parse_args()


def main() -> int:
    """Runs the cases, returning a process exit code."""
    options = parse_arguments()
    cases = [case for case in build_cases() if not options.only or options.only in case.name]
    if options.list:
        print("\n".join(case.name for case in cases))
        return 0
    if options.record:
        if not options.ref:
            print("--record needs --ref", file=sys.stderr)
            return 2
        for case in cases:
            save_result(options.goldens / case.name, run(options.ref, case))
        print(f"recorded {len(cases)} cases in {options.goldens}")
        return 0

    failures = 0
    for case in cases:
        expected = run(options.ref, case) if options.ref else load_result(options.goldens / case.name)
        actual = run(options.new, case)
        if expected == actual:
            if options.verbose:
                print(f"PASS {case.name}")
            continue
        failures += 1
        print(f"FAIL {case.name}")
        if options.verbose:
            print(describe_difference(expected, actual))
    if failures:
        print(f"concord input: FAIL ({failures} of {len(cases)} cases differ)", file=sys.stderr)
        return 1
    print("concord input: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
