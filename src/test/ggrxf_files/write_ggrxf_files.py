#!/usr/bin/env python3
"""Writes the small GGRXF files that the tests of the GGRXF reader and of the GGRXF deformation of coordsys.def read.

Usage: write_ggrxf_files.py OUTPUT_DIR

Each file is one case. Its name says what it is, and the tests (src/test/ggrxf_reader_*_test and
src/test/crdsys_rfdef_ggrxf_test) refer to the files by name, with the values that they expect written out by hand.
The grids are tiny, so that the values can be worked out by hand. The node (i, j) of a scalar grid has the raw value
offset + 100 * (i + 1) + 10 * (j + 1) + (axis + 1), which shows which node and which axis it is from, and is never 0.

THIS SHOULD FORM THE BASIS OF A REAL GGRXF WRITER IN FUTURE. The file layout here is the one that the format document
(src/help/help/coordsys/ggrxffile.html) specifies. It is not a real writer, though, in two ways. It does not validate
what it is given, as a real writer should, because the reader tests need files that are wrong in one way (a root grid
with the wrong priority, an array that is too short). And it can leave out a layer's variable, or replace one with an
array of any shape and type, for the same reason. A real writer would keep the validation, and would not have these
two.
"""

import argparse
from collections.abc import Callable, Sequence
from dataclasses import dataclass, field, replace
from pathlib import Path
from typing import Any

import netCDF4
import numpy as np
from numpy.typing import NDArray

# The two WKT strings that the reader tells apart. It looks only at the order of the two axis names.
LATITUDE_FIRST_WKT = (
    'GEOGCRS["test",AXIS["geodetic latitude (Lat)",north,ORDER[1]],AXIS["geodetic longitude (Lon)",east,ORDER[2]]]'
)
LONGITUDE_FIRST_WKT = (
    'GEOGCRS["test",AXIS["geodetic longitude (Lon)",east,ORDER[1]],AXIS["geodetic latitude (Lat)",north,ORDER[2]]]'
)

GGRXF_CONVENTIONS = "GGRXF-1.0"
GGRXF_CONTENT = "deformationModel"
PARAMETER_NAMES = ("displacementEast", "displacementNorth", "displacementUp")

# The value of parent_grid_index for a root grid of a ragged layer.
RAGGED_ROOT_GRID_SENTINEL = 4294967295

ENU_COMPONENTS = ("e", "n", "u")

SEGMENT_DTYPE = np.dtype([("segment_start_value", "<i4"), ("day_delta", "<u2"), ("segment_scale_factor", "<i2")])


@dataclass
class TimeFunctionEntry:
    """One entry of the timeFunctions of a layer of scalar grids."""

    function_type: str
    scale_factor: float
    function_reference_date: str | None = None
    start_date: str | None = None
    end_date: str | None = None
    event_date: str | None = None


@dataclass
class ScalarGrid:
    """A grid of a scalar_with_time_function layer. The shape of displacement is written as it is, to be wrong."""

    i_node_count: int
    j_node_count: int
    affine_coeffs: Sequence[float]
    displacement: NDArray[Any]
    scale_factor: float = 1.0
    missing_value: int | None = None
    grid_priority: int | None = None
    has_end_day: bool = False
    name: str | None = None  # the name of its group, which is grid_<index> if it is None
    child_grids: list["ScalarGrid"] = field(default_factory=list)


@dataclass
class RaggedSegment:
    """One segment of one node on one axis of a ragged grid."""

    segment_start_value: int
    day_delta: int
    segment_scale_factor: int
    function_type: int = 0


@dataclass
class RaggedGrid:
    """A grid of a ragged_segments layer. segments[axis][node] is the segments of a node on an axis (e, n or u)."""

    i_node_count: int
    j_node_count: int
    affine_coeffs: Sequence[float]
    reference_day: NDArray[Any]
    segments: dict[str, list[list[RaggedSegment]]]
    grid_priority: int | None = None
    child_grids: list["RaggedGrid"] = field(default_factory=list)


@dataclass
class Layer:  # pylint: disable=too-many-instance-attributes
    """One layer, which is one group of the file. Its grids are all scalar, or all ragged."""

    description: str
    grids: Sequence[ScalarGrid | RaggedGrid]
    time_functions: list[TimeFunctionEntry] | None = None
    grid_parameters: list[str] | None = None
    reference_date: str | None = None
    interpolation_method: str = "bilinear"
    grid_type: str | None = None  # what to write as grid_type instead of what the grids are
    has_end_day: Sequence[int] | None = None  # the has_end_day of every node of a ragged layer
    omit: frozenset[str] = frozenset()  # the variables of a ragged layer to leave out
    overrides: dict[str, NDArray[Any]] = field(default_factory=dict)  # the variables of a ragged layer to replace


def write_ggrxf_file(
    path: Path,
    layers: Sequence[Layer],
    *,
    interpolation_crs_wkt: str | None = LATITUDE_FIRST_WKT,
    model_version: str | None = None,
) -> None:
    """Writes a file of layers. A layer is the group layer_<index>."""
    with netCDF4.Dataset(path, mode="w", format="NETCDF4") as root:
        root.setncattr("Conventions", GGRXF_CONVENTIONS)
        root.setncattr("content", GGRXF_CONTENT)
        if interpolation_crs_wkt is not None:
            root.setncattr("interpolationCrsWkt", interpolation_crs_wkt)
        if model_version is not None:
            root.setncattr("modelVersion", model_version)
        write_parameters(root)
        segment_type = root.createCompoundType(SEGMENT_DTYPE, "segment_record")
        for index, layer in enumerate(layers):
            write_layer(root.createGroup(f"layer_{index}"), layer, segment_type)


def write_parameters(root: netCDF4.Dataset) -> None:
    """Writes the parameters of the file, as separate attributes with dotted names."""
    root.setncattr("parameters.count", np.uint8(len(PARAMETER_NAMES)))
    for index, name in enumerate(PARAMETER_NAMES):
        prefix = f"parameters.{index}."
        root.setncattr(prefix + "parameterName", name)
        root.setncattr(prefix + "parameterSet", "displacement")
        root.setncattr(prefix + "unitName", "millimetre")
        root.setncattr(prefix + "unitSiRatio", np.float64(0.001))
        root.setncattr(prefix + "noDataFlag", np.float64(2147483647))


def write_layer_attributes(group: netCDF4.Group, layer: Layer) -> None:
    """Writes the attributes of a layer that do not depend on its grids."""
    group.setncattr("description", layer.description)
    group.setncattr("interpolationMethod", layer.interpolation_method)
    if layer.reference_date is not None:
        group.setncattr("reference_date", layer.reference_date)
    if layer.time_functions is not None:
        write_time_functions(group, layer.time_functions)
    if layer.grid_parameters is not None:
        group.setncattr("gridParameters", list(layer.grid_parameters))


def write_layer(group: netCDF4.Group, layer: Layer, segment_type: netCDF4.CompoundType) -> None:
    """Writes the attributes of a layer, and then its grids."""
    write_layer_attributes(group, layer)
    ragged = [grid for grid in layer.grids if isinstance(grid, RaggedGrid)]
    scalar = [grid for grid in layer.grids if isinstance(grid, ScalarGrid)]
    default_type = "ragged_segments" if ragged else "scalar_with_time_function"
    group.setncattr("grid_type", layer.grid_type or default_type)
    if ragged:
        write_ragged_layer(group, layer, ragged, segment_type)
    for index, grid in enumerate(scalar):
        write_scalar_grid(group.createGroup(grid.name or f"grid_{index}"), grid)


def write_time_functions(group: netCDF4.Group, time_functions: Sequence[TimeFunctionEntry]) -> None:
    """Writes the timeFunctions of a layer, as separate attributes with dotted names."""
    group.setncattr("timeFunctions.count", np.uint8(len(time_functions)))
    for index, entry in enumerate(time_functions):
        prefix = f"timeFunctions.{index}."
        group.setncattr(prefix + "functionType", entry.function_type)
        group.setncattr(prefix + "scaleFactor", np.float64(entry.scale_factor))
        for name, date in (
            ("functionReferenceDate", entry.function_reference_date),
            ("startDate", entry.start_date),
            ("endDate", entry.end_date),
            ("eventDate", entry.event_date),
        ):
            if date is not None:
                group.setncattr(prefix + name, date)


def write_scalar_grid(group: netCDF4.Group, grid: ScalarGrid) -> None:
    """Writes a grid of scalar values, and then its child grids, which are groups inside it."""
    group.setncattr("affineCoeffs", np.array(grid.affine_coeffs, dtype=np.float64))
    group.setncattr("iNodeCount", np.uint32(grid.i_node_count))
    group.setncattr("jNodeCount", np.uint32(grid.j_node_count))
    if grid.grid_priority is not None:
        group.setncattr("gridPriority", np.uint8(grid.grid_priority))

    dimension_names = ("i", "j", "displacementCount")[: grid.displacement.ndim]
    for dimension_name, size in zip(dimension_names, grid.displacement.shape):
        group.createDimension(dimension_name, size)
    displacement = group.createVariable("displacement", "i4", dimension_names)
    displacement.set_auto_maskandscale(False)
    displacement.setncattr("scale_factor", np.float64(grid.scale_factor))
    if grid.missing_value is not None:
        displacement.setncattr("missing_value", np.int32(grid.missing_value))
    displacement[...] = grid.displacement.astype(np.int32)

    if grid.has_end_day:
        has_end_day = group.createVariable("has_end_day", "u1")
        has_end_day.set_auto_maskandscale(False)
        has_end_day.assignValue(1)
    for index, child in enumerate(grid.child_grids):
        write_scalar_grid(group.createGroup(child.name or f"grid_{index}"), child)


def flatten_ragged_grids(root_grids: Sequence[RaggedGrid]) -> list[tuple[RaggedGrid, int]]:
    """Every grid and its parent's position, in the order of the arrays: a grid and then the grids inside it."""
    flat: list[tuple[RaggedGrid, int]] = []

    def visit(grid: RaggedGrid, parent_index: int) -> None:
        own_index = len(flat)
        flat.append((grid, parent_index))
        for child in grid.child_grids:
            visit(child, own_index)

    for root in root_grids:
        visit(root, RAGGED_ROOT_GRID_SENTINEL)
    return flat


def write_ragged_variable(  # pylint: disable=too-many-arguments,too-many-positional-arguments
    group: netCDF4.Group,
    layer: Layer,
    name: str,
    nc_type: Any,
    array: NDArray[Any],
    dimension_names: tuple[str, ...],
) -> None:
    """Writes a variable of a ragged layer, unless the layer leaves it out. If the layer replaces it, the array that
    replaces it is written, with dimensions of its own."""
    if name in layer.omit:
        return
    if name in layer.overrides:
        array = layer.overrides[name]
        dimension_names = tuple(f"{name}_dimension_{axis}" for axis in range(array.ndim))
    for dimension_name, size in zip(dimension_names, array.shape):
        if dimension_name not in group.dimensions:
            group.createDimension(dimension_name, size)
    variable = group.createVariable(name, nc_type, dimension_names)
    variable.set_auto_maskandscale(False)
    variable[...] = array


def write_ragged_grid_arrays(put: Callable[..., None], flat: Sequence[tuple[RaggedGrid, int]], layer: Layer) -> None:
    """Writes the arrays of a ragged layer that have a value for each grid, or for each node of all of them, other than
    the segments."""
    put("affineCoeffs", "f8", np.array([grid.affine_coeffs for grid, _ in flat], dtype=np.float64), "grid", "six")
    put("iNodeCount", "u4", np.array([grid.i_node_count for grid, _ in flat], dtype=np.uint32), "grid")
    put("jNodeCount", "u4", np.array([grid.j_node_count for grid, _ in flat], dtype=np.uint32), "grid")
    priorities = [0 if grid.grid_priority is None else grid.grid_priority for grid, _ in flat]
    put("gridPriority", "u1", np.array(priorities, dtype=np.uint8), "grid")
    put("parent_grid_index", "u4", np.array([parent for _, parent in flat], dtype=np.uint32), "grid")
    reference_day = np.concatenate([grid.reference_day for grid, _ in flat]).astype(np.uint32)
    put("reference_day", "u4", reference_day, "node")
    if layer.has_end_day is not None:
        put("has_end_day", "u1", np.array(layer.has_end_day, dtype=np.uint8), "node")


def ragged_segment_arrays(
    flat: Sequence[tuple[RaggedGrid, int]], component: str
) -> tuple[NDArray[np.uint16], NDArray[Any], NDArray[np.uint8]]:
    """The arrays of the segments on one axis of all the grids: the number of segments of each node, the records of all
    the segments, and the function type of each of them."""
    node_segments = [segments for grid, _ in flat for segments in grid.segments[component]]
    counts = np.array([len(segments) for segments in node_segments], dtype=np.uint16)
    records = np.zeros(int(counts.sum()), dtype=SEGMENT_DTYPE)
    function_types = np.zeros(len(records), dtype=np.uint8)
    position = 0
    for segments in node_segments:
        for segment in segments:
            records[position] = (segment.segment_start_value, segment.day_delta, segment.segment_scale_factor)
            function_types[position] = segment.function_type
            position += 1
    return counts, records, function_types


def write_ragged_layer(
    group: netCDF4.Group, layer: Layer, root_grids: Sequence[RaggedGrid], segment_type: netCDF4.CompoundType
) -> None:
    """Writes the grids of a ragged layer as arrays that have a value for each grid, or for each node of all of them."""
    flat = flatten_ragged_grids(root_grids)

    def put(name: str, nc_type: Any, array: NDArray[Any], *dimension_names: str) -> None:
        write_ragged_variable(group, layer, name, nc_type, array, dimension_names)

    write_ragged_grid_arrays(put, flat, layer)
    for component in flat[0][0].segments:
        counts, records, function_types = ragged_segment_arrays(flat, component)
        records_dimension = f"segment_records_{component}"
        put(f"segment_count_{component}", "u2", counts, "node")
        put(f"segments_{component}", segment_type, records, records_dimension)
        put(f"function_type_{component}", "u1", function_types, records_dimension)


# ----- the cases -------------------------------------------------------------------------------------------------

# Node (i, j) of every grid below is at latitude origin[0] + i and longitude origin[1] + j, in degrees.
ORIGIN = (10.0, 20.0)

# A layer of scalar grids has this one time function unless it is given others: 0 before 2000-01-01, and 1 from then on.
STEP_2000 = TimeFunctionEntry("step", 1.0, event_date="2000-01-01")


def raw_displacement(i_count: int, j_count: int, axis_count: int, offset: int = 0) -> NDArray[np.int32]:
    """The raw values of a scalar grid: offset + 100 * (i + 1) + 10 * (j + 1) + (axis + 1)."""
    i, j, axis = np.indices((i_count, j_count, axis_count))
    return np.asarray(offset + 100 * (i + 1) + 10 * (j + 1) + (axis + 1), dtype=np.int32)


def scalar_grid(count: int = 3, origin: tuple[float, float] = ORIGIN, **fields: Any) -> ScalarGrid:
    """A square scalar grid of count by count nodes, one degree apart, with the displacement of raw_displacement()."""
    values: dict[str, Any] = {
        "i_node_count": count,
        "j_node_count": count,
        "affine_coeffs": (origin[0], 1.0, 0.0, origin[1], 0.0, 1.0),
        "displacement": raw_displacement(count, count, 3, int(fields.pop("offset", 0))),
        "scale_factor": 0.5,
    }
    values.update(fields)
    return ScalarGrid(**values)


def scalar_layer(*grids: ScalarGrid, **fields: Any) -> Layer:
    """A layer of scalar grids, with the one time function STEP_2000 unless it is given others."""
    values: dict[str, Any] = {"description": "scalar", "time_functions": [STEP_2000]}
    values.update(fields)
    return Layer(grids=list(grids), **values)


def ragged_segments() -> dict[str, list[list[RaggedSegment]]]:
    """The segments of every node of a ragged grid of 9 nodes: one on the first axis, two on the second, and none on the
    third."""
    return {
        "e": [[RaggedSegment(1000, 0, 365)] for _ in range(9)],
        "n": [[RaggedSegment(2000, 0, 0), RaggedSegment(3000, 100, 0)] for _ in range(9)],
        "u": [[] for _ in range(9)],
    }


def ragged_grid(origin: tuple[float, float] = ORIGIN, **fields: Any) -> RaggedGrid:
    """A ragged grid of 3 by 3 nodes, one degree apart, whose nodes have the reference day 0 and ragged_segments()."""
    values: dict[str, Any] = {
        "i_node_count": 3,
        "j_node_count": 3,
        "affine_coeffs": (origin[0], 1.0, 0.0, origin[1], 0.0, 1.0),
        "reference_day": np.zeros(9, dtype=np.uint32),
        "segments": ragged_segments(),
    }
    values.update(fields)
    return RaggedGrid(**values)


def ragged_layer(*grids: RaggedGrid, **fields: Any) -> Layer:
    """A layer of ragged grids, whose reference date is 2000-01-01."""
    values: dict[str, Any] = {"description": "ragged", "reference_date": "2000-01-01"}
    values.update(fields)
    return Layer(grids=list(grids), **values)


def write_file_cases(directory: Path) -> None:
    """The files that are wrong or unusual as a whole, and the one of each type of layer that is right."""
    write_ggrxf_file(directory / "scalar_basic.nc", [scalar_layer(scalar_grid())], model_version="1.2.3")
    write_ggrxf_file(directory / "scalar_no_model_version.nc", [scalar_layer(scalar_grid())])
    write_ggrxf_file(directory / "no_layers.nc", [])
    write_ggrxf_file(directory / "no_wkt.nc", [scalar_layer(scalar_grid())], interpolation_crs_wkt=None)
    latitude_only = 'GEOGCRS["test",AXIS["geodetic latitude (Lat)",north,ORDER[1]]]'
    write_ggrxf_file(
        directory / "wkt_no_longitude.nc", [scalar_layer(scalar_grid())], interpolation_crs_wkt=latitude_only
    )
    longitude_only = 'GEOGCRS["test",AXIS["geodetic longitude (Lon)",east,ORDER[1]]]'
    write_ggrxf_file(
        directory / "wkt_no_latitude.nc", [scalar_layer(scalar_grid())], interpolation_crs_wkt=longitude_only
    )

    # The grid of scalar_basic.nc and ragged_basic.nc, with its coefficients in the order of longitude, then latitude.
    swapped = (ORIGIN[1], 0.0, 1.0, ORIGIN[0], 1.0, 0.0)
    write_ggrxf_file(
        directory / "scalar_longitude_first.nc",
        [scalar_layer(scalar_grid(affine_coeffs=swapped))],
        interpolation_crs_wkt=LONGITUDE_FIRST_WKT,
    )
    write_ggrxf_file(
        directory / "ragged_longitude_first.nc",
        [ragged_layer(ragged_grid(affine_coeffs=swapped))],
        interpolation_crs_wkt=LONGITUDE_FIRST_WKT,
    )
    write_ggrxf_file(directory / "mixed_layers.nc", [scalar_layer(scalar_grid()), ragged_layer(ragged_grid())])
    write_ggrxf_file(directory / "ragged_basic.nc", [ragged_layer(ragged_grid())])
    write_ggrxf_file(directory / "layer_bicubic.nc", [scalar_layer(scalar_grid(), interpolation_method="bicubic")])
    write_ggrxf_file(directory / "layer_unknown_grid_type.nc", [scalar_layer(scalar_grid(), grid_type="sparse_points")])


def write_time_function_cases(directory: Path) -> None:
    """A layer with each of the time functions, and a layer with one that is not implemented."""
    three = [
        TimeFunctionEntry("linear", 0.25, function_reference_date="2000-01-01"),
        TimeFunctionEntry("step", 2.0, event_date="2001-01-01"),
        TimeFunctionEntry("ramp", 3.0, start_date="2002-01-01", end_date="2002-01-05"),
    ]
    write_ggrxf_file(directory / "scalar_time_functions.nc", [scalar_layer(scalar_grid(), time_functions=three)])
    quadratic = [TimeFunctionEntry("quadratic", 1.0, function_reference_date="2000-01-01")]
    write_ggrxf_file(
        directory / "scalar_time_function_quadratic.nc", [scalar_layer(scalar_grid(), time_functions=quadratic)]
    )


def write_grid_parameter_cases(directory: Path) -> None:
    """Layers that list some of the displacements, in an order, and layers that list them wrongly."""
    up = scalar_grid(displacement=raw_displacement(3, 3, 1))
    write_ggrxf_file(directory / "scalar_parameters_up.nc", [scalar_layer(up, grid_parameters=["displacementUp"])])
    up_east = scalar_grid(displacement=raw_displacement(3, 3, 2))
    write_ggrxf_file(
        directory / "scalar_parameters_up_east.nc",
        [scalar_layer(up_east, grid_parameters=["displacementUp", "displacementEast"])],
    )
    write_ggrxf_file(
        directory / "scalar_parameters_unknown.nc",
        [scalar_layer(up, grid_parameters=["displacementWest"])],
    )
    write_ggrxf_file(
        directory / "scalar_parameters_count_mismatch.nc",
        [scalar_layer(scalar_grid(), grid_parameters=["displacementEast", "displacementNorth"])],
    )


def write_scalar_grid_cases(directory: Path) -> None:
    """Grids that have a missing value, or an end day, or a shape that is wrong."""
    # The node (2, 2) is a corner of the grid, so it is in one of the four cells, and the others have all their values.
    missing = scalar_grid(missing_value=-999)
    missing.displacement[2, 2, 0] = -999
    write_ggrxf_file(directory / "scalar_missing_value.nc", [scalar_layer(missing)])
    write_ggrxf_file(directory / "scalar_end_day.nc", [scalar_layer(scalar_grid(has_end_day=True))])
    write_ggrxf_file(
        directory / "scalar_affine_coeffs_length.nc",
        [scalar_layer(scalar_grid(affine_coeffs=(ORIGIN[0], 1.0, 0.0, ORIGIN[1], 0.0)))],
    )
    write_ggrxf_file(
        directory / "scalar_displacement_dimensions.nc",
        [scalar_layer(scalar_grid(displacement=raw_displacement(3, 3, 1)[:, :, 0]))],
    )
    write_ggrxf_file(
        directory / "scalar_displacement_nodes.nc",
        [scalar_layer(scalar_grid(displacement=raw_displacement(2, 3, 3)))],
    )
    write_ggrxf_file(
        directory / "scalar_displacement_axes.nc",
        [scalar_layer(scalar_grid(displacement=raw_displacement(3, 3, 2)))],
    )


def write_scalar_order_cases(directory: Path) -> None:
    """Root grids whose names are numbers, and ones whose names are not."""
    # Both cover every point of the other. By the number in its name grid_2 comes first, but by its text grid_10 does,
    # and the group of grid_10 is written first.
    second = scalar_grid(offset=0, name="grid_2")
    tenth = scalar_grid(offset=1000, name="grid_10")
    write_ggrxf_file(directory / "scalar_file_order.nc", [scalar_layer(tenth, second)])
    write_ggrxf_file(directory / "scalar_group_name.nc", [scalar_layer(scalar_grid(name="first"))])
    write_ggrxf_file(directory / "scalar_group_number.nc", [scalar_layer(scalar_grid(name="grid_x"))])


def scalar_nest(**root_fields: Any) -> ScalarGrid:
    """A root grid that covers [10, 14] by [20, 24], with two grids of higher resolution inside it: a (priority 1) that
    covers [11, 13] by [21, 23], and b (priority 2) that covers [12, 14] by [22, 24], so a and b overlap on [12, 13]
    by [22, 23]. Each has a different offset in its values."""
    inner_a = scalar_grid(3, (11.0, 21.0), offset=1000, grid_priority=1)
    inner_b = scalar_grid(3, (12.0, 22.0), offset=2000, grid_priority=2)
    return scalar_grid(5, grid_priority=0, child_grids=[inner_a, inner_b], **root_fields)


def write_scalar_nesting_cases(directory: Path) -> None:
    """A nest of grids that is right, and nests that are wrong in each way their priorities can be."""
    write_ggrxf_file(directory / "scalar_nested.nc", [scalar_layer(scalar_nest())])

    def wrong(name: str, root: ScalarGrid) -> None:
        write_ggrxf_file(directory / f"scalar_nest_{name}.nc", [scalar_layer(root)])

    wrong("root_no_priority", replace(scalar_nest(), grid_priority=None))
    wrong("root_priority_1", replace(scalar_nest(), grid_priority=1))
    wrong("leaf_has_priority", scalar_grid(grid_priority=0))
    one_child = scalar_grid(5, grid_priority=0, child_grids=[scalar_grid(3, (11.0, 21.0), grid_priority=1)])
    wrong("child_no_priority", replace(one_child, child_grids=[scalar_grid(3, (11.0, 21.0))]))
    wrong("child_priority_0", replace(one_child, child_grids=[scalar_grid(3, (11.0, 21.0), grid_priority=0)]))
    two = [scalar_grid(3, (11.0, 21.0), grid_priority=1), scalar_grid(3, (12.0, 22.0), grid_priority=3)]
    wrong("child_priority_range", replace(scalar_nest(), child_grids=two))
    twice = [scalar_grid(3, (11.0, 21.0), grid_priority=1), scalar_grid(3, (12.0, 22.0), grid_priority=1)]
    wrong("child_priority_twice", replace(scalar_nest(), child_grids=twice))


def write_ragged_cases(directory: Path) -> None:
    """Ragged layers of two axes, nests of grids, a function type that is not implemented, an end day, and nodes that
    have different reference days."""
    two_axes = ragged_segments()
    del two_axes["u"]
    write_ggrxf_file(directory / "ragged_two_axes.nc", [ragged_layer(ragged_grid(segments=two_axes))])
    write_ggrxf_file(
        directory / "ragged_no_segment_count_e.nc",
        [ragged_layer(ragged_grid(), omit=frozenset({"segment_count_e"}))],
    )
    write_ggrxf_file(
        directory / "ragged_no_segment_count_n.nc",
        [ragged_layer(ragged_grid(), omit=frozenset({"segment_count_n"}))],
    )
    code_1 = ragged_segments()
    code_1["e"][0] = [RaggedSegment(1000, 0, 365, function_type=1)]
    write_ggrxf_file(directory / "ragged_function_type.nc", [ragged_layer(ragged_grid(segments=code_1))])
    write_ggrxf_file(
        directory / "ragged_end_day.nc",
        [ragged_layer(ragged_grid(), has_end_day=[0, 0, 0, 0, 1, 0, 0, 0, 0])],
    )
    write_ggrxf_file(
        directory / "ragged_end_day_none.nc",
        [ragged_layer(ragged_grid(), has_end_day=[0] * 9)],
    )

    # Node (i, j) of this grid is node 3 * i + j. Every node has one constant segment on the east axis, of the raw start
    # value 1000 and the day delta 100, and the four nodes of the first cell have the reference days 0, 10, 20 and 30,
    # so that the segments of the nodes start on the days 100, 110, 120 and 130.
    reference_days = np.zeros(9, dtype=np.uint32)
    reference_days[[0, 1, 3, 4]] = [0, 10, 20, 30]
    reference_day_segments = ragged_segments()
    reference_day_segments["e"] = [[RaggedSegment(1000, 100, 0)] for _ in range(9)]
    reference_day_segments["n"] = [[] for _ in range(9)]
    write_ggrxf_file(
        directory / "ragged_node_reference_days.nc",
        [ragged_layer(ragged_grid(reference_day=reference_days, segments=reference_day_segments))],
    )


def ragged_nest(**root_fields: Any) -> RaggedGrid:
    """The grids of scalar_nest(), as ragged grids, with the segments of ragged_segments() in every one of them, but a
    different start value for the first segment on the first axis of each, so that a value shows which grid it is
    from."""

    def inner(origin: tuple[float, float], start_value: int, priority: int) -> RaggedGrid:
        segments = ragged_segments()
        segments["e"] = [[RaggedSegment(start_value, 0, 0)] for _ in range(9)]
        return ragged_grid(origin, segments=segments, grid_priority=priority)

    children = [inner((11.0, 21.0), 11000, 1), inner((12.0, 22.0), 22000, 2)]
    nodes = 25
    segments: dict[str, list[list[RaggedSegment]]] = {axis: [[] for _ in range(nodes)] for axis in ENU_COMPONENTS}
    segments["e"] = [[RaggedSegment(100, 0, 0)] for _ in range(nodes)]
    return ragged_grid(
        ORIGIN,
        i_node_count=5,
        j_node_count=5,
        reference_day=np.zeros(nodes, dtype=np.uint32),
        segments=segments,
        grid_priority=0,
        child_grids=children,
        **root_fields,
    )


def write_ragged_nesting_cases(directory: Path) -> None:
    """A nest of ragged grids that is right, and nests that are wrong in each way their priorities can be, and a grid
    that no root reaches."""
    write_ggrxf_file(directory / "ragged_nested.nc", [ragged_layer(ragged_nest(), ragged_grid((30.0, 40.0)))])

    def wrong(name: str, root: RaggedGrid, **layer_fields: Any) -> None:
        write_ggrxf_file(directory / f"ragged_nest_{name}.nc", [ragged_layer(root, **layer_fields)])

    wrong("root_priority_1", replace(ragged_nest(), grid_priority=1))
    first, second = ragged_nest().child_grids
    wrong("child_priority_0", replace(ragged_nest(), child_grids=[replace(first, grid_priority=0), second]))
    wrong("child_priority_range", replace(ragged_nest(), child_grids=[first, replace(second, grid_priority=3)]))
    wrong("child_priority_twice", replace(ragged_nest(), child_grids=[first, replace(second, grid_priority=1)]))
    # Grids 1 and 2 have each other as parents, so no root reaches them.
    cycle = np.array([RAGGED_ROOT_GRID_SENTINEL, 2, 1], dtype=np.uint32)
    wrong("unreached", ragged_nest(), overrides={"parent_grid_index": cycle})


def write_ragged_length_cases(directory: Path) -> None:
    """Ragged layers with one array that is the wrong length, for each array that has to agree with the others."""
    nodes = 9  # a grid of 3 by 3 nodes, and the segments of ragged_segments() are 9, 18 and 0 in all
    short = {
        "jNodeCount": np.zeros(2, dtype=np.uint32),
        "gridPriority": np.zeros(2, dtype=np.uint8),
        "parent_grid_index": np.zeros(2, dtype=np.uint32),
        "affineCoeffs": np.zeros((1, 5), dtype=np.float64),
        "reference_day": np.zeros(nodes - 1, dtype=np.uint32),
        "has_end_day": np.zeros(nodes - 1, dtype=np.uint8),
        "segment_count_e": np.zeros(nodes - 1, dtype=np.uint16),
        "segments_e": np.zeros(nodes - 1, dtype=SEGMENT_DTYPE),
        "function_type_e": np.zeros(nodes - 1, dtype=np.uint8),
    }
    for name, array in short.items():
        layer = ragged_layer(ragged_grid(), has_end_day=[0] * nodes, overrides={name: array})
        write_ggrxf_file(directory / f"ragged_length_{name}.nc", [layer])


def main() -> None:
    """Writes every case into the directory that is given."""
    parser = argparse.ArgumentParser(description="Write the GGRXF files that the tests read.")
    parser.add_argument("directory", type=Path, help="where to write the files, which is made if it is not there")
    directory = parser.parse_args().directory
    directory.mkdir(parents=True, exist_ok=True)
    write_file_cases(directory)
    write_time_function_cases(directory)
    write_grid_parameter_cases(directory)
    write_scalar_grid_cases(directory)
    write_scalar_order_cases(directory)
    write_scalar_nesting_cases(directory)
    write_ragged_cases(directory)
    write_ragged_nesting_cases(directory)
    write_ragged_length_cases(directory)


if __name__ == "__main__":
    main()
