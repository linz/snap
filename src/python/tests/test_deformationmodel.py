"""Tests lib.deformationmodel.DeformationModel: parsing/validation, component building, and
binary writing. The v1 coverage uses the real production
regression_tests/snap/src/cmn_test_linzdef.def, including a byte-exact check against the
checked-in regression_tests/snap/in/cmn_test_linzdef.ldm - built by this same Python port rather
than Perl, since two of its embedded trig components (cmn_test_deftrig1.trg/
cmn_test_deftrig2.trg) each have their own point-ordering tie that Perl's maketrig.pl only ever
resolved via its own non-reproducible hash iteration order (see test_trigfile.py's module
docstring). The resulting triangulation is structurally identical either way - same points, same
triangulation edges, same displacement values - only the arbitrary tie-break ordering differs, so
this fixture is a faithful stand-in for what a Perl build would have produced, not a divergent
one. No real v2/v3 LINZDEF file exists anywhere in the repo, so v2/v3 coverage uses small
synthetic sources built here, referencing the existing small grid/trig fixtures by absolute path
- DEFORMATION_COMPONENT source paths were resolved relative to the current working directory in
makelinzdefmodel.pl (never relative to the .def file's own location), a behavior this port
matches, so a fixed relative path wouldn't be portable across different invocation directories.
"""

from __future__ import annotations

import io
from pathlib import Path

import pytest

from lib.dateutil import date_to_year
from lib.deformationmodel import DeformationModel, LinzDefFormat, _time_model_steps, build_and_write
from lib.endian_io import EndianCodec

REPO_ROOT = Path(__file__).resolve().parents[3]
REGRESSION_SRC = REPO_ROOT / "regression_tests" / "snap" / "src"
REGRESSION_IN = REPO_ROOT / "regression_tests" / "snap" / "in"
FIXTURES = Path(__file__).parent / "fixtures"
GRID_SOURCE = FIXTURES / "grid_ascii" / "simple.txt"
TRIG_SOURCE = FIXTURES / "trig_ascii" / "quad.txt"


def _load_real_v1_model() -> DeformationModel:
    """Loads the real production v1 file, rewriting each component's source to an absolute path
    via `.resolve()` (not simply trusting REGRESSION_SRC to already be one). The checked-in .def
    uses bare filenames there, resolved relative to whatever directory makelinzdefmodel.pl
    happened to be run from - not something a test should depend on."""
    model = DeformationModel.load(REGRESSION_SRC / "cmn_test_linzdef.def")
    for sequence in model.sequences:
        for component in sequence.components:
            filename = component.source.split(None, 1)[0]
            component.source = str((REGRESSION_SRC / filename).resolve())
    return model


def _write_v2_def(tmp_path: Path, *, nested: str = "no") -> Path:
    """Writes a small synthetic v2 .def file: one sequence with a single velocity-time-model
    trig component."""
    source = tmp_path / "synthetic_v2.def"
    source.write_text(
        f"""\
DEFORMATION_MODEL Synthetic v2 test model
FORMAT LINZDEF2L
VERSION_NUMBER 1.0
VERSION_DATE 1-Jan-2020
START_DATE 1-Jan-1990
END_DATE 1-Jan-2030
COORDSYS NZGD2000
DESCRIPTION
A tiny synthetic v2 model for testing.
END_DESCRIPTION

DEFORMATION_SEQUENCE Test sequence
DIMENSION 1
START_DATE 1-Jan-2000
END_DATE 1-Jan-2010
ZERO_BEYOND_RANGE no
NESTED_SEQUENCE {nested}
DESCRIPTION
END_DESCRIPTION

DEFORMATION_COMPONENT {TRIG_SOURCE}
MODEL_TYPE trig
REF_DATE 1-Jan-2005
TIME_MODEL VELOCITY 2.0
DESCRIPTION
END_DESCRIPTION
"""
    )
    return source


def _write_v3_def(tmp_path: Path) -> Path:
    """Writes a small synthetic v3 .def file: two VERSION records, one sequence with a version
    range and a single grid component."""
    source = tmp_path / "synthetic_v3.def"
    source.write_text(
        f"""\
DEFORMATION_MODEL Synthetic v3 test model
FORMAT LINZDEF3L
START_DATE 1-Jan-1990
END_DATE 1-Jan-2030
COORDSYS NZGD2000

VERSION 20200101
VERSION_DATE 1-Jan-2020
DESCRIPTION
First version
END_DESCRIPTION

VERSION 20210101
VERSION_DATE 1-Jan-2021
DESCRIPTION
Second version
END_DESCRIPTION

DEFORMATION_SEQUENCE Test sequence
DIMENSION 1
START_DATE 1-Jan-2000
END_DATE 1-Jan-2010
ZERO_BEYOND_RANGE no
NESTED_SEQUENCE no
VERSION_START 20200101
VERSION_END 0
DESCRIPTION
END_DESCRIPTION

DEFORMATION_COMPONENT {GRID_SOURCE}
MODEL_TYPE grid
REF_DATE 1-Jan-2005
TIME_MODEL PIECEWISE_LINEAR 1.0
DESCRIPTION
END_DESCRIPTION
"""
    )
    return source


class TestLoadAndValidateRealV1:
    """Parsing and validating the real production v1 file."""

    def test_loads_and_validates(self) -> None:
        """The real production file parses to its known format/name/sequence count and validates
        cleanly."""
        model = DeformationModel.load(REGRESSION_SRC / "cmn_test_linzdef.def")
        model.validate()
        assert model.format is LinzDefFormat.LINZDEF1B
        assert model.name == "NZGD1949 deformation model"
        assert len(model.sequences) == 6

    def test_missing_format_reports_via_bootstrap_schema(self, tmp_path: Path) -> None:
        """FORMAT missing entirely leaves the bootstrap default schema active (matching the
        original's own behavior), reporting a normal validation error rather than crashing."""
        source = tmp_path / "no_format.def"
        source.write_text("DEFORMATION_MODEL x\nCOORDSYS NZGD2000\n")
        model = DeformationModel.load(source)
        assert model.format is None
        with pytest.raises(ValueError, match="FORMAT"):
            model.validate()

    def test_invalid_parameter_name_raises(self, tmp_path: Path) -> None:
        """A record type not present in the active schema raises `ValueError` naming it."""
        source = tmp_path / "bad_param.def"
        source.write_text("DEFORMATION_MODEL x\nFORMAT LINZDEF1B\nNOT_A_REAL_PARAM y\n")
        with pytest.raises(ValueError, match="NOT_A_REAL_PARAM"):
            DeformationModel.load(source)

    def test_sequence_before_model_raises(self, tmp_path: Path) -> None:
        """A DEFORMATION_SEQUENCE record with no preceding DEFORMATION_MODEL raises `ValueError`."""
        source = tmp_path / "bad_order.def"
        source.write_text("DEFORMATION_SEQUENCE x\n")
        with pytest.raises(ValueError, match="DEFORMATION_MODEL"):
            DeformationModel.load(source)


class TestBuildComponents:
    """Building components against the real production v1 file."""

    def test_builds_and_forces_model_endianness(self, tmp_path: Path) -> None:
        """Every component is forced to the model's own big-endian format, regardless of what
        format each individual ASCII source itself declares (the velocity grid source declares
        little-endian GRID1L, but the model here is LINZDEF1B)."""
        model = _load_real_v1_model()
        fmt = model.resolve_format(None)
        assert fmt.big_endian is True
        model.build_components(fmt.big_endian, tmp_path)

        grid_component = model.sequences[0].components[0]
        assert grid_component.sourcefile is not None
        assert grid_component.sourcefile.name.read_bytes()[:20] == b"CRS grid binary v2.0"
        assert grid_component.sourcefile.coordsys == "NZGD1949"
        assert grid_component.sourcefile.dimension == 2
        assert model.range is not None

    def test_dimension_mismatch_raises(self, tmp_path: Path) -> None:
        """A grid/trig source whose own dimension disagrees with its sequence's DIMENSION is
        rejected."""
        source = tmp_path / "mismatch.def"
        source.write_text(
            f"""\
DEFORMATION_MODEL x
FORMAT LINZDEF2L
VERSION_NUMBER 1.0
VERSION_DATE 1-Jan-2020
START_DATE 1-Jan-1990
END_DATE 1-Jan-2030
COORDSYS NZGD2000

DEFORMATION_SEQUENCE s
DIMENSION 2
START_DATE 1-Jan-2000
END_DATE 1-Jan-2010

DEFORMATION_COMPONENT {GRID_SOURCE}
MODEL_TYPE grid
REF_DATE 1-Jan-2005
TIME_MODEL PIECEWISE_LINEAR 1.0
"""
        )
        model = DeformationModel.load(source)
        with pytest.raises(ValueError, match="incorrect dimension"):
            model.build_components(False, tmp_path)


class TestTimeModelSteps:
    """_time_model_steps: PIECEWISE_LINEAR passthrough and the VELOCITY-to-displacement
    conversion, hand-verified against the same arithmetic dbl4_utl_lnzdef.cpp's
    load_component/calc_seq_def performs at evaluation time."""

    def test_piecewise_linear_used_as_given(self) -> None:
        """A PIECEWISE_LINEAR value passes through unconverted - no VELOCITY re-centering."""
        factor0, steps = _time_model_steps(
            "PIECEWISE_LINEAR 0.0 30-Jun-2002 1.0 30-Jun-2003 0.0", "1-Jan-2000", "1-Jan-1990", "1-Jan-2010"
        )
        assert factor0 == 0.0
        assert steps == [("30-Jun-2002", 1.0), ("30-Jun-2003", 0.0)]

    def test_bare_velocity_defaults_to_rate_one(self) -> None:
        """A bare "VELOCITY" (no explicit rate) defaults to rate 1.0."""
        factor0, steps = _time_model_steps("VELOCITY", "1-Jan-2005", "1-Jan-2000", "1-Jan-2010")
        assert factor0 == -5.0
        assert steps == [("1-Jan-2000", -5.0), ("1-Jan-2010", 5.0)]

    def test_single_segment_velocity_zeroed_at_reference_date(self) -> None:
        """A constant 2.0/year velocity over a 10-year sequence, reference date at the exact
        midpoint: the resulting piecewise-linear curve must cross exactly 0 there."""
        factor0, steps = _time_model_steps("VELOCITY 2.0", "1-Jan-2005", "1-Jan-2000", "1-Jan-2010")
        assert factor0 == -10.0
        assert steps == [("1-Jan-2000", -10.0), ("1-Jan-2010", 10.0)]
        self._assert_zero_at_ref(factor0, steps, "1-Jan-2005")

    def test_multi_segment_velocity_zeroed_at_reference_date(self) -> None:
        """Rate 1.0/year until 2005, then rate 3.0/year until 2010; reference date 3 years into
        the second (faster) segment."""
        factor0, steps = _time_model_steps("VELOCITY 1.0 1-Jan-2005 3.0", "1-Jan-2008", "1-Jan-2000", "1-Jan-2010")
        assert factor0 == -14.0
        assert steps == [("1-Jan-2000", -14.0), ("1-Jan-2005", -9.0), ("1-Jan-2010", 6.0)]
        self._assert_zero_at_ref(factor0, steps, "1-Jan-2008")

    @staticmethod
    def _assert_zero_at_ref(factor0: float, steps: list[tuple[str, float]], ref_date: str) -> None:
        """Linearly interpolates the piecewise curve (factor0 must equal steps[0]'s own factor -
        no discontinuity before the first step) at `ref_date` and checks it lands on 0 - the
        whole point of the VELOCITY-to-displacement conversion."""
        assert factor0 == steps[0][1]
        years = [date_to_year(date) for date, _ in steps]
        factors = [factor for _, factor in steps]
        ref_year = date_to_year(ref_date)
        for i in range(len(years) - 1):
            if years[i] <= ref_year <= years[i + 1]:
                interpolated = factors[i] + (ref_year - years[i]) / (years[i + 1] - years[i]) * (
                    factors[i + 1] - factors[i]
                )
                assert interpolated == pytest.approx(0.0, abs=1e-9)
                return
        pytest.fail("reference date outside the piecewise range")


class TestWriteBinary:
    """Writing the final LINZDEF binary."""

    def test_v1_matches_checked_in_ground_truth(self, tmp_path: Path) -> None:
        """The real production v1 file builds byte-identical to the checked-in regression-suite
        fixture."""
        model = _load_real_v1_model()
        fmt = model.resolve_format(None)
        model.build_components(fmt.big_endian, tmp_path)
        output = tmp_path / "cmn_test_linzdef.ldm"
        model.write_binary(output, fmt)
        assert output.read_bytes() == (REGRESSION_IN / "cmn_test_linzdef.ldm").read_bytes()

    def test_v2_header_and_component(self, tmp_path: Path) -> None:
        """Decodes a synthetic v2 build's model/sequence/component header fields directly off
        the binary, including the VELOCITY-to-displacement TIME_MODEL conversion's factor0."""
        source = _write_v2_def(tmp_path)
        output = tmp_path / "out.ldm"
        build_and_write(source, output)
        data = output.read_bytes()

        signature = LinzDefFormat.LINZDEF2L.signature
        assert data[: len(signature)] == signature
        codec = EndianCodec(False)
        file = io.BytesIO(data)
        file.read(len(signature))
        (index_location,) = codec.read_long(file, 1)  # raw component bytes come first; seek past them
        file.seek(index_location)
        name, version_number, coordsys, description = codec.read_string(  # pylint: disable=unbalanced-tuple-unpacking
            file, 4
        )
        assert (name, version_number, coordsys) == ("Synthetic v2 test model", "1.0", "NZGD2000")
        assert "tiny synthetic v2 model" in description
        codec.read_short(file, 6 * 3)  # VERSION_DATE, START_DATE, END_DATE
        codec.read_double(file, 4)  # range
        (geographical,) = codec.read_short(file, 1)
        assert geographical == 1  # unset GEOGRAPHICAL defaults to geographic (1)
        (nseq,) = codec.read_short(file, 1)
        assert nseq == 1

        seq_name, seq_description = codec.read_string(file, 2)  # pylint: disable=unbalanced-tuple-unpacking
        assert (seq_name, seq_description) == ("Test sequence", "")
        codec.read_short(file, 12)  # START_DATE, END_DATE
        codec.read_double(file, 4)  # range
        (dimension,) = codec.read_short(file, 1)
        (zero_beyond, nested) = codec.read_short(file, 2)
        assert (dimension, zero_beyond, nested) == (1, 0, 0)  # ZERO_BEYOND_RANGE no -> 0
        (ncomp,) = codec.read_short(file, 1)
        assert ncomp == 1

        (comp_description,) = codec.read_string(file, 1)  # pylint: disable=unbalanced-tuple-unpacking
        assert comp_description == ""
        codec.read_short(file, 6)  # REF_DATE
        codec.read_double(file, 4)  # sourcefile range
        (time_model_type,) = codec.read_short(file, 1)
        assert time_model_type == 1
        (nstep,) = codec.read_short(file, 1)
        assert nstep == 2
        (factor0,) = codec.read_double(file, 1)
        assert factor0 == -10.0

    def test_v3_multiple_versions_and_sequence_version_range(self, tmp_path: Path) -> None:
        """Decodes a synthetic v3 build's repeated VERSION records and a sequence's
        VERSION_START/VERSION_END, including the VERSION_END "0" -> "99999999" substitution."""
        source = _write_v3_def(tmp_path)
        output = tmp_path / "out.ldm"
        build_and_write(source, output)
        data = output.read_bytes()

        signature = LinzDefFormat.LINZDEF3L.signature
        assert data[: len(signature)] == signature
        codec = EndianCodec(False)
        file = io.BytesIO(data)
        file.read(len(signature))
        (index_location,) = codec.read_long(file, 1)  # raw component bytes come first; seek past them
        file.seek(index_location)
        name, coordsys = codec.read_string(file, 2)  # pylint: disable=unbalanced-tuple-unpacking
        assert (name, coordsys) == ("Synthetic v3 test model", "NZGD2000")
        codec.read_short(file, 12)  # START_DATE, END_DATE
        codec.read_double(file, 4)  # range
        codec.read_short(file, 1)  # geographical
        (nversions,) = codec.read_short(file, 1)
        assert nversions == 2

        for expected_version in ("20200101", "20210101"):
            (version,) = codec.read_string(file, 1)  # pylint: disable=unbalanced-tuple-unpacking
            assert version == expected_version
            codec.read_short(file, 6)  # VERSION_DATE
            codec.read_string(file, 1)  # DESCRIPTION

        (nseq,) = codec.read_short(file, 1)
        assert nseq == 1
        codec.read_string(file, 2)  # name, DESCRIPTION
        codec.read_short(file, 12)  # START_DATE, END_DATE
        codec.read_double(file, 4)  # range
        codec.read_short(file, 3)  # DIMENSION, ZERO_BEYOND_RANGE, NESTED_SEQUENCE
        version_start, version_end = codec.read_string(file, 2)  # pylint: disable=unbalanced-tuple-unpacking
        assert (version_start, version_end) == ("20200101", "99999999")  # VERSION_END 0 -> 99999999

    def test_resolve_format_rejects_mismatched_major_version(self) -> None:
        """A forced --format whose major version disagrees with the file's own declared FORMAT
        is rejected, rather than silently writing a wrong-version binary."""
        model = DeformationModel.load(REGRESSION_SRC / "cmn_test_linzdef.def")  # a v1 file
        with pytest.raises(ValueError, match="version"):
            model.resolve_format("LINZDEF3L")
