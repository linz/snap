#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

// A thin RAII wrapper over the netCDF-C API (no C++ binding is installed on
// any of this project's target platforms) - deliberately GGRXF-agnostic.
// This layer knows nothing about GGRXF's own group/attribute/variable
// layout; that lives in the GGRXF-specific parser built on top of it.
namespace netcdf {

// Thrown for any failing netCDF-C call - wraps nc_strerror's own message
// together with the operation and the specific group/attribute/variable
// name involved, so a caller can tell which real call failed and on what.
class NetcdfError : public std::runtime_error {
 public:
    NetcdfError(const std::string& operation, const std::string& name, int errorCode);
};

// A non-owning handle to one netCDF-4 variable (identified by ncid + varid;
// varid is only unique within the group ncid identifies, so both are
// needed together to address a variable anywhere in a file).
class NetcdfVariable {
 public:
    NetcdfVariable(int ncid, int varid, std::string name);

    [[nodiscard]] const std::string& name() const;

    // This variable's own dimension lengths, outermost first.
    [[nodiscard]] std::vector<std::size_t> dimensionLengths() const;

    // Reads this variable's own full contents as a flat, row-major array of
    // T, its own exact stored type - no numeric conversion, so an integer
    // stays an exact integer (e.g. a day count, meaningful only as one).
    // Throws if this variable's own stored netCDF type doesn't exactly
    // match T (supported T: std::int32_t, std::uint32_t, std::uint16_t,
    // std::uint8_t, double).
    template <typename T>
    [[nodiscard]] std::vector<T> readValues() const;

    // Reads this variable's own contents when it must hold exactly one
    // value, which it returns. Use this instead of readValues()[0] for a
    // variable that stands for a single value, since a variable with no
    // value, or several, would otherwise read out of bounds or have all but
    // its first value ignored. Same T constraint and exact-type-match
    // behaviour as readValues(). Throws std::runtime_error, naming this
    // variable, if it does not hold exactly one value.
    template <typename T>
    [[nodiscard]] T readScalarValue() const;

    // Reads one named field out of every record of this compound-typed
    // variable's own flat array. T's own stored netCDF type must exactly
    // match the field's own (same supported-T set as readValues() above,
    // plus std::int16_t) - the caller is responsible for matching T to the
    // field's real stored type; this class has no notion of what any
    // particular field means, only how to pull it out generically.
    template <typename T>
    [[nodiscard]] std::vector<T> readCompoundField(const std::string& fieldName) const;

    // True if this variable itself carries an attribute of this name (as
    // opposed to the group it lives in - see NetcdfGroup::hasAttribute()).
    [[nodiscard]] bool hasAttribute(const std::string& attributeName) const;

    // Reads a numeric attribute attached to this variable itself (not its
    // own group) - e.g. GGRXF's displacement.scale_factor/.missing_value.
    // Same T constraint and exact-type-match behaviour as
    // NetcdfGroup::readNumericAttribute() below.
    template <typename T>
    [[nodiscard]] std::vector<T> readNumericAttribute(const std::string& attributeName) const;

    // Reads a numeric attribute attached to this variable itself that must
    // hold exactly one value, which it returns. Use this instead of
    // readNumericAttribute()[0] for a scalar attribute, since an attribute
    // with no value, or several, would otherwise read out of bounds or have
    // all but its first value ignored. Same T constraint and exact-type-match
    // behaviour as readNumericAttribute(). Throws std::runtime_error, naming
    // this variable and the attribute, if the attribute does not have
    // exactly one value.
    template <typename T>
    [[nodiscard]] T readNumericScalarAttribute(const std::string& attributeName) const;

 private:
    int ncid_;
    int varid_;
    std::string name_;
};

// A non-owning handle to one netCDF-4 group (identified by its own ncid,
// which a netCDF-4 group id already fully determines). The root group of an
// open NetcdfFile is itself just one of these.
class NetcdfGroup {
 public:
    NetcdfGroup(int ncid, std::string name);

    [[nodiscard]] const std::string& name() const;

    // This group's own direct child groups.
    [[nodiscard]] std::vector<NetcdfGroup> subgroups() const;

    // True if this group itself carries an attribute of this name.
    [[nodiscard]] bool hasAttribute(const std::string& attributeName) const;

    // Throws NetcdfError if attributeName is absent or not a text
    // attribute.
    [[nodiscard]] std::string readTextAttribute(const std::string& attributeName) const;

    // Reads a string-list attribute, whether it's actually stored as an
    // NC_STRING array or (a real netCDF4-python write-time quirk for a
    // length-1 list) a plain NC_CHAR text attribute - the latter returns as
    // a length-1 vector. Throws NetcdfError if attributeName is absent or
    // stored as neither.
    [[nodiscard]] std::vector<std::string> readStringListAttribute(const std::string& attributeName) const;

    // Reads a numeric attribute of this group, as its own exact stored
    // type, returned as a vector of its real length (a scalar attribute
    // returns a length-1 vector; e.g. GGRXF's affineCoeffs is a real
    // length-6 array). No numeric conversion - throws if attributeName is
    // absent or its own stored netCDF type doesn't exactly match T
    // (supported T: std::int32_t, std::uint32_t, std::uint16_t,
    // std::uint8_t, double).
    template <typename T>
    [[nodiscard]] std::vector<T> readNumericAttribute(const std::string& attributeName) const;

    // Reads a numeric attribute of this group that must hold exactly one
    // value, which it returns - see NetcdfVariable::readNumericScalarAttribute()
    // for why this is used instead of readNumericAttribute()[0]. Same T
    // constraint and exact-type-match behaviour as readNumericAttribute().
    // Throws std::runtime_error, naming this group and the attribute, if the
    // attribute does not have exactly one value.
    template <typename T>
    [[nodiscard]] T readNumericScalarAttribute(const std::string& attributeName) const;

    [[nodiscard]] bool hasVariable(const std::string& variableName) const;

    // Throws NetcdfError if variableName does not exist in this group.
    [[nodiscard]] NetcdfVariable variable(const std::string& variableName) const;

 private:
    int ncid_;
    std::string name_;
};

// Owns one open netCDF-4 file (read-only), closing it on destruction.
// Move-only - a real open file handle has no meaningful copy.
class NetcdfFile {
 public:
    explicit NetcdfFile(const std::string& path);
    ~NetcdfFile();

    NetcdfFile(const NetcdfFile&) = delete;
    NetcdfFile& operator=(const NetcdfFile&) = delete;
    NetcdfFile(NetcdfFile&& other) noexcept;
    NetcdfFile& operator=(NetcdfFile&& other) noexcept;

    [[nodiscard]] NetcdfGroup rootGroup() const;

 private:
    int ncid_;
    bool open_;
};

}  // namespace netcdf
