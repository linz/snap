"""Reads LINZ/SNAP binary triangulated deformation-model files (TRIG2L/TRIG2B format), and builds
one from an ASCII triangulation source describing a set of points and the triangles connecting
them.

Only the binary format's read side and its metadata fields are exposed here; interpolating within
the triangulation isn't needed by anything this port targets, so it's left out.

`build_trig_file` raises on the first problem found, rather than accumulating every error before
reporting - a caller wanting file-style multi-error output can catch and re-report.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import BinaryIO

from lib.endian_io import EndianCodec


class TrigFormat(Enum):
    """The on-disk representation of a binary trig file: its signature and byte order, named the
    same way as the value an ASCII source's own `FORMAT` header carries."""

    TRIG2L = (b"SNAP trig binary v2.0 \r\n\x1a", False)
    TRIG2B = (b"CRS trig binary v2.0  \r\n\x1a", True)

    def __init__(self, signature: bytes, big_endian: bool) -> None:
        """Attaches this format's signature and byte order to the member."""
        self.signature = signature
        self.big_endian = big_endian


def parse_trig_format(name: str) -> TrigFormat:
    """Looks up a trig format by name (case-insensitive), raising `ValueError` if unrecognized."""
    try:
        return TrigFormat[name.upper()]
    except KeyError as error:
        raise ValueError(f"Invalid trig output format {name}") from error


_SIGNATURE_LENGTH = len(TrigFormat.TRIG2L.signature)
_FORMAT_BY_SIGNATURE = {fmt.signature: fmt for fmt in TrigFormat}

_REQUIRED_ASCII_HEADERS = ("FORMAT", "HEADER0", "HEADER1", "HEADER2", "CRDSYS", "NDIM")
_POINT_LINE = re.compile(r"^\s*P\s+(\d+)\s+(-?\d+\.?\d*)\s+(-?\d+\.?\d*)\s+(.*?)\s*$", re.IGNORECASE)
_TRIANGLE_LINE = re.compile(r"^\s*T\s+(\d+)\s+(\d+)\s+(\d+)\s*$", re.IGNORECASE)
_HEADER_LINE = re.compile(r"^\s*(\S+)\s*(\S.*?)\s*$")


class TrigFile:  # pylint: disable=too-many-instance-attributes, too-few-public-methods
    """Reads a LINZ/SNAP binary triangulated deformation-model file (TRIG2L/TRIG2B)."""

    def __init__(self, filename: Path) -> None:
        """Opens `filename` and parses its header, point, and topology data."""
        self.filename: Path = filename
        with filename.open("rb") as file:
            self._read(file)

    def _read(self, file: BinaryIO) -> None:
        """Parses the binary signature, header, and point/topology arrays from `file`."""
        signature = file.read(_SIGNATURE_LENGTH)
        fmt = _FORMAT_BY_SIGNATURE.get(signature)
        if fmt is None:
            raise ValueError(f"{self.filename} is not a valid triangle file - signature incorrect")
        codec = EndianCodec(fmt.big_endian)

        title0, title1, title2, crdsys_code = codec.read_string(  # pylint: disable=unbalanced-tuple-unpacking
            file, 4
        )
        ymin, ymax, xmin, xmax = codec.read_double(file, 4)
        npts, dimension = codec.read_short(file, 2)
        (narray,) = codec.read_long(file)

        self.titles: tuple[str, str, str] = (title0, title1, title2)
        self.crdsys_code = crdsys_code
        self.xmin, self.xmax, self.ymin, self.ymax = xmin, xmax, ymin, ymax
        self.npts = npts
        self.dimension = dimension
        self._point_coords = codec.read_double(file, 2 * npts)
        self._point_data = codec.read_double(file, dimension * npts)
        self._topology_index = codec.read_long(file, npts)
        self._topology_data = codec.read_short(file, narray)


@dataclass
class _Point:
    """One point record from an ASCII trig source (an id, coordinates, and data values), plus the
    one thing the build algorithm attaches to it afterwards:

    - `source_id`/`x`/`y`/`data`: taken directly from the record. `source_id` is only used in
      error messages, to let a message trace back to the user's own file - everything else below
      is keyed on `index`, not this.
    - `index`: this point's 1-based position once every point is sorted ascending by `x` - not an
      identifier of its own, just a cached position. Triangle vertices and the adjacency
      structures built in `build_trig_file` are all keyed on it, and it's what actually gets
      written to the binary file's point/topology arrays. `None` until `build_trig_file` assigns
      it.

    A point's neighbour/topology information is deliberately *not* stored here - see
    `build_trig_file`'s `next_point`/`opposite_node` for why that's contributed by triangles
    rather than being a property of the point itself.
    """

    source_id: int
    x: float
    y: float
    data: list[float]
    index: int | None = None


def _index_of(point: _Point) -> int:
    """Returns `point.index`, narrowed to `int`: always real by the time triangle-linking runs,
    since `build_trig_file` assigns every point's index in one pass before any of it starts.
    Needed as a helper (rather than inline `assert`s at each use) because mypy's narrowing doesn't
    reliably follow `.index` through the tuple-indexed point accesses `_link_triangle_topology`
    uses (`nodes[i0]`, etc.), only through a plain local variable like this function's `point`."""
    assert point.index is not None
    return point.index


@dataclass(frozen=True)
class _Triangle:
    """3 points forming a triangle face, plus its signed area (positive when wound
    anticlockwise). Raw construction stores points/area exactly as given, with no winding
    opinion; `from_points` is the normalizing entry point used when first parsing a triangle
    record, and `reversed` is the explicit escape hatch `_link_triangle_topology` uses to force
    the non-canonical winding when two triangles disagree about a shared edge's direction."""

    points: tuple[_Point, _Point, _Point]
    area: float

    def __post_init__(self) -> None:
        """Validates that exactly 3 points were given."""
        if len(self.points) != 3:
            raise ValueError("A triangle must have exactly 3 points")

    @classmethod
    def from_points(cls, points: tuple[_Point, _Point, _Point]) -> _Triangle:
        """Builds a triangle from 3 points in source order, reversed to anticlockwise winding
        (non-negative area) if the source order came out clockwise."""
        p0, p1, p2 = points
        area = (p1.x - p2.x) * (p1.y - p0.y) - (p1.x - p0.x) * (p1.y - p2.y)
        triangle = cls(points, area)
        return triangle if area >= 0 else triangle.reversed()

    def reversed(self) -> _Triangle:
        """Returns a copy with its 2nd/3rd points swapped and area negated (opposite winding)."""
        p0, p1, p2 = self.points
        return _Triangle((p0, p2, p1), -self.area)


def _parse_ascii_trig_source(path: Path) -> tuple[dict[str, str], dict[int, _Point], list[_Triangle], int]:
    """Parses an ASCII trig source's headers, points, and triangles, validating as it goes.
    Returns the raw (not yet uppercased/validated) headers, points keyed by their source id,
    triangles, and the data dimension established by the first point record."""
    headers: dict[str, str] = {}
    points: dict[int, _Point] = {}
    triangles: list[_Triangle] = []
    dimension = -1

    for line in path.read_text(encoding="ascii").splitlines():
        point_match = _POINT_LINE.match(line)
        if point_match:
            source_id = int(point_match.group(1))
            values = point_match.group(4).split()
            if dimension < 0:
                dimension = len(values)
            if dimension == 0 or len(values) != dimension:
                raise ValueError(f"Missing or inconsistent number of data values for point {source_id}")
            if source_id in points:
                raise ValueError(f"Duplicated point id {source_id}")
            x, y = float(point_match.group(2)), float(point_match.group(3))
            points[source_id] = _Point(source_id, x, y, [float(value) for value in values])
            continue

        triangle_match = _TRIANGLE_LINE.match(line)
        if triangle_match:
            point_ids = [int(triangle_match.group(i)) for i in (1, 2, 3)]
            for point_id in point_ids:
                if point_id not in points:
                    raise ValueError(f"Point {point_id} referenced in triangle is not defined")
            node_points = (points[point_ids[0]], points[point_ids[1]], points[point_ids[2]])
            triangles.append(_Triangle.from_points(node_points))
            continue

        header_match = _HEADER_LINE.match(line)
        if header_match:
            headers[header_match.group(1).upper()] = header_match.group(2)

    missing = [key for key in _REQUIRED_ASCII_HEADERS if key not in headers]
    if missing:
        raise ValueError(f"Header record{'s' if len(missing) > 1 else ''} {' '.join(missing)} missing from file")
    if not re.match(r"^\w+$", headers["CRDSYS"]):
        raise ValueError(f"Invalid coordinate system definition {headers['CRDSYS']}")
    headers["CRDSYS"] = headers["CRDSYS"].upper()
    if int(headers["NDIM"]) != dimension:
        raise ValueError("Dimension of data in NDIM header record incompatible with data")

    return headers, points, triangles, dimension


_NODE_TRIPLES = ((2, 0, 1), (0, 1, 2), (1, 2, 0))

# The _NextPointMap and _OppositeNodeMap structures are the two structures every triangle contributes
# to as `build_trig_file` walks the triangle list, and `_build_point_topology` later walks to produce
# each point's final neighbour/opposite arrays. Both are indexed by point *index* (see `_Point.index`),
# and neither is a property of any single point or triangle in isolation - each entry is a fact contributed
# by exactly one triangle, about a directed edge that triangle happens to have.

# next_point[P] is a directed graph over point P's neighbours: an edge `a -> b` means "having
# arrived at P via neighbour a, the next neighbour going anticlockwise around P is b". E.g. the
# triangle (P, a, b) - the same one used in the opposite_node example below - contributes the
# entry next_point[P][a] = b. Every triangle with P as a vertex contributes exactly one such edge
# - a point shared by several triangles ends up with several edges, one per triangle, and walking
# them start-to-end (in `_build_point_topology`) recovers that point's whole neighbour order.
# `_link_triangle_topology`'s own checks guarantee the resulting graph is always either a single
# cycle (an interior point, fully surrounded by triangles) or a single path (a boundary point) -
# never anything more irregular.
_NextPointMap = dict[int, dict[int, int]]

# opposite_node[(a, b)] is the third corner of whichever triangle has `a` and `b` as two of its
# anticlockwise-ordered corners, in that order. It answers a different question than next_point
# does: when `_build_point_topology` walks some point P's neighbours and reaches two consecutive
# ones, `a` then `b`, those three - P, a, b - are one triangle's corners. opposite_node looks up
# any *other* triangle glued to the a-b edge on the far side from P, and returns its third corner
# - or 0 if no such triangle exists, i.e. a-b is on the boundary of the whole mesh. The (P, a, b)
# triangle itself contributes the reverse entry, opposite_node[(b, a)] = P - which is why lookup
# order matters: it's what selects the other triangle sharing that edge (if any), not this one.
_OppositeNodeMap = dict[tuple[int, int], int]


def _link_triangle_topology(triangle: _Triangle, next_point: _NextPointMap, opposite_node: _OppositeNodeMap) -> None:
    """Contributes this triangle's 3 corners to `next_point` and `opposite_node` (see their
    definitions above), reversing the triangle first if any edge is already defined in this
    direction (i.e. the triangles disagree on winding at a shared edge)."""
    nodes = triangle.points
    if any(_index_of(nodes[i1]) in next_point.get(_index_of(nodes[i0]), {}) for i0, i1, _ in _NODE_TRIPLES):
        triangle = triangle.reversed()
        nodes = triangle.points

    for i0, i1, i2 in _NODE_TRIPLES:
        pt0, pt1, pt2 = nodes[i0], nodes[i1], nodes[i2]
        neighbours = next_point.setdefault(_index_of(pt0), {})
        if _index_of(pt1) in neighbours:
            raise ValueError("Invalid triangulation - cannot form consistent triangle node order")
        neighbours[_index_of(pt1)] = _index_of(pt2)
        opposite_node[(_index_of(pt2), _index_of(pt1))] = _index_of(pt0)


def _build_point_topology(
    point: _Point, next_point: _NextPointMap, opposite_node: _OppositeNodeMap
) -> tuple[list[int], list[int]]:
    """Walks `point`'s neighbour graph (`next_point[point.index]`, see above) starting from its
    unique node with no incoming edge (or an arbitrary neighbour if the point is fully surrounded,
    i.e. every neighbour has one), returning the neighbours in anticlockwise order and - via
    `opposite_node` - the third corner of any other triangle sharing each consecutive pair's edge
    (0 where there isn't one, i.e. that edge is on the boundary of the mesh). A boundary
    (non-fully-surrounded) point's graph is a path rather than a cycle, so the walk ends at a dead
    end rather than closing back on its start, recorded here as a trailing `0` sentinel in both
    arrays."""
    assert point.index is not None
    neighbours = next_point.get(point.index, {})
    neighbour_count = len(neighbours)
    if neighbour_count == 0:
        raise ValueError(f"Invalid triangulation: node {point.source_id} is not used in any triangulation")

    incoming = {next_id: node_id for node_id, next_id in neighbours.items()}
    start_candidates = [node_id for node_id in neighbours if node_id not in incoming]
    if len(start_candidates) > 1:
        raise ValueError(f"Triangulation too complex at node {point.source_id}")
    start = start_candidates[0] if start_candidates else next(iter(neighbours))

    nodes = [start]
    opposite: list[int] = []
    previous_neighbour = start
    next_neighbour = neighbours[start]
    while True:
        nodes.append(next_neighbour)
        opposite.append(opposite_node.get((previous_neighbour, next_neighbour), 0))
        previous_neighbour = next_neighbour
        next_neighbour = neighbours.get(previous_neighbour, 0)
        if next_neighbour == start or not previous_neighbour:
            break
    opposite.append(opposite_node.get((previous_neighbour, start), 0))

    if not next_neighbour:
        neighbour_count += 2
    if len(nodes) != neighbour_count or len(opposite) != neighbour_count:
        raise AssertionError(f"Error in algorithm - node {point.source_id}!")
    return nodes, opposite


def build_trig_file(input_path: Path, output_path: Path, big_endian: bool | None = None) -> None:
    """Builds a binary TRIG2L/TRIG2B triangulation file from an ASCII trig source. `big_endian=None`
    uses the source's own `FORMAT` header; an explicit `True`/`False` overrides it."""
    headers, points_by_source_id, triangles, dimension = _parse_ascii_trig_source(input_path)

    if big_endian is not None:
        fmt = TrigFormat.TRIG2B if big_endian else TrigFormat.TRIG2L
    else:
        fmt = parse_trig_format(headers["FORMAT"])

    points = sorted(points_by_source_id.values(), key=lambda point: point.x)
    for new_index, point in enumerate(points, start=1):
        point.index = new_index

    triangles.sort(key=lambda triangle: triangle.area, reverse=True)
    next_point: _NextPointMap = {}
    opposite_node: _OppositeNodeMap = {}
    for triangle in triangles:
        _link_triangle_topology(triangle, next_point, opposite_node)
    topologies = [_build_point_topology(point, next_point, opposite_node) for point in points]

    arrindex = 0
    offsets = []
    for nodes, _ in topologies:
        offsets.append(arrindex)
        arrindex += 1 + 2 * len(nodes)

    try:
        with output_path.open("wb") as file:
            _write_trig_file(file, fmt, headers, points, topologies, offsets, arrindex, dimension)
    except Exception:
        output_path.unlink(missing_ok=True)
        raise


def _write_trig_file(  # pylint: disable=too-many-arguments, too-many-positional-arguments
    file: BinaryIO,
    fmt: TrigFormat,
    headers: dict[str, str],
    points: list[_Point],
    topologies: list[tuple[list[int], list[int]]],
    offsets: list[int],
    arrindex: int,
    dimension: int,
) -> None:
    """Writes the binary signature, header, point, and topology data built by `build_trig_file`."""
    codec = EndianCodec(fmt.big_endian)
    file.write(fmt.signature)
    file.write(codec.pack_string([headers["HEADER0"], headers["HEADER1"], headers["HEADER2"], headers["CRDSYS"]]))
    file.write(
        codec.pack_double(
            [
                min(point.y for point in points),
                max(point.y for point in points),
                min(point.x for point in points),
                max(point.x for point in points),
            ]
        )
    )
    file.write(codec.pack_short([len(points), dimension]))
    file.write(codec.pack_long([arrindex]))
    for point in points:
        file.write(codec.pack_double([point.x, point.y]))
    for point in points:
        file.write(codec.pack_double(point.data))
    for offset in offsets:
        file.write(codec.pack_long([offset]))
    for nodes, opposite in topologies:
        file.write(codec.pack_short([len(nodes)]))
        file.write(codec.pack_short(nodes))
        file.write(codec.pack_short(opposite))
