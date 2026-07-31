"""Byte-for-byte fixture tests for grid.py/trig.py/linzdeformationmodel.py against real production
regression-test data, rather than the synthetic/small fixtures test_gridfile.py/test_trigfile.py/
test_deformationmodel.py use - real data is larger, has more varied magnitudes and precision, and
exercises the CLI scripts directly rather than the library they wrap.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from grid import main as grid_main
from linzdeformationmodel import main as linzdefmodel_main
from trig import main as trig_main

REPO_ROOT = Path(__file__).resolve().parents[3]
REGRESSION_SRC = REPO_ROOT / "regression_tests" / "snap" / "src"
REGRESSION_IN = REPO_ROOT / "regression_tests" / "snap" / "in"
FIXTURES = Path(__file__).parent / "fixtures"


class TestGridGoldenFixtures:
    """grid.py against real production grid data."""

    def test_ascii_to_grid1l_matches_checked_in_ground_truth(self, tmp_path: Path) -> None:
        """With no -f, grid.py falls back to the real ASCII source's own FORMAT: GRID1L header,
        matching the binary already checked into regression_tests/snap/in/ (used elsewhere by the
        C++ regression suite)."""
        out_path = tmp_path / "velgrid.grd"
        assert grid_main(["-i", str(REGRESSION_SRC / "cmn_test_velgrid.gdf"), "-o", str(out_path)]) == 0
        assert out_path.read_bytes() == (REGRESSION_IN / "cmn_test_velgrid.grd").read_bytes()

    def test_grid1b_matches_perl_reference(self, tmp_path: Path) -> None:
        """grid.py -f GRID1B on a real production ASCII source with no checked-in golden binary
        matches a reference generated once by actually running makegrid.pl -f GRID1B."""
        out_path = tmp_path / "defgrid.grd"
        args = ["-i", str(REGRESSION_SRC / "cmr_test_defgrid.gdf"), "-o", str(out_path), "-f", "GRID1B"]
        assert grid_main(args) == 0
        assert out_path.read_bytes() == (FIXTURES / "golden" / "cmr_test_defgrid.grid1b.grd").read_bytes()

    def test_ascii_dump_round_trips_to_the_original_binary(self, tmp_path: Path) -> None:
        """Dumping the real production GRID1L binary to ASCII, then converting that ASCII straight
        back to binary (its own FORMAT: header names GRID1L, so no -f is needed), reproduces the
        original file exactly - proving the ASCII form's number formatting round-trips without
        loss. Tested this way, rather than pinning an exact text snapshot, since the precise text
        representation of a value isn't itself a compatibility requirement - only its round-trip
        precision is."""
        original = REGRESSION_IN / "cmn_test_velgrid.grd"
        dumped = tmp_path / "velgrid.txt"
        rebuilt = tmp_path / "velgrid.grd"
        assert grid_main(["-i", str(original), "-o", str(dumped), "-f", "ASCII"]) == 0
        assert grid_main(["-i", str(dumped), "-o", str(rebuilt)]) == 0
        assert rebuilt.read_bytes() == original.read_bytes()


class TestTrigGoldenFixtures:
    """trig.py against real production trig data - reusing test_trigfile.py's own fixtures
    already established in an earlier session, rather than regenerating them."""

    def test_deftrig1_matches_perl_reference_both_endians(self, tmp_path: Path) -> None:
        """trig.py -f TRIG2L/-f TRIG2B on a real production ASCII source matches the fixtures
        already built by actually running maketrig.pl. -f must be given here: the source's own
        FORMAT header says TRIG1B, which build_trig_file rejects (only TRIG2L/TRIG2B are valid
        binary targets), so the fallback default can't apply."""
        source = REGRESSION_SRC / "cmn_test_deftrig1.trg"
        for suffix, fmt in (("trig2l", "TRIG2L"), ("trig2b", "TRIG2B")):
            out_path = tmp_path / f"out.{suffix}"
            assert trig_main(["-i", str(source), "-o", str(out_path), "-f", fmt]) == 0
            assert out_path.read_bytes() == (FIXTURES / "trig_ascii" / f"deftrig1.{suffix}.trg").read_bytes()

    def test_deftrig2_matches_perl_reference(self, tmp_path: Path) -> None:
        """Same as above for the second real production trig source (only a TRIG2L ground truth
        was built for this one)."""
        out_path = tmp_path / "out.trg"
        source = REGRESSION_SRC / "cmn_test_deftrig2.trg"
        assert trig_main(["-i", str(source), "-o", str(out_path), "-f", "TRIG2L"]) == 0
        assert out_path.read_bytes() == (FIXTURES / "trig_ascii" / "deftrig2.trig2l.trg").read_bytes()


class TestLinzDeformationModelGoldenFixtures:  # pylint: disable=too-few-public-methods
    """linzdeformationmodel.py against real production deformation-model data."""

    def test_matches_checked_in_ground_truth(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        """cmn_test_linzdef.def's own DEFORMATION_COMPONENT lines are bare filenames, resolved
        relative to the current working directory (matching the original tool, which never
        resolved them relative to the .def file's own location either - build_ldm.bat itself runs
        from this same directory). Unlike test_deformationmodel.py's library-level tests, there's
        no loaded model object here to rewrite paths on between load and build - main() runs the
        whole pipeline in one call - so this is the one test in this file that needs the real
        working directory, scoped to just this test via monkeypatch."""
        monkeypatch.chdir(REGRESSION_SRC)
        out_path = tmp_path / "cmn_test_linzdef.ldm"
        assert linzdefmodel_main(["-i", "cmn_test_linzdef.def", "-o", str(out_path)]) == 0
        assert out_path.read_bytes() == (REGRESSION_IN / "cmn_test_linzdef.ldm").read_bytes()
