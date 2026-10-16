// Standalone unit test for the checked scalar reads of the netCDF wrapper (deformation/netcdf_wrapper.h):
// readNumericScalarAttribute() of a group and of a variable, and readScalarValue() of a variable. Each must give the
// value when there is exactly one, and throw when there are none or several, instead of reading out of bounds or
// ignoring all but the first. The test writes a small netCDF file with the netCDF-C library, reads it back through the
// wrapper and deletes it. Failures are reported to stdout and the process exit code is the pass/fail signal (0 = all
// passed), as for the other test-only tools in src/test/.

#include <netcdf.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "deformation/netcdf_wrapper.h"

namespace {

int failures = 0;

void check(const bool condition, const std::string& description) {
    if (condition) {
        return;
    }
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

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

/// Checks that calling the function throws a std::runtime_error whose message contains the text.
template <typename Function>
void checkThrows(Function function,                 ///< what to call
                 const std::string& messageText,    ///< text that the message must contain
                 const std::string& description) {  ///< what the check is of
    try {
        function();
    } catch (const std::runtime_error& error) {
        check(std::string(error.what()).find(messageText) != std::string::npos,
              description + ": the message '" + error.what() + "' does not contain '" + messageText + "'");
        return;
    }
    check(false, description + ": expected an exception, but there was none");
}

void check_scalar_attribute_of_a_group(const netcdf::NetcdfGroup& group) {
    check(group.readNumericScalarAttribute<double>("oneValue") == 2.5, "a group attribute of one value gives it");
    check(group.readNumericScalarAttribute<std::uint8_t>("flag") == 7, "a uint8 group attribute of one value gives it");
    checkThrows([&group] { return group.readNumericScalarAttribute<double>("noValues"); }, "has 0 values",
                "a group attribute of no values");
    checkThrows([&group] { return group.readNumericScalarAttribute<double>("twoValues"); }, "has 2 values",
                "a group attribute of two values");
    checkThrows([&group] { return group.readNumericScalarAttribute<std::uint32_t>("oneValue"); }, "type id",
                "a group attribute read as the wrong type");
    check(group.readNumericAttribute<double>("twoValues").size() == 2,
          "readNumericAttribute still gives every value of an array attribute");
    check(group.readNumericAttribute<double>("noValues").empty(),
          "readNumericAttribute still gives an empty vector for an attribute of no values");
}

void check_scalar_value_of_a_variable(const netcdf::NetcdfGroup& group) {
    check(group.variable("oneFlag").readScalarValue<std::uint8_t>() == 1, "a variable of one value gives it");
    checkThrows([&group] { return group.variable("noFlags").readScalarValue<std::uint8_t>(); }, "has 0 values",
                "a variable of no values");
    checkThrows([&group] { return group.variable("twoFlags").readScalarValue<std::uint8_t>(); }, "has 2 values",
                "a variable of two values");
    check(group.variable("twoFlags").readValues<std::uint8_t>().size() == 2,
          "readValues still gives every value of a variable");
}

void check_scalar_attribute_of_a_variable(const netcdf::NetcdfGroup& group) {
    const netcdf::NetcdfVariable variable = group.variable("oneFlag");
    check(variable.readNumericScalarAttribute<std::int32_t>("scale") == -9,
          "a variable attribute of one value gives it");
    checkThrows([&variable] { return variable.readNumericScalarAttribute<std::int32_t>("emptyScale"); }, "has 0 values",
                "a variable attribute of no values");
}

}  // namespace

int main() {
    const std::string path = (std::filesystem::temp_directory_path() / "snap_netcdf_wrapper_test.nc").string();
    try {
        writeTestFile(path);
        {
            const netcdf::NetcdfFile file(path);
            const netcdf::NetcdfGroup group = file.rootGroup();
            check_scalar_attribute_of_a_group(group);
            check_scalar_value_of_a_variable(group);
            check_scalar_attribute_of_a_variable(group);
        }
    } catch (const std::exception& error) {
        ++failures;
        std::cout << "FAIL: unexpected exception: " << error.what() << "\n";
    }
    std::filesystem::remove(path);

    if (failures > 0) {
        std::cout << failures << " netcdf_wrapper_test check(s) failed\n";
        return 1;
    }
    std::cout << "netcdf_wrapper_test: all checks passed\n";
    return 0;
}
