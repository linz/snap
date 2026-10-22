// Standalone unit test for the netCDF wrapper (deformation/netcdf_wrapper.h), which reads the files of the GGRXF format
// through the netCDF-C library: the checked scalar reads (readNumericScalarAttribute() of a group and of a variable,
// and readScalarValue() of a variable, which must give the value when there is exactly one, and throw when there are
// none or several, instead of reading out of bounds or ignoring all but the first), and the rest of what the reader
// uses: opening a file, groups, text, string list and numeric attributes, variables of each type and their dimensions,
// and the fields of a compound variable. Only the types that the format has are used, as the wrapper only has those.
// The test writes small netCDF files with the netCDF-C library, reads them back through the wrapper and deletes them.
// Failures are reported to stdout and the process exit code is the pass/fail signal (0 = all passed), as for the other
// test-only tools in src/test/.

#include <netcdf.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "deformation/netcdf_wrapper.h"
#include "test_support.h"

namespace {

using snap_test::check;
using snap_test::checkThrows;

/// Stops the test if a netCDF-C call that writes the test file failed, since nothing after it would mean anything.
void require(const int status, const std::string& call) {
    if (status != NC_NOERR) {
        throw std::runtime_error("netCDF-C call " + call + " failed: " + nc_strerror(status));
    }
}

/// Writes the file that the checks read: on its root group, attributes that hold 1, 2 and 0 values, and a uint8
/// attribute; and variables that hold 1, 2 and 0 values.
void writeTestFile(const std::string& path) {
    int ncid = 0;
    require(nc_create(path.c_str(), NC_NETCDF4 | NC_CLOBBER, &ncid), "nc_create");

    const double oneValue = 2.5;
    const double twoValues[2] = {1.0, 2.0};
    const std::uint8_t flag = 7;
    require(nc_put_att_double(ncid, NC_GLOBAL, "oneValue", NC_DOUBLE, 1, &oneValue), "nc_put_att_double one");
    require(nc_put_att_double(ncid, NC_GLOBAL, "twoValues", NC_DOUBLE, 2, twoValues), "nc_put_att_double two");
    require(nc_put_att_double(ncid, NC_GLOBAL, "noValues", NC_DOUBLE, 0, nullptr), "nc_put_att_double none");
    require(nc_put_att_uchar(ncid, NC_GLOBAL, "flag", NC_UBYTE, 1, &flag), "nc_put_att_uchar");

    int oneDimension = 0;
    int twoDimension = 0;
    int unlimitedDimension = 0;
    require(nc_def_dim(ncid, "one", 1, &oneDimension), "nc_def_dim one");
    require(nc_def_dim(ncid, "two", 2, &twoDimension), "nc_def_dim two");
    require(nc_def_dim(ncid, "records", NC_UNLIMITED, &unlimitedDimension), "nc_def_dim records");

    int oneVariable = 0;
    int twoVariable = 0;
    int noVariable = 0;
    require(nc_def_var(ncid, "oneFlag", NC_UBYTE, 1, &oneDimension, &oneVariable), "nc_def_var oneFlag");
    require(nc_def_var(ncid, "twoFlags", NC_UBYTE, 1, &twoDimension, &twoVariable), "nc_def_var twoFlags");
    require(nc_def_var(ncid, "noFlags", NC_UBYTE, 1, &unlimitedDimension, &noVariable), "nc_def_var noFlags");

    // Attributes of the variable oneFlag itself, one that holds one value and one that holds none.
    const std::int32_t scale = -9;
    require(nc_put_att_int(ncid, oneVariable, "scale", NC_INT, 1, &scale), "nc_put_att_int scale");
    require(nc_put_att_int(ncid, oneVariable, "emptyScale", NC_INT, 0, nullptr), "nc_put_att_int emptyScale");
    require(nc_enddef(ncid), "nc_enddef");

    const std::uint8_t one = 1;
    const std::uint8_t two[2] = {0, 1};
    require(nc_put_var_uchar(ncid, oneVariable, &one), "nc_put_var_uchar one");
    require(nc_put_var_uchar(ncid, twoVariable, two), "nc_put_var_uchar two");
    require(nc_close(ncid), "nc_close");
}

void check_scalar_attribute_of_a_group(const netcdf::NetcdfGroup& group) {
    check(group.readNumericScalarAttribute<double>("oneValue") == 2.5, "a group attribute of one value gives it");
    check(group.readNumericScalarAttribute<std::uint8_t>("flag") == 7, "a uint8 group attribute of one value gives it");
    checkThrows<std::runtime_error>([&group] { return group.readNumericScalarAttribute<double>("noValues"); },
                                    "has 0 values", "a group attribute of no values");
    checkThrows<std::runtime_error>([&group] { return group.readNumericScalarAttribute<double>("twoValues"); },
                                    "has 2 values", "a group attribute of two values");
    checkThrows<std::runtime_error>([&group] { return group.readNumericScalarAttribute<std::uint32_t>("oneValue"); },
                                    "type id", "a group attribute read as the wrong type");
    check(group.readNumericAttribute<double>("twoValues").size() == 2,
          "readNumericAttribute still gives every value of an array attribute");
    check(group.readNumericAttribute<double>("noValues").empty(),
          "readNumericAttribute still gives an empty vector for an attribute of no values");
}

void check_scalar_value_of_a_variable(const netcdf::NetcdfGroup& group) {
    check(group.variable("oneFlag").readScalarValue<std::uint8_t>() == 1, "a variable of one value gives it");
    checkThrows<std::runtime_error>([&group] { return group.variable("noFlags").readScalarValue<std::uint8_t>(); },
                                    "has 0 values", "a variable of no values");
    checkThrows<std::runtime_error>([&group] { return group.variable("twoFlags").readScalarValue<std::uint8_t>(); },
                                    "has 2 values", "a variable of two values");
    check(group.variable("twoFlags").readValues<std::uint8_t>().size() == 2,
          "readValues still gives every value of a variable");
}

void check_scalar_attribute_of_a_variable(const netcdf::NetcdfGroup& group) {
    const netcdf::NetcdfVariable variable = group.variable("oneFlag");
    check(variable.readNumericScalarAttribute<std::int32_t>("scale") == -9,
          "a variable attribute of one value gives it");
    checkThrows<std::runtime_error>(
        [&variable] { return variable.readNumericScalarAttribute<std::int32_t>("emptyScale"); }, "has 0 values",
        "a variable attribute of no values");
}

/// One record of the compound variable: the same three fields, of the same types, as a ragged segment of a GGRXF file.
struct SegmentRecord {
    std::int32_t start;
    std::uint16_t delta;
    std::int16_t factor;
};

/// Writes the file that the structure checks read. On its root group it has a text attribute, a string list, a
/// single-text attribute and numeric attributes; the subgroups first (which has a subgroup, inner) and
/// second; and variables of each type, of 0 to 3 dimensions, one with no values, one that is compound, and attributes
/// of a variable.
void writeStructureFile(const std::string& path) {
    int ncid = 0;
    require(nc_create(path.c_str(), NC_NETCDF4 | NC_CLOBBER, &ncid), "nc_create");

    require(nc_put_att_text(ncid, NC_GLOBAL, "title", 5, "hello"), "nc_put_att_text title");
    require(nc_put_att_text(ncid, NC_GLOBAL, "single", 4, "only"), "nc_put_att_text single");
    const char* names[] = {"a", "bc", "def"};
    require(nc_put_att_string(ncid, NC_GLOBAL, "names", 3, names), "nc_put_att_string names");
    const std::int32_t i32[2] = {1, -2};
    const std::uint32_t u32[1] = {3};
    const std::uint8_t u8[1] = {6};
    const double f64[2] = {0.5, 1.5};
    require(nc_put_att_int(ncid, NC_GLOBAL, "i32", NC_INT, 2, i32), "nc_put_att_int");
    require(nc_put_att_uint(ncid, NC_GLOBAL, "u32", NC_UINT, 1, u32), "nc_put_att_uint");
    require(nc_put_att_uchar(ncid, NC_GLOBAL, "u8", NC_UBYTE, 1, u8), "nc_put_att_uchar");
    require(nc_put_att_double(ncid, NC_GLOBAL, "f64", NC_DOUBLE, 2, f64), "nc_put_att_double");

    int first = 0;
    int inner = 0;
    int second = 0;
    require(nc_def_grp(ncid, "first", &first), "nc_def_grp first");
    require(nc_def_grp(first, "inner", &inner), "nc_def_grp inner");
    require(nc_def_grp(ncid, "second", &second), "nc_def_grp second");

    int rows = 0;
    int cols = 0;
    int axes = 0;
    int records = 0;
    require(nc_def_dim(ncid, "rows", 2, &rows), "nc_def_dim rows");
    require(nc_def_dim(ncid, "cols", 3, &cols), "nc_def_dim cols");
    require(nc_def_dim(ncid, "axes", 2, &axes), "nc_def_dim axes");
    require(nc_def_dim(ncid, "records", NC_UNLIMITED, &records), "nc_def_dim records");

    int i32Variable = 0;
    int u32Variable = 0;
    int u16Variable = 0;
    int u8Variable = 0;
    int f64Variable = 0;
    int matrix = 0;
    int cube = 0;
    int scalar = 0;
    int empty = 0;
    require(nc_def_var(ncid, "i32v", NC_INT, 1, &rows, &i32Variable), "nc_def_var i32v");
    require(nc_def_var(ncid, "u32v", NC_UINT, 1, &cols, &u32Variable), "nc_def_var u32v");
    require(nc_def_var(ncid, "u16v", NC_USHORT, 1, &cols, &u16Variable), "nc_def_var u16v");
    require(nc_def_var(ncid, "u8v", NC_UBYTE, 1, &rows, &u8Variable), "nc_def_var u8v");
    require(nc_def_var(ncid, "f64v", NC_DOUBLE, 1, &cols, &f64Variable), "nc_def_var f64v");
    const int matrixDimensions[2] = {rows, cols};
    require(nc_def_var(ncid, "matrix", NC_INT, 2, matrixDimensions, &matrix), "nc_def_var matrix");
    const int cubeDimensions[3] = {rows, cols, axes};
    require(nc_def_var(ncid, "cube", NC_INT, 3, cubeDimensions, &cube), "nc_def_var cube");
    require(nc_def_var(ncid, "scalarVar", NC_INT, 0, nullptr, &scalar), "nc_def_var scalarVar");
    require(nc_def_var(ncid, "emptyValues", NC_INT, 1, &records, &empty), "nc_def_var emptyValues");

    // Attributes of a variable: one value of a double, and a list of two integers.
    const double scale = 0.5;
    const std::int32_t list[2] = {1, 2};
    require(nc_put_att_double(ncid, i32Variable, "scale", NC_DOUBLE, 1, &scale), "nc_put_att_double scale");
    require(nc_put_att_int(ncid, i32Variable, "list", NC_INT, 2, list), "nc_put_att_int list");

    // A compound type of three fields, a variable of three records of it, and a compound variable of two dimensions.
    int segmentType = 0;
    require(nc_def_compound(ncid, sizeof(SegmentRecord), "Segment", &segmentType), "nc_def_compound");
    require(nc_insert_compound(ncid, segmentType, "start", offsetof(SegmentRecord, start), NC_INT), "start");
    require(nc_insert_compound(ncid, segmentType, "delta", offsetof(SegmentRecord, delta), NC_USHORT), "delta");
    require(nc_insert_compound(ncid, segmentType, "factor", offsetof(SegmentRecord, factor), NC_SHORT), "factor");
    int segments = 0;
    int segmentGrid = 0;
    require(nc_def_var(ncid, "segs", segmentType, 1, &cols, &segments), "nc_def_var segs");
    require(nc_def_var(ncid, "segGrid", segmentType, 2, matrixDimensions, &segmentGrid), "nc_def_var segGrid");
    require(nc_enddef(ncid), "nc_enddef");

    const std::int32_t i32Values[2] = {10, -20};
    const std::uint32_t u32Values[3] = {1, 2, 3};
    const std::uint16_t u16Values[3] = {7, 8, 9};
    const std::uint8_t u8Values[2] = {1, 2};
    const double f64Values[3] = {0.25, 0.5, 0.75};
    const std::int32_t matrixValues[6] = {0, 1, 2, 3, 4, 5};
    const std::int32_t cubeValues[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    const std::int32_t scalarValue = 42;
    const SegmentRecord segmentValues[3] = {{100, 7, -3}, {200, 8, -4}, {300, 9, -5}};
    require(nc_put_var_int(ncid, i32Variable, i32Values), "nc_put_var_int i32v");
    require(nc_put_var_uint(ncid, u32Variable, u32Values), "nc_put_var_uint u32v");
    require(nc_put_var_ushort(ncid, u16Variable, u16Values), "nc_put_var_ushort u16v");
    require(nc_put_var_uchar(ncid, u8Variable, u8Values), "nc_put_var_uchar u8v");
    require(nc_put_var_double(ncid, f64Variable, f64Values), "nc_put_var_double f64v");
    require(nc_put_var_int(ncid, matrix, matrixValues), "nc_put_var_int matrix");
    require(nc_put_var_int(ncid, cube, cubeValues), "nc_put_var_int cube");
    require(nc_put_var_int(ncid, scalar, &scalarValue), "nc_put_var_int scalarVar");
    require(nc_put_var(ncid, segments, segmentValues), "nc_put_var segs");
    require(nc_close(ncid), "nc_close");
}

void check_opening_a_file(const std::string& path, const std::string& otherPath) {
    checkThrows<std::runtime_error>([] { netcdf::NetcdfFile("/no/such/directory/file.nc"); }, "nc_open",
                                    "opening a file that does not exist");

    netcdf::NetcdfFile original(path);
    netcdf::NetcdfFile moved(std::move(original));
    check(moved.rootGroup().hasAttribute("title"), "a file that was moved to a new file can be read");

    netcdf::NetcdfFile other(otherPath);
    other = std::move(moved);
    check(other.rootGroup().hasAttribute("title"), "a file that was moved over an open file can be read");
}

void check_groups_and_attributes_exist(const netcdf::NetcdfGroup& group) {
    const std::vector<netcdf::NetcdfGroup> subgroups = group.subgroups();
    check(subgroups.size() == 2 && subgroups[0].name() == "first" && subgroups[1].name() == "second",
          "the subgroups of the root group are first and second, and not the inner group of first");
    check(subgroups[0].subgroups().size() == 1 && subgroups[0].subgroups()[0].name() == "inner",
          "a group with a group inside it has that one subgroup");
    check(subgroups[1].subgroups().empty(), "a group with no group inside it has no subgroups");

    check(group.hasAttribute("title"), "a group has an attribute that it was given");
    check(!group.hasAttribute("absent"), "a group has no attribute that it was not given");
    check(group.variable("i32v").hasAttribute("scale"), "a variable has an attribute that it was given");
    check(!group.variable("i32v").hasAttribute("absent"), "a variable has no attribute that it was not given");
}

void check_text_attributes(const netcdf::NetcdfGroup& group) {
    check(group.readTextAttribute("title") == "hello", "a text attribute");
    checkThrows<std::runtime_error>([&group] { return group.readTextAttribute("absent"); }, "absent",
                                    "a text attribute that is not there");
    checkThrows<std::runtime_error>([&group] { return group.readTextAttribute("i32"); }, "i32",
                                    "a numeric attribute read as text");

    check(group.readStringListAttribute("names") == std::vector<std::string>({"a", "bc", "def"}),
          "a list of three strings");
    check(group.readStringListAttribute("single") == std::vector<std::string>({"only"}),
          "a list of one string that is stored as plain text");
    checkThrows<std::runtime_error>([&group] { return group.readStringListAttribute("absent"); }, "absent",
                                    "a string list that is not there");
    checkThrows<std::runtime_error>([&group] { return group.readStringListAttribute("i32"); }, "i32",
                                    "a numeric attribute read as a string list");
}

void check_numeric_attributes(const netcdf::NetcdfGroup& group) {
    // The types that the group attributes of a GGRXF file have.
    check(group.readNumericAttribute<std::uint32_t>("u32") == std::vector<std::uint32_t>({3}), "a uint32 attribute");
    check(group.readNumericAttribute<std::uint8_t>("u8") == std::vector<std::uint8_t>({6}), "a uint8 attribute");
    check(group.readNumericAttribute<double>("f64") == std::vector<double>({0.5, 1.5}), "a double attribute");

    checkThrows<std::runtime_error>([&group] { return group.readNumericAttribute<double>("i32"); }, "type id",
                                    "an int32 attribute read as a double");
    checkThrows<std::runtime_error>([&group] { return group.readNumericAttribute<double>("absent"); }, "absent",
                                    "a numeric attribute that is not there");
    checkThrows<std::runtime_error>([&group] { return group.readNumericAttribute<double>("title"); }, "title",
                                    "a text attribute read as a number");
}

void check_variables_and_their_dimensions(const netcdf::NetcdfGroup& group) {
    check(group.hasVariable("matrix"), "a variable that is there");
    check(!group.hasVariable("absent"), "a variable that is not there");
    checkThrows<std::runtime_error>([&group] { return group.variable("absent"); }, "absent",
                                    "a variable that is not there");
    check(group.variable("matrix").name() == "matrix", "the name of a variable");

    check(group.variable("scalarVar").dimensionLengths().empty(), "a variable with no dimensions");
    check(group.variable("i32v").dimensionLengths() == std::vector<std::size_t>({2}), "a variable of one dimension");
    check(group.variable("matrix").dimensionLengths() == std::vector<std::size_t>({2, 3}),
          "a variable of two dimensions");
    check(group.variable("cube").dimensionLengths() == std::vector<std::size_t>({2, 3, 2}),
          "a variable of three dimensions");
}

void check_values_of_each_type(const netcdf::NetcdfGroup& group) {
    check(group.variable("i32v").readValues<std::int32_t>() == std::vector<std::int32_t>({10, -20}), "int32 values");
    check(group.variable("u32v").readValues<std::uint32_t>() == std::vector<std::uint32_t>({1, 2, 3}), "uint32 values");
    check(group.variable("u16v").readValues<std::uint16_t>() == std::vector<std::uint16_t>({7, 8, 9}), "uint16 values");
    check(group.variable("u8v").readValues<std::uint8_t>() == std::vector<std::uint8_t>({1, 2}), "uint8 values");
    check(group.variable("f64v").readValues<double>() == std::vector<double>({0.25, 0.5, 0.75}), "double values");

    check(group.variable("matrix").readValues<std::int32_t>() == std::vector<std::int32_t>({0, 1, 2, 3, 4, 5}),
          "the values of two dimensions are in row-major order");
    check(group.variable("cube").readValues<std::int32_t>().size() == 12, "the values of three dimensions");
    check(group.variable("scalarVar").readValues<std::int32_t>() == std::vector<std::int32_t>({42}),
          "the value of a variable with no dimensions");
    check(group.variable("emptyValues").readValues<std::int32_t>().empty(), "a variable with no values");

    checkThrows<std::runtime_error>([&group] { return group.variable("i32v").readValues<double>(); }, "type id",
                                    "int32 values read as doubles");
}

void check_compound_fields(const netcdf::NetcdfGroup& group) {
    const netcdf::NetcdfVariable segments = group.variable("segs");
    check(segments.readCompoundField<std::int32_t>("start") == std::vector<std::int32_t>({100, 200, 300}),
          "an int32 field of a compound variable");
    check(segments.readCompoundField<std::uint16_t>("delta") == std::vector<std::uint16_t>({7, 8, 9}),
          "a uint16 field of a compound variable");
    check(segments.readCompoundField<std::int16_t>("factor") == std::vector<std::int16_t>({-3, -4, -5}),
          "an int16 field of a compound variable");

    checkThrows<std::runtime_error>([&segments] { return segments.readCompoundField<std::uint16_t>("start"); },
                                    "type id", "an int32 field read as a uint16");
    checkThrows<std::runtime_error>([&segments] { return segments.readCompoundField<std::int32_t>("absent"); },
                                    "absent", "a field that the compound type does not have");
    checkThrows<std::runtime_error>(
        [&group] { return group.variable("i32v").readCompoundField<std::int32_t>("start"); }, "i32v",
        "a variable that is not compound");
    checkThrows<std::runtime_error>(
        [&group] { return group.variable("segGrid").readCompoundField<std::int32_t>("start"); }, "segGrid",
        "a compound variable of two dimensions");
}

void check_attributes_of_a_variable(const netcdf::NetcdfGroup& group) {
    const netcdf::NetcdfVariable variable = group.variable("i32v");
    check(variable.readNumericAttribute<std::int32_t>("list") == std::vector<std::int32_t>({1, 2}),
          "a list attribute of a variable");
    check(variable.readNumericAttribute<double>("scale") == std::vector<double>({0.5}),
          "a double attribute of a variable");
    checkThrows<std::runtime_error>([&variable] { return variable.readNumericAttribute<double>("list"); }, "type id",
                                    "an int32 attribute of a variable read as a double");
}

}  // namespace

int main() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path();
    const std::string path = (directory / "snap_netcdf_wrapper_test.nc").string();
    const std::string structurePath = (directory / "snap_netcdf_wrapper_structure_test.nc").string();
    const std::string otherPath = (directory / "snap_netcdf_wrapper_other_test.nc").string();
    try {
        writeTestFile(path);
        {
            const netcdf::NetcdfFile file(path);
            const netcdf::NetcdfGroup group = file.rootGroup();
            check_scalar_attribute_of_a_group(group);
            check_scalar_value_of_a_variable(group);
            check_scalar_attribute_of_a_variable(group);
        }

        writeStructureFile(structurePath);
        writeStructureFile(otherPath);
        check_opening_a_file(structurePath, otherPath);
        {
            const netcdf::NetcdfFile file(structurePath);
            const netcdf::NetcdfGroup group = file.rootGroup();
            check_groups_and_attributes_exist(group);
            check_text_attributes(group);
            check_numeric_attributes(group);
            check_variables_and_their_dimensions(group);
            check_values_of_each_type(group);
            check_compound_fields(group);
            check_attributes_of_a_variable(group);
        }
    } catch (const std::exception& error) {
        check(false, std::string("unexpected exception: ") + error.what());
    }
    std::filesystem::remove(path);
    std::filesystem::remove(structurePath);
    std::filesystem::remove(otherPath);

    return snap_test::finish("netcdf_wrapper_test");
}
