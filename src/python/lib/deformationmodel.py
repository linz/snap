"""Parses, validates, builds, and writes a LINZ deformation model (.ldm) binary from its ASCII
definition. See src/help/help/coordsys/linzdeffile.html for the user-facing format description. A
model is one or more named sequences, each a list of components - a single grid or triangulated
deformation/velocity model, with its own reference date and spatial extent. Ported from
makelinzdefmodel.pl, which supports 3 format versions:

- v1 (LINZDEF1L/1B): each component has a BEFORE_REF_DATE/AFTER_REF_DATE method
  (fixed/zero/interpolate). Two components with different reference dates aren't a choice between
  them: every spatially-overlapping component in the sequence contributes, each scaled by its own
  time-dependent factor derived from its method (roughly, "fixed" keeps full weight, "zero" drops
  to no weight, "interpolate" ramps between - see dbl4_utl_lnzdef.cpp's
  load_component/calc_seq_def for the exact runtime calculation), and the results are summed. This
  script only packages each component's reference date and method codes; that factor calculation
  happens in the C++ reader at evaluation time, not here - this build step does no date arithmetic
  at all for v1.
- v2 (LINZDEF2L/2B): the same "every spatially-overlapping component contributes, scaled by a
  time-dependent factor" model as v1, but the factor is given directly as a piecewise-linear
  TIME_MODEL rather than derived from a before/after method code. If the sequence is
  NESTED_SEQUENCE, only the first spatially-matching component applies instead of summing. Unlike
  v1, a TIME_MODEL velocity *is* converted here, at build time, into its equivalent
  piecewise-linear displacement sequence, zeroed at the reference epoch (see WriteModelBinaryV23) -
  this script's own work, not deferred to the reader.
- v3 (LINZDEF3L/3B): the same as v2, but the model header can repeat a version number/date/
  description (VERSION records), and sequences additionally carry a version range.
"""

from __future__ import annotations

import re
import tempfile
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import BinaryIO

from lib.dateutil import date_to_year, parse_date
from lib.endian_io import EndianCodec
from lib.gridfile import GridFile, GridFormat, WriteOptions
from lib.trigfile import TrigFile, build_trig_file

_METHOD_CODES = {"zero": 0, "fixed": 1, "interpolate": 2}

_BLANK_OR_COMMENT_LINE = re.compile(r"^\s*($|#)")
_RECORD_KEYWORD = re.compile(r"^(DEFORMATION_(MODEL|SEQUENCE|COMPONENT)|VERSION)$")
_END_DESCRIPTION_LINE = re.compile(r"^\s*end_description\s*$", re.IGNORECASE)
_VERSION_NUMBER = re.compile(r"^20\d\d[01]\d[0123]\d$")

# Each format version's model/sequence/component parameter schemas share a common base, with a
# few fields added or swapped per version - built here as base plus delta rather than 3 fully
# independent copies of the shared fields.
_MODEL_BASE = {
    "FORMAT": r"(LINZDEF[123][BL])",
    "START_DATE": "date",
    "END_DATE": "date",
    "COORDSYS": r"\w+",
    "GEOGRAPHICAL": "(yes|no)?",
}

_SEQUENCE_BASE = {
    "DIMENSION": "[123]",
    "START_DATE": "date",
    "END_DATE": "date",
    "ZERO_BEYOND_RANGE": "(yes|no)?",
    "DESCRIPTION": ".*",
}

_COMPONENT_BASE = {
    "MODEL_TYPE": "(trig|grid)",
    "REF_DATE": "date",
    "DESCRIPTION": ".*",
}

_MODEL_PARAMS_V1 = _MODEL_BASE | {"VERSION_NUMBER": r"\d+(\.\d+)?", "VERSION_DATE": "date", "DESCRIPTION": ".*"}
_SEQUENCE_PARAMS_V1 = _SEQUENCE_BASE | {"DATA_TYPE": "(velocity|deformation)?"}
_COMPONENT_PARAMS_V1 = _COMPONENT_BASE | {
    "BEFORE_REF_DATE": "(fixed|zero|interpolate)?",
    "AFTER_REF_DATE": "(fixed|zero|interpolate)?",
}

_MODEL_PARAMS_V2 = _MODEL_PARAMS_V1
_SEQUENCE_PARAMS_V2 = _SEQUENCE_BASE | {"NESTED_SEQUENCE": "(yes|no)?"}
_COMPONENT_PARAMS_V2 = _COMPONENT_BASE | {
    "TIME_MODEL": r"(PIECEWISE_LINEAR\s+float(\s+date\s+float)*|VELOCITY(\s+float(\s+date\s+float)*)?)",
}

_MODEL_PARAMS_V3 = _MODEL_BASE
_SEQUENCE_PARAMS_V3 = _SEQUENCE_PARAMS_V2 | {
    "VERSION_START": r"(20\d\d[01]\d[0123]\d)",
    "VERSION_END": r"(20\d\d[01]\d[0123]\d|0|99999999)",
}
_VERSION_PARAMS_V3 = {"VERSION_DATE": "date", "DESCRIPTION": ".*"}


@dataclass(frozen=True)
class _VersionSchema:
    """The set of valid parameter names (and their value patterns) for one LINZDEF format
    version's model, VERSION record (version 3 only), sequence, and component records."""

    model: dict[str, str]
    version: dict[str, str] | None
    sequence: dict[str, str]
    component: dict[str, str]


_SCHEMA_V1 = _VersionSchema(_MODEL_PARAMS_V1, None, _SEQUENCE_PARAMS_V1, _COMPONENT_PARAMS_V1)
_SCHEMA_V2 = _VersionSchema(_MODEL_PARAMS_V2, None, _SEQUENCE_PARAMS_V2, _COMPONENT_PARAMS_V2)
_SCHEMA_V3 = _VersionSchema(_MODEL_PARAMS_V3, _VERSION_PARAMS_V3, _SEQUENCE_PARAMS_V3, _COMPONENT_PARAMS_V2)


class LinzDefFormat(Enum):
    """The 6 on-disk LINZDEF format variants: each carries its own major version number, the
    model/sequence/component schema that version uses, its byte order, and its file signature."""

    LINZDEF1L = (1, _SCHEMA_V1, False, b"LINZ deformation model v1.0L\r\n\x1a")
    LINZDEF1B = (1, _SCHEMA_V1, True, b"LINZ deformation model v1.0B\r\n\x1a")
    LINZDEF2L = (2, _SCHEMA_V2, False, b"LINZ deformation model v2.0L\r\n\x1a")
    LINZDEF2B = (2, _SCHEMA_V2, True, b"LINZ deformation model v2.0B\r\n\x1a")
    LINZDEF3L = (3, _SCHEMA_V3, False, b"LINZ deformation model v3.0L\r\n\x1a")
    LINZDEF3B = (3, _SCHEMA_V3, True, b"LINZ deformation model v3.0B\r\n\x1a")

    def __init__(self, version: int, schema: _VersionSchema, big_endian: bool, signature: bytes) -> None:
        """Attaches this format's version number, schema, byte order, and signature to the member."""
        self.version = version
        self.schema = schema
        self.big_endian = big_endian
        self.signature = signature


def parse_linzdef_format(name: str) -> LinzDefFormat:
    """Looks up a LINZDEF format by name (case-insensitive), raising `ValueError` if unrecognized."""
    try:
        return LinzDefFormat[name.upper()]
    except KeyError as error:
        raise ValueError(f"Invalid model format {name}") from error


Range = tuple[float, float, float, float]  # (ymin, ymax, xmin, xmax) - the binary layout's own order


@dataclass
class SourceFile:
    """A component's binary sub-file: its path, byte size, and the metadata read back off it once
    built (coordinate system, dimension, spatial range)."""

    name: Path
    original_name: Path
    size: int
    coordsys: str
    dimension: int
    range: Range
    location: int = 0  # byte offset within the .ldm file, filled in while writing


def _build_grid_source(original_path: Path, big_endian: bool, workdir: Path) -> SourceFile:
    """Builds one grid component: an already-binary source is used verbatim; an ASCII source is
    built into `workdir` in the model's own endianness. GridFile's own signature detection decides
    which, rather than a heuristic - its constructor already tells ASCII and binary apart
    exactly."""
    grid = GridFile(original_path)
    if grid.format is GridFormat.ASCII:
        binary_path = workdir / f"{original_path.stem}.grd"
        output_format = GridFormat.GRID2B if big_endian else GridFormat.GRID2L
        grid.write_to_file(binary_path, WriteOptions(output_format=output_format))
    else:
        binary_path = original_path
    return SourceFile(
        name=binary_path,
        original_name=original_path,
        size=binary_path.stat().st_size,
        coordsys=grid.crdsys_code,
        dimension=grid.dimension,
        range=(grid.ymin, grid.ymax, grid.xmin, grid.xmax),
    )


def _build_trig_source(original_path: Path, big_endian: bool, workdir: Path) -> SourceFile:
    """Builds one trig component: a source with a recognized binary TrigFormat signature is used
    verbatim; anything else is treated as an ASCII source and built into `workdir` in the model's
    own endianness. Unlike GridFile, TrigFile has no ASCII reader of its own, so a failed binary
    read (not a separate heuristic) is what signals an ASCII source here."""
    try:
        trig: TrigFile | None = TrigFile(original_path)
        binary_path = original_path
    except ValueError:
        trig = None
        binary_path = workdir / f"{original_path.stem}.trg"
        build_trig_file(original_path, binary_path, big_endian)
    if trig is None:
        trig = TrigFile(binary_path)
    return SourceFile(
        name=binary_path,
        original_name=original_path,
        size=binary_path.stat().st_size,
        coordsys=trig.crdsys_code,
        dimension=trig.dimension,
        range=(trig.ymin, trig.ymax, trig.xmin, trig.xmax),
    )


def _expand_range(old: Range | None, new: Range) -> Range:
    """Expands `old`'s bounding box to also cover `new`: each range is (ymin, ymax, xmin, xmax),
    so the minima (ymin, xmin) may shrink and the maxima (ymax, xmax) may grow."""
    if old is None:
        return new
    ymin, ymax, xmin, xmax = old
    new_ymin, new_ymax, new_xmin, new_xmax = new
    return (min(ymin, new_ymin), max(ymax, new_ymax), min(xmin, new_xmin), max(xmax, new_xmax))


@dataclass
class Component:
    """One DEFORMATION_COMPONENT record: `source` (see below) plus format-version-dependent
    parameters (`values`, validated generically against the active component schema)."""

    source: str  # the record's raw value: a filename, optionally followed by extra text
    values: dict[str, str] = field(default_factory=dict)
    sourcefile: SourceFile | None = None

    def build(self, big_endian: bool, workdir: Path) -> None:
        """Builds this component's binary sub-file. Only the first whitespace-separated token of
        `source` (the filename) is used; anything after it is ignored."""
        original_path = Path(self.source.split(None, 1)[0])
        if not original_path.is_file():
            raise ValueError(f"Cannot find component source file {original_path}")

        model_type = self.values.get("MODEL_TYPE", "").lower()
        if model_type == "grid":
            self.sourcefile = _build_grid_source(original_path, big_endian, workdir)
        elif model_type == "trig":
            self.sourcefile = _build_trig_source(original_path, big_endian, workdir)
        else:
            raise ValueError(f"Invalid component MODEL_TYPE {self.values.get('MODEL_TYPE', '')}")


@dataclass
class Sequence:
    """One DEFORMATION_SEQUENCE record: a named group of components plus format-version-dependent
    parameters."""

    name: str
    values: dict[str, str] = field(default_factory=dict)
    components: list[Component] = field(default_factory=list)
    range: Range | None = None


@dataclass
class ModelVersion:
    """One VERSION record (format version 3 only): a version number plus its own date/description."""

    version: str
    values: dict[str, str] = field(default_factory=dict)


@dataclass
class DeformationModel:
    """Parses, validates, builds, and writes a LINZ deformation model (.ldm) binary from its ASCII
    definition."""

    name: str
    values: dict[str, str] = field(default_factory=dict)
    sequences: list[Sequence] = field(default_factory=list)
    versions: list[ModelVersion] = field(default_factory=list)
    format: LinzDefFormat | None = None
    range: Range | None = None

    @classmethod
    def load(cls, path: Path) -> DeformationModel:  # pylint: disable=too-many-branches
        """Parses an ASCII deformation model definition into model/sequence/component objects.

        FORMAT (a model parameter) selects the parameter schema used for every subsequent record -
        it must appear before any DEFORMATION_SEQUENCE/VERSION record for those to validate
        correctly, mirroring the original's own ordering requirement."""
        model: DeformationModel | None = None
        sequence: Sequence | None = None
        current: DeformationModel | Sequence | Component | ModelVersion | None = None
        current_params: dict[str, str] | None = _MODEL_PARAMS_V1
        sequence_params: dict[str, str] | None = None
        component_params: dict[str, str] | None = None
        version_params: dict[str, str] | None = None

        with path.open("r", encoding="ascii") as file:
            for raw_line in file:
                if _BLANK_OR_COMMENT_LINE.match(raw_line):
                    continue
                line = raw_line.rstrip("\n")
                parts = line.split(None, 1)
                record_type = parts[0].upper()
                value = parts[1] if len(parts) > 1 else ""

                if _RECORD_KEYWORD.match(record_type):
                    if record_type == "DEFORMATION_MODEL":
                        if model is not None:
                            raise ValueError("Only one DEFORMATION_MODEL can be defined")
                        model = cls(name=value)
                        current = model
                        current_params = _MODEL_PARAMS_V1
                    elif version_params is not None and record_type == "VERSION":
                        if model is None:
                            raise ValueError(f"Must define DEFORMATION MODEL before {record_type}")
                        if not _VERSION_NUMBER.match(value):
                            raise ValueError(f"Invalid version number {value}")
                        current = ModelVersion(version=value)
                        model.versions.append(current)
                        current_params = version_params
                    elif record_type == "DEFORMATION_SEQUENCE":
                        if model is None:
                            raise ValueError(f"Must define DEFORMATION_MODEL before {record_type}")
                        sequence = Sequence(name=value)
                        model.sequences.append(sequence)
                        current = sequence
                        current_params = sequence_params
                    else:  # DEFORMATION_COMPONENT
                        if sequence is None:
                            raise ValueError(f"Must define DEFORMATION_SEQUENCE before {record_type}")
                        current = Component(source=value)
                        sequence.components.append(current)
                        current_params = component_params
                    continue

                if current is None:
                    raise ValueError("DEFORMATION_MODEL must be defined first in file")
                if current_params is None or record_type not in current_params:
                    raise ValueError(f"Parameter {record_type} is not valid in {record_type_label(current)}")

                if record_type == "DESCRIPTION":
                    description_lines = []
                    for description_line in file:
                        if _END_DESCRIPTION_LINE.match(description_line):
                            break
                        description_lines.append(description_line)
                    value = "".join(description_lines)

                current.values[record_type] = value

                if record_type == "FORMAT":
                    assert model is not None
                    fmt = parse_linzdef_format(value)
                    current_params = fmt.schema.model
                    version_params = fmt.schema.version
                    sequence_params = fmt.schema.sequence
                    component_params = fmt.schema.component
                    model.format = fmt

        if model is None:
            raise ValueError("DEFORMATION_MODEL must be defined first in file")
        return model

    def validate(self) -> None:
        """Checks every model/sequence/component parameter's value against its format version's
        schema, raising `ValueError` naming every problem found (not just the first). If FORMAT
        was never given, the bootstrap default schema (_SCHEMA_V1) is still active - matching
        load()'s own fallback - so this reports a normal missing-FORMAT error rather than
        crashing."""
        schema = self.format.schema if self.format is not None else _SCHEMA_V1
        errors = _check_values(self.values, schema.model, "deformation model")
        if schema.version is not None:
            if not self.versions:
                errors.append("Deformation model has no version number")
            for model_version in self.versions:
                errors += _check_values(model_version.values, schema.version, "model version")

        if not self.sequences:
            errors.append("Deformation model has no sequences")
        for sequence in self.sequences:
            errors += _check_values(sequence.values, schema.sequence, "deformation sequence")
            if not sequence.components:
                errors.append("Deformation sequence has no components")
            for component in sequence.components:
                errors += _check_values(component.values, schema.component, "deformation component")

        if errors:
            raise ValueError("Failed with invalid syntax:\n" + "\n".join(errors))

    def resolve_format(self, forced_format: str | None) -> LinzDefFormat:
        """Resolves the effective LINZDEF format to build/write as: `forced_format` if given,
        otherwise the model's own declared FORMAT. A forced format must be the same major version
        as the model's own - the original tool never checked this; this raises instead."""
        if forced_format is None:
            if self.format is None:
                raise ValueError("Deformation model has no FORMAT")
            return self.format
        fmt = parse_linzdef_format(forced_format)
        if self.format is not None and fmt.version != self.format.version:
            raise ValueError(
                f"Forced format {forced_format} is version {fmt.version}, but the model declares "
                f"{self.format.name} (version {self.format.version})"
            )
        return fmt

    def build_components(self, big_endian: bool, workdir: Path) -> None:
        """Validates the model, then builds every component's binary sub-file and computes each
        sequence's and the model's own bounding-box range from its components' ranges."""
        self.validate()
        for sequence in self.sequences:
            for component in sequence.components:
                component.build(big_endian, workdir)
                assert component.sourcefile is not None
                if component.sourcefile.dimension != int(sequence.values["DIMENSION"]):
                    raise ValueError(f"Component {component.sourcefile.original_name} has incorrect dimension")
                sequence.range = _expand_range(sequence.range, component.sourcefile.range)
            assert sequence.range is not None
            self.range = _expand_range(self.range, sequence.range)

    def write_binary(self, path: Path, fmt: LinzDefFormat) -> None:
        """Writes this model as a LINZDEF binary in `fmt`. Every component's sourcefile must
        already be built (see build_components)."""
        for sequence in self.sequences:
            for component in sequence.components:
                if component.sourcefile is None:
                    raise ValueError("Components must be built (see build_components) before writing")
        codec = EndianCodec(fmt.big_endian)
        with path.open("wb") as file:
            file.write(fmt.signature)
            if fmt.version == 1:
                self._write_binary_v1(file, codec)
            else:
                self._write_binary_v23(file, codec, fmt.version)

    def _write_component_bytes(self, file: BinaryIO) -> None:
        """Copies every component's already-built binary bytes into `file` in turn, recording
        each one's byte offset (`SourceFile.location`) for the index written afterwards."""
        for sequence in self.sequences:
            for component in sequence.components:
                assert component.sourcefile is not None
                component.sourcefile.location = file.tell()
                file.write(component.sourcefile.name.read_bytes())

    def _write_model_header_single_version(self, file: BinaryIO, codec: EndianCodec) -> None:
        """Writes the model header shared byte-for-byte by v1 and v2 (and v23's own version-2
        branch): a single VERSION_NUMBER/VERSION_DATE/DESCRIPTION set directly on the model,
        rather than v3's repeated VERSION records."""
        assert self.range is not None
        file.write(
            codec.pack_string(
                [self.name, self.values["VERSION_NUMBER"], self.values["COORDSYS"], self.values.get("DESCRIPTION", "")]
            )
        )
        file.write(_pack_date(codec, self.values["VERSION_DATE"]))
        file.write(_pack_date(codec, self.values["START_DATE"]))
        file.write(_pack_date(codec, self.values["END_DATE"]))
        file.write(codec.pack_double(list(self.range)))
        file.write(codec.pack_short([_yes_no_flag(self.values.get("GEOGRAPHICAL", ""))]))

    def _write_binary_v1(self, file: BinaryIO, codec: EndianCodec) -> None:
        """Writes the v1 binary layout: component bytes first, then a model/sequence/component
        index (whose start offset is recorded right after the signature)."""
        index_pointer_location = file.tell()
        file.write(codec.pack_long([0]))
        self._write_component_bytes(file)

        index_location = file.tell()
        self._write_model_header_single_version(file, codec)
        file.write(codec.pack_short([len(self.sequences)]))

        for sequence in self.sequences:
            assert sequence.range is not None
            file.write(codec.pack_string([sequence.name, sequence.values.get("DESCRIPTION", "")]))
            file.write(_pack_date(codec, sequence.values["START_DATE"]))
            file.write(_pack_date(codec, sequence.values["END_DATE"]))
            file.write(codec.pack_double(list(sequence.range)))
            file.write(codec.pack_short([1 if sequence.values.get("DATA_TYPE", "").lower() == "velocity" else 0]))
            file.write(codec.pack_short([int(sequence.values["DIMENSION"])]))
            file.write(codec.pack_short([_yes_no_flag(sequence.values.get("ZERO_BEYOND_RANGE", ""))]))
            file.write(codec.pack_short([len(sequence.components)]))

            for component in sequence.components:
                sourcefile = component.sourcefile
                assert sourcefile is not None
                before = _METHOD_CODES[component.values.get("BEFORE_REF_DATE", "").lower() or "interpolate"]
                after = _METHOD_CODES[component.values.get("AFTER_REF_DATE", "").lower() or "interpolate"]
                file.write(codec.pack_string([component.values.get("DESCRIPTION", "")]))
                file.write(_pack_date(codec, component.values["REF_DATE"]))
                file.write(codec.pack_double(list(sourcefile.range)))
                file.write(codec.pack_short([before]))
                file.write(codec.pack_short([after]))
                file.write(codec.pack_short([1 if component.values["MODEL_TYPE"].lower() == "trig" else 0]))
                file.write(codec.pack_long([sourcefile.location]))

        file.seek(index_pointer_location)
        file.write(codec.pack_long([index_location]))

    def _write_binary_v23(self, file: BinaryIO, codec: EndianCodec, version: int) -> None:
        """Writes the v2/v3 binary layout: component bytes first, then a model/sequence/component
        index. v3 repeats VERSION_NUMBER/DATE/DESCRIPTION (see `versions`) instead of the single
        set v2 carries directly on the model, and sequences additionally carry a version range."""
        index_pointer_location = file.tell()
        file.write(codec.pack_long([0]))
        self._write_component_bytes(file)

        index_location = file.tell()
        assert self.range is not None
        if version >= 3:
            file.write(codec.pack_string([self.name, self.values["COORDSYS"]]))
            file.write(_pack_date(codec, self.values["START_DATE"]))
            file.write(_pack_date(codec, self.values["END_DATE"]))
            file.write(codec.pack_double(list(self.range)))
            file.write(codec.pack_short([_yes_no_flag(self.values.get("GEOGRAPHICAL", ""))]))
            file.write(codec.pack_short([len(self.versions)]))
            for model_version in self.versions:
                file.write(codec.pack_string([model_version.version]))
                file.write(_pack_date(codec, model_version.values["VERSION_DATE"]))
                file.write(codec.pack_string([model_version.values.get("DESCRIPTION", "")]))
        else:
            self._write_model_header_single_version(file, codec)

        file.write(codec.pack_short([len(self.sequences)]))
        for sequence in self.sequences:
            assert sequence.range is not None
            file.write(codec.pack_string([sequence.name, sequence.values.get("DESCRIPTION", "")]))
            file.write(_pack_date(codec, sequence.values["START_DATE"]))
            file.write(_pack_date(codec, sequence.values["END_DATE"]))
            file.write(codec.pack_double(list(sequence.range)))
            file.write(codec.pack_short([int(sequence.values["DIMENSION"])]))
            file.write(codec.pack_short([_yes_no_flag(sequence.values.get("ZERO_BEYOND_RANGE", ""))]))
            file.write(codec.pack_short([_yes_no_flag(sequence.values.get("NESTED_SEQUENCE", ""))]))
            if version >= 3:
                version_end = sequence.values["VERSION_END"]
                file.write(codec.pack_string([sequence.values["VERSION_START"]]))
                file.write(codec.pack_string(["99999999" if version_end == "0" else version_end]))
            file.write(codec.pack_short([len(sequence.components)]))

            for component in sequence.components:
                sourcefile = component.sourcefile
                assert sourcefile is not None
                initial_factor, steps = _time_model_steps(
                    component.values["TIME_MODEL"],
                    component.values["REF_DATE"],
                    sequence.values["START_DATE"],
                    sequence.values["END_DATE"],
                )
                file.write(codec.pack_string([component.values.get("DESCRIPTION", "")]))
                file.write(_pack_date(codec, component.values["REF_DATE"]))
                file.write(codec.pack_double(list(sourcefile.range)))
                file.write(codec.pack_short([1]))  # time model type - always piecewise linear
                file.write(codec.pack_short([len(steps)]))
                file.write(codec.pack_double([initial_factor]))
                for step_date, step_factor in steps:
                    file.write(_pack_date(codec, step_date))
                    file.write(codec.pack_double([step_factor]))
                file.write(codec.pack_short([1 if component.values["MODEL_TYPE"].lower() == "trig" else 0]))
                file.write(codec.pack_long([sourcefile.location]))

        file.seek(index_pointer_location)
        file.write(codec.pack_long([index_location]))


def _yes_no_flag(value: str) -> int:
    """Packs a `(yes|no)?` schema value as a short: 0 only for an explicit "no", 1 otherwise
    (including when the value is absent) - matching every yes/no field's own default-true
    behavior (GEOGRAPHICAL, ZERO_BEYOND_RANGE, NESTED_SEQUENCE)."""
    return 0 if value.lower() == "no" else 1


def _pack_date(codec: EndianCodec, date: str) -> bytes:
    """Packs a date string as 6 shorts: year, month, day, hour, minute, second (0 if omitted)."""
    parsed = parse_date(date)
    return codec.pack_short([parsed.year, parsed.month, parsed.day, parsed.hour, parsed.minute, parsed.second])


def _time_model_steps(value: str, ref_date: str, seq_start: str, seq_end: str) -> tuple[float, list[tuple[str, float]]]:
    """Parses a TIME_MODEL value into (initial_factor, [(date, factor), ...]) - the same
    piecewise-linear shape the binary format stores (a value for dates at or before the first
    step, then one (date, factor) pair per step). A PIECEWISE_LINEAR value is used as given. A
    VELOCITY value is converted into an equivalent piecewise-linear displacement, spanning the
    sequence's own START_DATE to END_DATE and treating the velocity as constant over each interval
    between its own listed dates (defaulting to one constant-rate interval spanning the whole
    sequence if none are given), then re-centered so the displacement at ref_date is exactly 0."""
    tokens = value.split()
    kind = tokens[0].upper()
    rest = tokens[1:] or ["1.0"]

    if kind == "PIECEWISE_LINEAR":
        initial_factor = float(rest[0])
        steps = [(rest[i], float(rest[i + 1])) for i in range(1, len(rest), 2)]
        return initial_factor, steps

    dates = [seq_start, *rest[1::2], seq_end]
    rates = [float(rest[0]), *(float(rate) for rate in rest[2::2])]
    years = [date_to_year(date) for date in dates]
    ref_year = date_to_year(ref_date)

    cumulative = [0.0]
    offset = 0.0
    for i, rate in enumerate(rates):
        year0, year1 = years[i], years[i + 1]
        if year0 <= ref_year < year1:
            offset = cumulative[-1] + (ref_year - year0) * rate
        cumulative.append(cumulative[-1] + (year1 - year0) * rate)

    displacements = [value - offset for value in cumulative]
    return displacements[0], list(zip(dates, displacements))


def build_and_write(def_path: Path, output_path: Path, forced_format: str | None = None) -> None:
    """Builds a LINZ deformation model binary from its ASCII definition at `def_path`, writing it
    to `output_path`. `forced_format` overrides the model's own declared FORMAT (must be the same
    major version)."""
    model = DeformationModel.load(def_path)
    fmt = model.resolve_format(forced_format)
    with tempfile.TemporaryDirectory() as workdir:
        model.build_components(fmt.big_endian, Path(workdir))
        model.write_binary(output_path, fmt)


def record_type_label(obj: DeformationModel | Sequence | Component | ModelVersion) -> str:
    """Names `obj`'s record type for error messages."""
    if isinstance(obj, DeformationModel):
        return "DEFORMATION_MODEL"
    if isinstance(obj, Sequence):
        return "DEFORMATION_SEQUENCE"
    if isinstance(obj, Component):
        return "DEFORMATION_COMPONENT"
    return "VERSION"


def _check_values(values: dict[str, str], schema: dict[str, str], type_label: str) -> list[str]:
    """Checks each of `schema`'s parameters' value in `values` against its pattern, returning one
    error message per problem found."""
    errors = []
    for name in sorted(schema):
        value = values.get(name, "")
        valid = _valid_time_model(value) if name == "TIME_MODEL" else _matches_pattern(schema[name], value)
        if not valid:
            problem = "Missing value" if value == "" else f"Invalid value {value}"
            errors.append(f"{problem} for {name} in {type_label}")
    return errors


def _matches_pattern(pattern: str, value: str) -> bool:
    """Checks `value` against `pattern`: a plain date string, or a regex fragment otherwise."""
    if pattern == "date":
        return _is_valid_date(value)
    return bool(re.fullmatch(pattern, value, re.IGNORECASE | re.DOTALL))


def _is_valid_date(value: str) -> bool:
    """Whether `value` parses as a valid `dateutil.parse_date` date."""
    try:
        parse_date(value)
        return True
    except ValueError:
        return False


def _is_valid_float(value: str) -> bool:
    """Whether `value` parses as a valid float."""
    try:
        float(value)
        return True
    except ValueError:
        return False


def _valid_time_model(value: str) -> bool:
    """Validates a TIME_MODEL value: `PIECEWISE_LINEAR <float> (<date> <float>)*`, or
    `VELOCITY [<float> (<date> <float>)*]` (keyword case-insensitive) - tokenized and checked
    directly rather than via one compound regex, since each token is just a date or a float."""
    tokens = value.split()
    if not tokens:
        return False
    kind, *rest = tokens
    kind = kind.upper()
    if kind == "VELOCITY" and not rest:
        return True
    if kind not in ("PIECEWISE_LINEAR", "VELOCITY"):
        return False
    if not rest or len(rest) % 2 == 0:
        return False
    if not _is_valid_float(rest[0]):
        return False
    return all(_is_valid_date(rest[i]) and _is_valid_float(rest[i + 1]) for i in range(1, len(rest), 2))
