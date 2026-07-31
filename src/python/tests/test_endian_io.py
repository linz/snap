"""Tests for lib.endian_io.EndianCodec."""

from __future__ import annotations

import io

import pytest

from lib.endian_io import EndianCodec


@pytest.mark.parametrize("big_endian", [False, True])
class TestRoundTrip:
    """Pack a value (or list of values) then unpack it back, for both byte orders."""

    def test_char_round_trip(self, big_endian: bool) -> None:
        """Packs then unpacks a mix of small unsigned byte values."""
        codec = EndianCodec(big_endian)
        values = [0, 1, 127, 255]
        assert codec.unpack_char(codec.pack_char(values)) == tuple(values)

    def test_char_wraps_negative_values(self, big_endian: bool) -> None:
        """A negative input wraps to its low byte rather than raising, matching how compressed
        grid rows store signed differences through this unsigned format. Reading the same bytes
        back as signed (`unpack_byte`) recovers the original negative values."""
        codec = EndianCodec(big_endian)
        packed = codec.pack_char([-1, -128])
        assert codec.unpack_char(packed) == (255, 128)
        assert codec.unpack_byte(packed) == (-1, -128)

    def test_short_round_trip(self, big_endian: bool) -> None:
        """Packs then unpacks the full signed 16-bit range, including negative values."""
        codec = EndianCodec(big_endian)
        values = [-32768, -1, 0, 1, 32767]
        assert codec.unpack_short(codec.pack_short(values)) == tuple(values)

    def test_long_round_trip(self, big_endian: bool) -> None:
        """Packs then unpacks the full signed 32-bit range, including negative values."""
        codec = EndianCodec(big_endian)
        values = [-2147483648, -1, 0, 1, 2147483647]
        assert codec.unpack_long(codec.pack_long(values)) == tuple(values)

    def test_double_round_trip(self, big_endian: bool) -> None:
        """Packs then unpacks a mix of positive, negative, and fractional double values."""
        codec = EndianCodec(big_endian)
        values = [0.0, -1.5, 1.5, 123456.789, -0.000001]
        assert codec.unpack_double(codec.pack_double(values)) == tuple(values)

    def test_single_value_list(self, big_endian: bool) -> None:
        """A single-element list packs and unpacks the same as any other length."""
        codec = EndianCodec(big_endian)
        assert codec.unpack_short(codec.pack_short([42])) == (42,)

    def test_empty_list(self, big_endian: bool) -> None:
        """An empty list packs to an empty buffer and unpacks back to an empty tuple."""
        codec = EndianCodec(big_endian)
        packed = codec.pack_short([])
        assert packed == b""
        assert codec.unpack_short(packed) == ()

    def test_string_round_trip(self, big_endian: bool) -> None:
        """Packs then reads back several strings, including an empty one."""
        codec = EndianCodec(big_endian)
        values = ["hello", "", "grid title"]
        packed = codec.pack_string(values)
        file = io.BytesIO(packed)
        assert codec.read_string(file, len(values)) == tuple(values)


@pytest.mark.parametrize("big_endian", [False, True])
class TestByteOrder:
    """Confirm the packed bytes actually reflect the requested byte order, not just that
    round-tripping through the same codec works (which would pass even if both directions
    silently used the same, possibly wrong, byte order)."""

    def test_short_byte_order(self, big_endian: bool) -> None:
        """A packed short's bytes are in the requested order, not just internally consistent."""
        codec = EndianCodec(big_endian)
        packed = codec.pack_short([1])
        expected = b"\x00\x01" if big_endian else b"\x01\x00"
        assert packed == expected

    def test_long_byte_order(self, big_endian: bool) -> None:
        """A packed long's bytes are in the requested order, not just internally consistent."""
        codec = EndianCodec(big_endian)
        packed = codec.pack_long([1])
        expected = b"\x00\x00\x00\x01" if big_endian else b"\x01\x00\x00\x00"
        assert packed == expected


class TestReadFromFile:
    """`read_*` methods must consume exactly the right number of bytes from the file position,
    so that repeated reads land on the correct subsequent data."""

    def test_sequential_reads_advance_correctly(self) -> None:
        """Reading several values of different types back to back lands on the right data each time."""
        codec = EndianCodec(big_endian=False)
        file = io.BytesIO(codec.pack_short([1, 2]) + codec.pack_long([3]) + codec.pack_double([4.5]))
        assert codec.read_short(file, 2) == (1, 2)
        assert codec.read_long(file) == (3,)
        assert codec.read_double(file) == (4.5,)

    def test_read_string_advances_past_length_prefix_and_data(self) -> None:
        """After reading a string, the file position is past both its length prefix and its bytes."""
        codec = EndianCodec(big_endian=False)
        file = io.BytesIO(codec.pack_string(["abc"]) + codec.pack_short([99]))
        assert codec.read_string(file) == ("abc",)
        assert codec.read_short(file) == (99,)

    def test_read_raises_on_truncated_file(self) -> None:
        """Reading past the end of the file raises rather than silently returning short data."""
        codec = EndianCodec(big_endian=False)
        file = io.BytesIO(b"\x00")
        with pytest.raises(EOFError):
            codec.read_short(file)
