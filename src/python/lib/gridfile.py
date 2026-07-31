"""Reads and writes LINZ/SNAP grid files: ASCII text, or binary GEOID/GRID1/GRID2 formats.

Row data can be stored three ways on disk, independent of the overall file
format: plain text, a fixed 2-byte short per value (GEOID and GRID1x), or a
variable-width, optionally delta-encoded scheme (GRID2x). `RowEncoding`
captures that, so row reading/building dispatches on it explicitly rather
than on ad hoc format-name string checks.
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass
from enum import Enum, IntEnum
from pathlib import Path
from typing import BinaryIO, NamedTuple

from lib.endian_io import EndianCodec

_MISSING_FIXED_SHORT = 0x7FFF
_MAX_SHORT_MAGNITUDE = 0x7FFF
_MAX_LONG_MAGNITUDE = 0x7FFFFFFF

_REQUIRED_ASCII_HEADERS = (
    "FORMAT",
    "HEADER0",
    "HEADER1",
    "HEADER2",
    "CRDSYS",
    "NGRDX",
    "NGRDY",
    "XMIN",
    "XMAX",
    "YMIN",
    "YMAX",
    "VRES",
    "NDIM",
    "VALUES",
)


class ByteOrder(Enum):
    """Byte order (endianness) for a binary grid file format."""

    LITTLE = "little"
    BIG = "big"


class RowEncoding(Enum):
    """How a single row's data values are stored, independent of the overall file format."""

    ASCII = "ascii"  # plain `V<col>,<row>: v1 v2 ...` text lines
    FIXED = "fixed"  # one 2-byte short per value; a reserved sentinel marks a value missing
    COMPRESSED = "compressed"  # variable-width (1/2/4 byte), optionally delta-encoded per row


class GridFormat(Enum):
    """The on-disk representation of a grid file, along with everything specific to it: its
    binary signature (empty for the text format), byte order, and row encoding. Keeping this
    metadata on the member itself, rather than in separate lookup tables, means a new format
    can't be added without also giving it a signature/byte-order/row-encoding."""

    GEOID = (b"SNAP geoid binary file\r\n\x1a", ByteOrder.LITTLE, RowEncoding.FIXED)
    GRID1L = (b"SNAP grid binary v1.0 \r\n\x1a", ByteOrder.LITTLE, RowEncoding.FIXED)
    GRID1B = (b"CRS grid binary v1.0  \r\n\x1a", ByteOrder.BIG, RowEncoding.FIXED)
    GRID2L = (b"SNAP grid binary v2.0 \r\n\x1a", ByteOrder.LITTLE, RowEncoding.COMPRESSED)
    GRID2B = (b"CRS grid binary v2.0  \r\n\x1a", ByteOrder.BIG, RowEncoding.COMPRESSED)
    ASCII = (b"", ByteOrder.LITTLE, RowEncoding.ASCII)

    def __init__(self, signature: bytes, byte_order: ByteOrder, row_encoding: RowEncoding) -> None:
        """Attaches this format's signature, byte order, and row encoding to the member."""
        self.signature = signature
        self.byte_order = byte_order
        self.row_encoding = row_encoding


def parse_grid_format(name: str) -> GridFormat:
    """Looks up a grid format by name (case-insensitive), raising `ValueError` if unrecognized."""
    try:
        return GridFormat[name.upper()]
    except KeyError as error:
        raise ValueError(f"Invalid grid output format {name}") from error


def _binary_signature_length() -> int:
    """Returns the shared byte length of every binary format's signature, or raises if they differ."""
    lengths = {len(fmt.signature) for fmt in GridFormat if fmt.signature}
    if len(lengths) != 1:
        raise AssertionError("All binary grid format signatures must be the same length")
    return lengths.pop()


_SIGNATURE_LENGTH = _binary_signature_length()
_FORMAT_BY_SIGNATURE: dict[bytes, GridFormat] = {fmt.signature: fmt for fmt in GridFormat if fmt.signature}


class _CompressedWidth(IntEnum):
    """The byte width of a GRID2 compressed row block's values - always 1, 2, or 4 per the file
    format, never any other value. Restricting this to an enum (rather than a bare `int`) means a
    corrupted file's block header can't silently produce a nonsensical width elsewhere: unpacking
    one from a raw value outside {1, 2, 4} (see `_BlockFormat.unpack`) raises `ValueError`
    immediately, rather than, say, a negative width later corrupting the exponent computations
    below."""

    ONE = 1
    TWO = 2
    FOUR = 4


def _compressed_width_max(width_bytes: _CompressedWidth) -> int:
    """The largest magnitude a signed value can hold at `width_bytes` width in the GRID2
    compressed row scheme, reserving the top value as the undefined-point sentinel (see
    `_compressed_undefined_sentinel`)."""
    # `1 << n` (left-shift by n) equals `2 ** n` for any non-negative n: each shift doubles the
    # value, exactly what multiplying by 2 does. Written this way rather than `2 ** n` because
    # int's `**` is typed to return Any (its stub has to allow for a negative exponent producing
    # a float, e.g. `2 ** -1 == 0.5`), whereas `<<` is unambiguously typed as returning `int` -
    # and `width_bytes` being a `_CompressedWidth` guarantees n is never negative here anyway.
    return (1 << (width_bytes * 8 - 1)) - 2


def _compressed_undefined_sentinel(width_bytes: _CompressedWidth) -> int:
    """The reserved value marking an undefined/missing point at `width_bytes` width in the
    GRID2 compressed row scheme."""
    return (1 << (width_bytes * 8 - 1)) - 1


class _BlockFormat(NamedTuple):
    """The per-block header packed into one short at the start of each GRID2 compressed block:
    the byte width and differencing order its values use, whether it covers only a [imin, imax]
    subset of the row rather than the full width, and whether more blocks follow for the rest of
    the row. The writer always emits exactly one full-width-or-subset block per row per
    dimension; `more_blocks` only ever appears set on an empty (all-undefined) block, doubling as
    a "nothing follows" marker there. The reader's loop supports more blocks in general, so a
    future writer could split a row into differently-compressed sections without a format change."""

    width_bytes: _CompressedWidth
    diff_order: int
    subset: bool
    more_blocks: bool = False

    def pack(self) -> int:
        """Packs these fields into the single short value written to the file."""
        subset_bits = (2 if self.more_blocks else 0) + (1 if self.subset else 0)
        return (self.width_bytes * 4 + self.diff_order) * 4 + subset_bits

    @classmethod
    def unpack(cls, value: int) -> _BlockFormat:
        """Unpacks a short value read from the file back into its fields, raising `ValueError`
        if the width nibble isn't one of `_CompressedWidth`'s 3 valid values."""
        return cls(
            width_bytes=_CompressedWidth(value >> 4),
            diff_order=(value >> 2) & 3,
            subset=bool(value & 1),
            more_blocks=bool(value & 2),
        )


@dataclass
class _RowDifferenceStats:
    """First/second differences (`None` at undefined positions) plus running max magnitudes and
    the [imin, imax] span of defined values, treating consecutive *defined* values as adjacent
    regardless of any undefined gap between their positions - matching the GRID2 format's
    differencing scheme, where gaps are skipped rather than resetting the running difference."""

    differences: list[int | None]
    second_differences: list[int | None]
    max_value: float
    max_diff: float
    max_diff2: float
    imin: int
    imax: int
    count: int


def _compute_row_differences(values: list[float | None]) -> _RowDifferenceStats:
    """Computes first/second differences and running max magnitudes across `values`, skipping
    undefined (`None`) positions as though they weren't there. Isolated from the width/diff-order
    decision and the final packing below so this one pass - the actual per-element hot loop - is
    the only place that would need to change for a future vectorized (e.g. numpy) implementation.
    """
    ngridx = len(values)
    differences: list[int | None] = [None] * ngridx
    second_differences: list[int | None] = [None] * ngridx
    max_value = max_diff = max_diff2 = 0.0
    last_value = last_diff = 0.0
    imin = imax = count = 0

    for i, value in enumerate(values):
        if value is None:
            continue
        count += 1
        if count == 1:
            imin = i
        imax = i

        diff = 0 if count == 1 else value - last_value
        diff2 = 0 if count == 2 else diff - last_diff
        differences[i] = int(diff)
        second_differences[i] = int(diff2)
        last_value, last_diff = value, diff

        max_value = max(max_value, abs(value))
        if count > 1:
            max_diff = max(max_diff, abs(diff))
        if count > 2:
            max_diff2 = max(max_diff2, abs(diff2))

    return _RowDifferenceStats(differences, second_differences, max_value, max_diff, max_diff2, imin, imax, count)


def _format_ascii_value(value: float) -> str:
    """Formats a numeric grid value for the ASCII format. Whole numbers print with no trailing
    `.0` (Python's `str(float)` always includes one); fractional values print via `str`'s
    shortest-round-trip form, which is exact but occasionally longer than a fixed-precision
    rendering would give - a deliberate choice over losing precision to match one, since nothing
    depends on this format's exact text, only on it round-tripping without loss."""
    return str(int(value)) if value == int(value) else str(value)


class Resolution(Enum):
    """A non-numeric resolution request: use the value the source file itself specifies,
    or calculate one (AUTO) from the data being written."""

    AS_SPECIFIED = "as_specified"
    AUTO = "auto"


@dataclass(frozen=True)
class WriteOptions:
    """Options controlling `GridFile.write_to_file`'s output.

    `output_format=None` writes in `GridFile.default_output_format` rather than an
    explicitly chosen format.
    """

    output_format: GridFormat | None = None
    vres: float | Resolution = Resolution.AS_SPECIFIED
    real: bool = False
    header_only: bool = False


class GridFile:  # pylint: disable=too-many-instance-attributes
    """Reads and writes LINZ/SNAP grid files (ASCII text or binary GEOID/GRID1*/GRID2* formats)."""

    def __init__(self, filename: Path) -> None:
        """Opens `filename` and parses its header, auto-detecting ASCII vs. binary format."""
        self.filename: Path = filename
        self._file: BinaryIO = self.filename.open("rb")  # noqa: SIM115 - kept open for lazy row reads
        self.titles: tuple[str, str, str] = (f"Grid data from file {self.filename}", "", "")
        self.crdsys_code = "NONE"
        self.dimension = 1
        self.vres = 1.0
        self.output_vres = 1.0
        self.format = GridFormat.ASCII
        self.output_format: GridFormat | None = None
        self.xmin = self.xmax = self.ymin = self.ymax = 0.0
        self.xres = self.yres = 0.0
        self.ngridx = self.ngridy = 0
        self.latlon = False
        self.is_global = False
        self._row_encoding = RowEncoding.ASCII
        self._row_size = 0
        self._row_locations: list[int] = []
        self._row_cache: dict[int, list[float | None]] = {}
        self._codec: EndianCodec | None = None
        fmt = self._identify_input_signature()
        if fmt is GridFormat.ASCII:
            self._initialise_ascii_read()
        else:
            self._initialise_binary_read(fmt)
        if self.ngridx < 2 or self.ngridy < 2:
            raise ValueError(f"Invalid grid definition in grid file {self.filename}")

    def close(self) -> None:
        """Closes the underlying file. Safe to call more than once."""
        self._file.close()

    def __enter__(self) -> GridFile:
        """Allows use as a context manager, closing the file on exit."""
        return self

    def __exit__(self, *exc_info: object) -> None:
        """Closes the file when used as a context manager."""
        self.close()

    @property
    def default_output_format(self) -> GridFormat:
        """The format to fall back to when `WriteOptions.output_format` is `None`: the format
        recorded in an ASCII source's own `FORMAT:` header, if it has one; otherwise GRID2L
        for an ASCII source with no such hint, or ASCII (a readable dump) for a binary source."""
        return self.output_format or (GridFormat.GRID2L if self.format is GridFormat.ASCII else GridFormat.ASCII)

    # -- setup --

    def _identify_input_signature(self) -> GridFormat:
        """Identifies the input file's `GridFormat` by reading its signature, or raises if unrecognized."""
        signature = self._file.read(_SIGNATURE_LENGTH)
        fmt = _FORMAT_BY_SIGNATURE.get(signature)
        if fmt is not None:
            return fmt

        match = re.match(rb"^(\w+):", signature)
        prefix = match.group(1).upper().decode("ascii") if match else ""
        # ASCII headers may appear in any order, so the first one could be any of them.
        if prefix not in (*_REQUIRED_ASCII_HEADERS, "COMMENT"):
            raise ValueError(f"{self.filename} is not a valid grid file - signature incorrect")

        return GridFormat.ASCII

    def _initialise_binary_read(self, fmt: GridFormat) -> None:
        """Parses one of the binary GEOID/GRID1*/GRID2* formats' header and row index."""
        self.format = fmt
        self._row_encoding = fmt.row_encoding
        self._codec = EndianCodec(fmt.byte_order is ByteOrder.BIG)

        (index_location,) = self._codec.read_long(self._file)
        if not index_location:
            raise ValueError("Grid file not completed")
        self._file.seek(index_location)

        ymin, ymax, xmin, xmax, vres = self._codec.read_double(self._file, 5)
        ngridy, ngridx = self._codec.read_short(self._file, 2)
        if fmt is GridFormat.GEOID:
            dimension, latlon = 1, 1
        else:
            dimension, latlon = self._codec.read_short(self._file, 2)
        title0, title1, title2, crdsys_code = self._codec.read_string(  # pylint: disable=unbalanced-tuple-unpacking
            self._file, 4
        )
        row_locations = list(self._codec.read_long(self._file, ngridy))

        self.titles = (title0, title1, title2)
        self.crdsys_code = crdsys_code
        self._set_grid_extents((xmin, xmax, ngridx), (ymin, ymax, ngridy), bool(latlon))
        self.dimension = dimension
        self._row_size = ngridx * dimension
        self._row_locations = row_locations
        self.vres = vres
        self.output_vres = vres

    def _initialise_ascii_read(self) -> None:
        """Parses the human-readable ASCII grid format's `KEY: value` headers and row index."""
        self._file.seek(0)
        headers = {"NDIM": "1", "LATLON": "1", "HEADER1": "", "HEADER2": "", "VALUES": "REAL"}
        file_location = 0
        line = self._read_ascii_line()
        while line is not None and not re.match(r"^\s*v\d+,\d+:", line, re.IGNORECASE):
            match = re.match(r"^\s*(\w+)\s*:\s*(.*?)\s*$", line)
            if match:
                headers[match.group(1).upper()] = match.group(2)
            file_location = self._file.tell()
            line = self._read_ascii_line()

        missing = [key for key in _REQUIRED_ASCII_HEADERS if key not in headers]
        if missing:
            raise ValueError(f"Definition file missing records {' '.join(missing)}")
        fmt = parse_grid_format(headers["FORMAT"])
        if not re.match(r"^\w+$", headers["CRDSYS"]):
            raise ValueError(f"Invalid CRDSYS definition {headers['CRDSYS']}")
        ngridx = int(headers["NGRDX"])
        ngridy = int(headers["NGRDY"])
        vres = 0.0 if headers["VRES"].upper() == "AUTO" else float(headers["VRES"])
        dimension = int(headers["NDIM"])
        if ngridx < 1 or ngridy < 1:
            raise ValueError("Invalid row or column count")
        if vres < 0.0:
            raise ValueError("Invalid resolution")
        if dimension < 1:
            raise ValueError("Invalid grid element dimension")

        self.titles = (headers["HEADER0"], headers["HEADER1"], headers["HEADER2"])
        self.crdsys_code = headers["CRDSYS"]
        self._set_grid_extents(
            (float(headers["XMIN"]), float(headers["XMAX"]), ngridx),
            (float(headers["YMIN"]), float(headers["YMAX"]), ngridy),
            bool(int(headers["LATLON"])),
        )
        self.dimension = dimension
        self.format = GridFormat.ASCII
        self.output_format = fmt
        self.vres = 1.0 if headers["VALUES"].upper() == "REAL" else vres
        self.output_vres = vres
        self._row_encoding = RowEncoding.ASCII

        row_locations = [-1] * ngridy
        row = -1
        match = re.match(r"^\s*v\d+,(\d+):", line, re.IGNORECASE) if line is not None else None
        while match is not None:
            row_number = int(match.group(1))
            if row_number != row:
                row = row_number
                row_locations[row - 1] = file_location
            file_location = self._file.tell()
            line = self._read_ascii_line()
            match = re.match(r"^\s*v\d+,(\d+):", line, re.IGNORECASE) if line is not None else None
        self._row_locations = row_locations

    def _read_ascii_line(self) -> str | None:
        """Reads one line as text, or `None` at end of file."""
        raw = self._file.readline()
        if not raw:
            return None
        return raw.decode("ascii")

    def _set_grid_extents(
        self, x_range: tuple[float, float, int], y_range: tuple[float, float, int], latlon: bool
    ) -> None:
        """Derives grid resolution and global-wraparound state from the extents and point counts."""
        xmin, xmax, ngridx = x_range
        ymin, ymax, ngridy = y_range
        if ngridx < 2 or ngridy < 2:
            raise ValueError(f"{self.filename}: Invalid grid extents")
        xres = (xmax - xmin) / (ngridx - 1)
        yres = (ymax - ymin) / (ngridy - 1)
        self.xmin, self.xmax, self.ymin, self.ymax = xmin, xmax, ymin, ymax
        self.xres, self.yres = xres, yres
        self.ngridx, self.ngridy = ngridx, ngridy
        self.latlon = latlon
        self.is_global = latlon and abs(ngridx * xres - 360) < 0.001

    # -- row data (dispatches on the source file's row encoding; used both for inspection and by write_to_file) --

    def _get_row(self, row: int) -> list[float | None]:
        """Returns a row's data values (`None` for missing), reading and caching it if needed."""
        if row not in self._row_cache:
            if not 0 <= row < self.ngridy:
                raise ValueError(f"Invalid grid row {row} requested from {self.filename}")
            self._file.seek(self._row_locations[row])
            if self._row_encoding is RowEncoding.COMPRESSED:
                values = self._get_row_compressed()
            elif self._row_encoding is RowEncoding.FIXED:
                values = self._get_row_fixed()
            else:
                values = self._get_row_ascii(row)
            self._row_cache[row] = values
        return self._row_cache[row]

    def _get_row_fixed(self) -> list[float | None]:
        """Reads a row stored as one 2-byte short per value; a reserved sentinel marks it missing."""
        assert self._codec is not None
        values = self._codec.read_short(self._file, self._row_size)
        return [None if value == _MISSING_FIXED_SHORT else float(value) for value in values]

    def _get_row_ascii(self, row: int) -> list[float | None]:
        """Reads the `V<col>,<row>: v1 v2 ...` text lines belonging to `row`."""
        data: list[float | None] = [None] * (self.ngridx * self.dimension)
        row_number = row + 1
        line = self._read_ascii_line()
        while line is not None:
            stripped = line.rstrip()
            match = re.match(r"^\s*v(\d+),(\d+):\s*(.*)$", stripped, re.IGNORECASE)
            if not match or int(match.group(2)) != row_number:
                break
            column = int(match.group(1))
            if column < 1 or column > self.ngridx:
                break
            offset = (column - 1) * self.dimension
            point_values = match.group(3).split()
            for dim in range(self.dimension):
                if point_values[dim] != "*":
                    data[offset + dim] = float(point_values[dim])
            line = self._read_ascii_line()
        return data

    def _get_row_compressed(self) -> list[float | None]:
        """Reads a row stored in the variable-width, optionally delta-encoded GRID2 scheme."""
        assert self._codec is not None
        codec = self._codec
        data: list[float | None] = [None] * self._row_size

        for dim in range(self.dimension):
            imin = imax = 0
            more_blocks = True
            while more_blocks:
                (raw_format,) = codec.read_short(self._file)
                block = _BlockFormat.unpack(raw_format)
                more_blocks = block.more_blocks
                undefined_at_width = _compressed_undefined_sentinel(block.width_bytes)

                if more_blocks and not block.subset:
                    break

                imax = self.ngridx - 1
                if block.subset:
                    imin, imax = codec.read_short(self._file, 2)
                count = imax - imin + 1
                delta1 = codec.read_long(self._file)[0] if block.diff_order >= 1 else 0
                delta2 = codec.read_long(self._file)[0] if block.diff_order >= 2 else 0

                if block.width_bytes == 1:
                    raw_values = list(codec.read_byte(self._file, count))
                elif block.width_bytes == 2:
                    raw_values = list(codec.read_short(self._file, count))
                else:
                    raw_values = list(codec.read_long(self._file, count))

                values = self._decode_differences(raw_values, block.diff_order, delta1, delta2, undefined_at_width)

                index = imin * self.dimension + dim
                for value in values:
                    data[index] = value
                    index += self.dimension

        return data

    @staticmethod
    def _decode_differences(
        raw_values: list[int], diff_order: int, delta1: int, delta2: int, undefined_at_width: int
    ) -> list[float | None]:
        """Reverses first/second-differencing on a block of raw compressed values."""
        if diff_order == 0:
            return [None if value == undefined_at_width else float(value) for value in raw_values]

        values: list[float | None] = [delta1]
        for raw in raw_values[1:]:
            if raw == undefined_at_width:
                values.append(None)
                continue
            if diff_order == 1:
                delta1 += raw
                values.append(float(delta1))
            else:
                delta2 += raw
                delta1 += delta2
                values.append(float(delta1))
        return values

    # -- writing --

    def write_to_file(self, path: Path, options: WriteOptions | None = None) -> None:
        """Writes this grid's data to `path` in ASCII or binary form, per `options`, deleting a
        partially-written file if writing fails."""
        try:
            with path.open("wb") as file:
                self.write_to_stream(file, options)
        except Exception:
            path.unlink(missing_ok=True)
            raise

    def write_to_stream(self, stream: BinaryIO, options: WriteOptions | None = None) -> None:
        """Writes this grid's data to the already-open binary stream `stream` (e.g. stdout), in
        ASCII or binary form, per `options`."""
        requested = options or WriteOptions()
        fmt = requested.output_format or self.default_output_format

        vres_request: float | Resolution = requested.vres
        if vres_request is Resolution.AS_SPECIFIED:
            vres_request = self.output_vres
        if isinstance(vres_request, Resolution) or not vres_request:
            vres = self._calc_vres(fmt) if fmt is not GridFormat.ASCII else self.vres
        else:
            vres = vres_request

        if fmt is GridFormat.ASCII:
            self._write_ascii(stream, vres, requested.real, requested.header_only)
        else:
            self._write_binary(stream, fmt, vres, requested.real)

    def _write_ascii(self, file: BinaryIO, vres: float, real: bool, header_only: bool) -> None:
        """Writes the human-readable `KEY: value` header followed by `V<col>,<row>:` data lines."""
        fmt = self.format if self.format is not GridFormat.ASCII else GridFormat.GRID2L
        lines = [
            f"FORMAT: {fmt.name}",
            f"HEADER0: {self.titles[0]}",
            f"HEADER1: {self.titles[1]}",
            f"HEADER2: {self.titles[2]}",
            f"CRDSYS: {self.crdsys_code}",
            f"NGRDX: {self.ngridx}",
            f"NGRDY: {self.ngridy}",
            f"XMIN: {_format_ascii_value(self.xmin)}",
            f"XMAX: {_format_ascii_value(self.xmax)}",
            f"YMIN: {_format_ascii_value(self.ymin)}",
            f"YMAX: {_format_ascii_value(self.ymax)}",
            f"VRES: {_format_ascii_value(vres)}",
        ]
        if fmt is not GridFormat.GEOID:
            lines.append(f"NDIM: {self.dimension}")
            lines.append(f"LATLON: {1 if self.latlon else 0}")

        value_type = "REAL" if vres == 1 or real else "INTEGER"
        if value_type == "REAL":
            real = True
            vres = 1.0
        lines.append(f"VALUES: {value_type}")
        file.write(("\n".join(lines) + "\n").encode("ascii"))

        if header_only:
            return
        for row in range(self.ngridy):
            file.write(self._build_row(row, RowEncoding.ASCII, real, vres))

    def _write_binary(self, file: BinaryIO, fmt: GridFormat, vres: float, real: bool) -> None:
        """Writes the binary signature, row data, and trailing index in `fmt`."""
        codec = EndianCodec(fmt.byte_order is ByteOrder.BIG)
        file.write(fmt.signature)
        index_pointer_location = file.tell()
        file.write(codec.pack_long([0]))

        row_locations = []
        for row in range(self.ngridy):
            row_locations.append(file.tell())
            file.write(self._build_row(row, fmt.row_encoding, real, vres, codec))

        index_location = file.tell()
        file.write(codec.pack_double([self.ymin, self.ymax, self.xmin, self.xmax, vres]))
        file.write(codec.pack_short([self.ngridy, self.ngridx]))
        if fmt is not GridFormat.GEOID:
            file.write(codec.pack_short([self.dimension, 1 if self.latlon else 0]))
        file.write(codec.pack_string([*self.titles, self.crdsys_code]))
        file.write(codec.pack_long(row_locations))

        file.seek(index_pointer_location)
        file.write(codec.pack_long([index_location]))

    def _calc_vres(self, fmt: GridFormat) -> float:
        """Picks a power-of-ten-ish integer scale factor that fits the data within `fmt`'s range."""
        max_value = self._max_data_value()
        ceiling = _MAX_SHORT_MAGNITUDE if fmt.row_encoding is RowEncoding.FIXED else _MAX_LONG_MAGNITUDE
        target = max_value / ceiling
        if target <= 0:
            target = 0.0000001
        vres = 1.0
        while vres < target:
            vres *= 10
        while vres > target:
            vres /= 10
        if vres < target:
            vres *= 2
        if vres < target:
            vres *= 2.5
        if vres < target:
            vres *= 2
        return vres

    def _max_data_value(self) -> float:
        """Returns the largest absolute real-world (post-`vres`-scaling) data value across all rows."""
        max_value = 0.0
        for row in range(self.ngridy):
            for value in self._get_row(row):
                if value is not None:
                    max_value = max(max_value, abs(value))
        return max_value * self.vres

    def _build_row(
        self, row: int, encoding: RowEncoding, real: bool, output_vres: float, codec: EndianCodec | None = None
    ) -> bytes:
        """Scales one row's data to the output resolution, then encodes it per `encoding`."""
        source_row = self._get_row(row)
        is_int = not real
        output_vres = output_vres if is_int else 1.0
        factor = self.vres / (output_vres or 1.0)

        scaled: list[list[float | None]] = [[None] * self.ngridx for _ in range(self.dimension)]
        index = 0
        for column in range(self.ngridx):
            for dim in range(self.dimension):
                value = source_row[index]
                index += 1
                if value is not None:
                    value *= factor
                    if is_int:
                        value = math.floor(value + 0.5)
                scaled[dim][column] = value

        if encoding is RowEncoding.COMPRESSED:
            assert codec is not None
            return self._build_row_compressed(scaled, codec)
        if encoding is RowEncoding.ASCII:
            return self._build_row_ascii(scaled, row)
        assert codec is not None
        return self._build_row_fixed(scaled, codec)

    def _build_row_ascii(self, row_data: list[list[float | None]], row: int) -> bytes:
        """Formats one row as `V<col>,<row>: v1 v2 ...` text lines."""
        lines = []
        for column in range(self.ngridx):
            values = [
                "*" if (value := row_data[dim][column]) is None else _format_ascii_value(value)
                for dim in range(self.dimension)
            ]
            lines.append(f"V{column + 1},{row + 1}: {' '.join(values)}\n")
        return "".join(lines).encode("ascii")

    def _build_row_fixed(self, row_data: list[list[float | None]], codec: EndianCodec) -> bytes:
        """Packs one row as a fixed 2-byte short per value, using a reserved sentinel for missing values."""
        flat = [
            _MISSING_FIXED_SHORT if (value := row_data[dim][column]) is None else int(value)
            for column in range(self.ngridx)
            for dim in range(self.dimension)
        ]
        return codec.pack_short(flat)

    def _build_row_compressed(self, row_data: list[list[float | None]], codec: EndianCodec) -> bytes:
        """Packs one row using the variable-width, optionally delta-encoded GRID2 scheme."""
        return b"".join(self._build_data_row_compressed(row_data[dim], codec) for dim in range(self.dimension))

    def _build_data_row_compressed(  # pylint: disable=too-many-branches
        self, values: list[float | None], codec: EndianCodec
    ) -> bytes:
        """Encodes one data dimension's row values with the smallest workable width and differencing."""
        ngridx = self.ngridx
        stats = _compute_row_differences(values)

        width_bytes = _CompressedWidth.ONE
        if stats.max_diff2 > _compressed_width_max(_CompressedWidth.ONE):
            width_bytes = _CompressedWidth.TWO
        if stats.max_diff2 > _compressed_width_max(_CompressedWidth.TWO):
            width_bytes = _CompressedWidth.FOUR
        if stats.imax - stats.imin < 4:
            width_bytes = _CompressedWidth.FOUR
        width_max = _compressed_width_max(width_bytes)
        diff_order = 2
        if stats.max_diff < width_max:
            diff_order = 1
        if stats.max_value < width_max:
            diff_order = 0
        if width_bytes == 4:
            diff_order = 0

        is_empty = stats.count == 0
        is_subset = not is_empty and (stats.imin > 0 or stats.imax < ngridx - 1)
        block = _BlockFormat(width_bytes=width_bytes, diff_order=diff_order, subset=is_subset, more_blocks=is_empty)
        buffer = codec.pack_short([block.pack()])
        if is_empty:
            return buffer

        codec_values: list[int | None]
        if diff_order == 0:
            codec_values = [None if v is None else int(v) for v in values]
        elif diff_order == 1:
            codec_values = stats.differences
        else:
            codec_values = stats.second_differences

        if is_subset:
            buffer += codec.pack_short([stats.imin, stats.imax])
        if diff_order:
            if values[stats.imin] is None:
                raise AssertionError(f"No defined value at row index {stats.imin} despite a non-empty block")
            buffer += codec.pack_long([int(values[stats.imin])])  # type: ignore[arg-type]
            if diff_order == 2:
                for i in range(stats.imin + 1, stats.imax + 1):
                    if stats.differences[i] is not None:
                        buffer += codec.pack_long([stats.differences[i]])  # type: ignore[list-item]
                        break

        undefined_at_width = _compressed_undefined_sentinel(width_bytes)
        codec_values_in_range = codec_values[stats.imin : stats.imax + 1] if is_subset else codec_values
        subset_values = [undefined_at_width if v is None else v for v in codec_values_in_range]

        if width_bytes == 1:
            buffer += codec.pack_char(subset_values)
        elif width_bytes == 2:
            buffer += codec.pack_short(subset_values)
        else:
            buffer += codec.pack_long(subset_values)
        return buffer
