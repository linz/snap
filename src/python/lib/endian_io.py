"""Fixed-endianness binary pack/unpack helpers for the .grd/.trg/.ldm file formats.

Byte order is chosen explicitly per instance and applied via struct's '<'/'>'
format prefixes, so no host-endianness detection is ever needed.
"""

from __future__ import annotations

import struct
from collections.abc import Sequence
from typing import BinaryIO


class EndianCodec:
    """Packs values to, and unpacks values from, a fixed little- or big-endian byte order."""

    def __init__(self, big_endian: bool) -> None:
        """Fixes the byte order this codec will always pack to and unpack from."""
        self._prefix = ">" if big_endian else "<"

    # -- pack (write side) --

    def pack_char(self, values: Sequence[int]) -> bytes:
        """Packs unsigned bytes. Negative inputs wrap to their low byte rather than raising,
        since compressed grid rows store signed differences through this unsigned format."""
        return struct.pack(f"{len(values)}B", *(value & 0xFF for value in values))

    def pack_short(self, values: Sequence[int]) -> bytes:
        """Packs signed 16-bit integers in the codec's byte order."""
        return struct.pack(f"{self._prefix}{len(values)}h", *values)

    def pack_long(self, values: Sequence[int]) -> bytes:
        """Packs signed 32-bit integers in the codec's byte order."""
        return struct.pack(f"{self._prefix}{len(values)}l", *values)

    def pack_double(self, values: Sequence[float]) -> bytes:
        """Packs IEEE 754 double-precision floats in the codec's byte order."""
        return struct.pack(f"{self._prefix}{len(values)}d", *values)

    def pack_string(self, values: Sequence[str]) -> bytes:
        """Packs each string as a short byte-length prefix followed by the null-terminated bytes."""
        result = b""
        for value in values:
            data = value.encode("ascii") + b"\0"
            result += self.pack_short([len(data)]) + data
        return result

    # -- unpack (read side, operating on an in-memory buffer) --

    def unpack_byte(self, buffer: bytes) -> tuple[int, ...]:
        """Unpacks signed bytes from a buffer sized to hold a whole number of them."""
        return struct.unpack(f"{len(buffer)}b", buffer)

    def unpack_char(self, buffer: bytes) -> tuple[int, ...]:
        """Unpacks unsigned bytes from a buffer sized to hold a whole number of them."""
        return struct.unpack(f"{len(buffer)}B", buffer)

    def unpack_short(self, buffer: bytes) -> tuple[int, ...]:
        """Unpacks signed 16-bit integers from a buffer, in the codec's byte order."""
        count = len(buffer) // 2
        return struct.unpack(f"{self._prefix}{count}h", buffer)

    def unpack_long(self, buffer: bytes) -> tuple[int, ...]:
        """Unpacks signed 32-bit integers from a buffer, in the codec's byte order."""
        count = len(buffer) // 4
        return struct.unpack(f"{self._prefix}{count}l", buffer)

    def unpack_double(self, buffer: bytes) -> tuple[float, ...]:
        """Unpacks IEEE 754 double-precision floats from a buffer, in the codec's byte order."""
        count = len(buffer) // 8
        return struct.unpack(f"{self._prefix}{count}d", buffer)

    # -- read (read side, combining a file read with unpack) --

    def read_byte(self, file: BinaryIO, count: int = 1) -> tuple[int, ...]:
        """Reads and unpacks `count` signed bytes from `file`."""
        return self.unpack_byte(_read_exact(file, count))

    def read_char(self, file: BinaryIO, count: int = 1) -> tuple[int, ...]:
        """Reads and unpacks `count` unsigned bytes from `file`."""
        return self.unpack_char(_read_exact(file, count))

    def read_short(self, file: BinaryIO, count: int = 1) -> tuple[int, ...]:
        """Reads and unpacks `count` signed 16-bit integers from `file`."""
        return self.unpack_short(_read_exact(file, count * 2))

    def read_long(self, file: BinaryIO, count: int = 1) -> tuple[int, ...]:
        """Reads and unpacks `count` signed 32-bit integers from `file`."""
        return self.unpack_long(_read_exact(file, count * 4))

    def read_double(self, file: BinaryIO, count: int = 1) -> tuple[float, ...]:
        """Reads and unpacks `count` IEEE 754 double-precision floats from `file`."""
        return self.unpack_double(_read_exact(file, count * 8))

    def read_string(self, file: BinaryIO, count: int = 1) -> tuple[str, ...]:
        """Reads `count` strings from `file`, each preceded by a short byte-length prefix."""
        strings = []
        for _ in range(count):
            (length,) = self.read_short(file)
            data = _read_exact(file, length)
            strings.append(data.replace(b"\0", b"").decode("ascii"))
        return tuple(strings)


def _read_exact(file: BinaryIO, size: int) -> bytes:
    """Reads exactly `size` bytes from `file`, raising `EOFError` if the file is shorter."""
    data = file.read(size)
    if len(data) != size:
        raise EOFError("Unexpected end of file")
    return data
