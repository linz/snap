#include "deformation/netcdf_wrapper.h"

#include <cstdint>
#include <cstring>
#include <utility>

#include <netcdf.h>

namespace netcdf {

NetcdfError::NetcdfError( const std::string &operation, const std::string &name, int errorCode )
    : std::runtime_error( operation + " on '" + name + "' failed: " + nc_strerror( errorCode ) )
{
}

namespace detail {

// Maps a supported C++ type to the netCDF atomic type its own stored data
// must exactly match - a size-only check would let e.g. an 8-byte
// NC_STRING field (a pointer, on every real platform this project targets)
// pass as a double by coincidence of size, so the check is by real type
// id, not size.
template <typename T>
struct NcAtomicType;

template <>
struct NcAtomicType<std::int16_t>
{
    static constexpr nc_type value = NC_SHORT;
};

template <>
struct NcAtomicType<std::uint16_t>
{
    static constexpr nc_type value = NC_USHORT;
};

template <>
struct NcAtomicType<std::int32_t>
{
    static constexpr nc_type value = NC_INT;
};

template <>
struct NcAtomicType<std::uint32_t>
{
    static constexpr nc_type value = NC_UINT;
};

template <>
struct NcAtomicType<std::uint8_t>
{
    static constexpr nc_type value = NC_UBYTE;
};

template <>
struct NcAtomicType<double>
{
    static constexpr nc_type value = NC_DOUBLE;
};

// --- shared attribute helpers (ncid + varid together address either a
// group's own attribute, varid == NC_GLOBAL, or one variable's own
// attribute, varid == that variable's real id) ---------------------------

bool hasAttributeImpl( int ncid, int varid, const std::string &attributeName )
{
    int attId = 0;
    const int status = nc_inq_attid( ncid, varid, attributeName.c_str(), &attId );
    if( status == NC_NOERR ) {
        return true;
    }
    if( status == NC_ENOTATT ) {
        return false;
    }
    throw NetcdfError( "nc_inq_attid", attributeName, status );
}

std::string readTextAttributeImpl(
    int ncid, int varid, const std::string &containerName, const std::string &attributeName )
{
    std::size_t length = 0;
    int status = nc_inq_attlen( ncid, varid, attributeName.c_str(), &length );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_attlen", containerName + "." + attributeName, status );
    }

    std::string value( length, '\0' );
    if( length > 0 ) {
        status = nc_get_att_text( ncid, varid, attributeName.c_str(), value.data() );
        if( status != NC_NOERR ) {
            throw NetcdfError( "nc_get_att_text", containerName + "." + attributeName, status );
        }
    }
    return value;
}

std::vector<std::string> readStringListAttributeImpl(
    int ncid, int varid, const std::string &containerName, const std::string &attributeName )
{
    // A length-1 string-list attribute can round-trip as a plain NC_CHAR
    // text attribute instead of a length-1 NC_STRING array - a real
    // netCDF4-python write-time quirk (confirmed directly against real
    // GGRXF files' own single-parameter `gridParameters`), not specific to
    // that attribute. Handled here, not by each caller, since it's a
    // general netCDF representation ambiguity this wrapper's own contract
    // ("a string list, however it's actually stored") should absorb.
    nc_type xtype = NC_NAT;
    int status = nc_inq_atttype( ncid, varid, attributeName.c_str(), &xtype );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_atttype", containerName + "." + attributeName, status );
    }
    if( xtype == NC_CHAR ) {
        return { readTextAttributeImpl( ncid, varid, containerName, attributeName ) };
    }
    if( xtype != NC_STRING ) {
        throw std::runtime_error(
            "netcdf: attribute '" + attributeName + "' of '" + containerName + "' has netCDF type id "
            + std::to_string( xtype ) + ", expected NC_STRING or NC_CHAR" );
    }

    std::size_t count = 0;
    status = nc_inq_attlen( ncid, varid, attributeName.c_str(), &count );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_attlen", containerName + "." + attributeName, status );
    }

    std::vector<char *> raw( count, nullptr );
    status = nc_get_att_string( ncid, varid, attributeName.c_str(), raw.data() );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_get_att_string", containerName + "." + attributeName, status );
    }

    std::vector<std::string> values;
    values.reserve( count );
    for( char *entry : raw ) {
        values.emplace_back( entry != nullptr ? entry : "" );
    }
    nc_free_string( count, raw.data() );
    return values;
}

template <typename T>
std::vector<T> readNumericAttributeImpl(
    int ncid, int varid, const std::string &containerName, const std::string &attributeName )
{
    nc_type xtype = NC_NAT;
    int status = nc_inq_atttype( ncid, varid, attributeName.c_str(), &xtype );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_atttype", containerName + "." + attributeName, status );
    }
    if( xtype != NcAtomicType<T>::value ) {
        throw std::runtime_error(
            "netcdf: attribute '" + attributeName + "' of '" + containerName + "' has netCDF type id "
            + std::to_string( xtype ) + ", expected type id " + std::to_string( NcAtomicType<T>::value ) );
    }

    std::size_t length = 0;
    status = nc_inq_attlen( ncid, varid, attributeName.c_str(), &length );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_attlen", containerName + "." + attributeName, status );
    }

    std::vector<T> result( length );
    status = nc_get_att( ncid, varid, attributeName.c_str(), result.data() );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_get_att", containerName + "." + attributeName, status );
    }
    return result;
}

// --- shared compound-field helpers (variable data and, in principle,
// attributes - only the variable path is wired up below, since no real
// GGRXF file uses a compound-typed attribute; see readCompoundField's own
// comment) ---------------------------------------------------------------

// Looks up fieldName's own byte offset within compound type xtype, and
// that compound type's own total per-record size - throwing (naming
// containerName, the variable the field was requested from) if xtype
// isn't a compound type, fieldName doesn't exist on it, or its own stored
// type doesn't exactly match expectedFieldType.
void compoundFieldLayout(
    int ncid, nc_type xtype, const std::string &containerName, const std::string &fieldName,
    nc_type expectedFieldType, std::size_t &fieldOffset, std::size_t &recordSize )
{
    int status = nc_inq_compound_size( ncid, xtype, &recordSize );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_compound_size", containerName, status );
    }

    int fieldId = 0;
    status = nc_inq_compound_fieldindex( ncid, xtype, fieldName.c_str(), &fieldId );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_compound_fieldindex", containerName + "." + fieldName, status );
    }

    nc_type fieldType = NC_NAT;
    status = nc_inq_compound_fieldtype( ncid, xtype, fieldId, &fieldType );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_compound_fieldtype", containerName + "." + fieldName, status );
    }
    if( fieldType != expectedFieldType ) {
        throw std::runtime_error(
            "netcdf: field '" + fieldName + "' of '" + containerName + "' has netCDF type id "
            + std::to_string( fieldType ) + ", expected type id " + std::to_string( expectedFieldType ) );
    }

    std::size_t offset = 0;
    status = nc_inq_compound_fieldoffset( ncid, xtype, fieldId, &offset );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_compound_fieldoffset", containerName + "." + fieldName, status );
    }
    fieldOffset = offset;
}

// Extracts field T from every one of recordCount records packed into raw
// (each recordSize bytes, the field itself fieldOffset bytes into each
// record).
template <typename T>
std::vector<T> extractCompoundField(
    const std::vector<char> &raw, std::size_t recordCount, std::size_t recordSize, std::size_t fieldOffset )
{
    std::vector<T> result( recordCount );
    for( std::size_t r = 0; r < recordCount; ++r ) {
        std::memcpy( &result[r], raw.data() + r * recordSize + fieldOffset, sizeof( T ) );
    }
    return result;
}

} // namespace detail

// --- NetcdfVariable ---------------------------------------------------

NetcdfVariable::NetcdfVariable( int ncid, int varid, std::string name )
    : ncid_( ncid ),
      varid_( varid ),
      name_( std::move( name ) )
{
}

const std::string &NetcdfVariable::name() const
{
    return name_;
}

std::vector<std::size_t> NetcdfVariable::dimensionLengths() const
{
    int ndims = 0;
    int status = nc_inq_varndims( ncid_, varid_, &ndims );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_varndims", name_, status );
    }

    std::vector<int> dimids( static_cast<std::size_t>( ndims ) );
    status = nc_inq_vardimid( ncid_, varid_, dimids.data() );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_vardimid", name_, status );
    }

    std::vector<std::size_t> lengths( dimids.size() );
    for( std::size_t d = 0; d < dimids.size(); ++d ) {
        std::size_t length = 0;
        status = nc_inq_dimlen( ncid_, dimids[d], &length );
        if( status != NC_NOERR ) {
            throw NetcdfError( "nc_inq_dimlen", name_, status );
        }
        lengths[d] = length;
    }
    return lengths;
}

template <typename T>
std::vector<T> NetcdfVariable::readValues() const
{
    std::size_t total = 1;
    for( std::size_t length : dimensionLengths() ) {
        total *= length;
    }

    nc_type xtype = NC_NAT;
    int status = nc_inq_vartype( ncid_, varid_, &xtype );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_vartype", name_, status );
    }
    if( xtype != detail::NcAtomicType<T>::value ) {
        throw std::runtime_error(
            "netcdf::NetcdfVariable::readValues: '" + name_ + "' has netCDF type id " + std::to_string( xtype )
            + ", expected type id " + std::to_string( detail::NcAtomicType<T>::value ) );
    }

    std::vector<T> result( total );
    status = nc_get_var( ncid_, varid_, result.data() );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_get_var", name_, status );
    }
    return result;
}

template <typename T>
std::vector<T> NetcdfVariable::readCompoundField( const std::string &fieldName ) const
{
    const auto dims = dimensionLengths();
    if( dims.size() != 1 ) {
        throw std::runtime_error(
            "netcdf::NetcdfVariable::readCompoundField: '" + name_ + "' is not a 1-dimensional array" );
    }
    const std::size_t recordCount = dims[0];

    nc_type xtype = NC_NAT;
    int status = nc_inq_vartype( ncid_, varid_, &xtype );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_vartype", name_, status );
    }

    std::size_t fieldOffset = 0;
    std::size_t recordSize = 0;
    detail::compoundFieldLayout(
        ncid_, xtype, name_, fieldName, detail::NcAtomicType<T>::value, fieldOffset, recordSize );

    std::vector<char> raw( recordCount * recordSize );
    status = nc_get_var( ncid_, varid_, raw.data() );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_get_var", name_, status );
    }

    return detail::extractCompoundField<T>( raw, recordCount, recordSize, fieldOffset );
}

bool NetcdfVariable::hasAttribute( const std::string &attributeName ) const
{
    return detail::hasAttributeImpl( ncid_, varid_, attributeName );
}

template <typename T>
std::vector<T> NetcdfVariable::readNumericAttribute( const std::string &attributeName ) const
{
    return detail::readNumericAttributeImpl<T>( ncid_, varid_, name_, attributeName );
}

// --- NetcdfGroup -------------------------------------------------------

NetcdfGroup::NetcdfGroup( int ncid, std::string name )
    : ncid_( ncid ),
      name_( std::move( name ) )
{
}

const std::string &NetcdfGroup::name() const
{
    return name_;
}

std::vector<NetcdfGroup> NetcdfGroup::subgroups() const
{
    int numGroups = 0;
    int status = nc_inq_grps( ncid_, &numGroups, nullptr );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_grps", name_, status );
    }

    std::vector<int> childIds( static_cast<std::size_t>( numGroups ) );
    status = nc_inq_grps( ncid_, &numGroups, childIds.data() );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_grps", name_, status );
    }

    std::vector<NetcdfGroup> groups;
    groups.reserve( childIds.size() );
    for( int childId : childIds ) {
        char nameBuffer[NC_MAX_NAME + 1] = {};
        status = nc_inq_grpname( childId, nameBuffer );
        if( status != NC_NOERR ) {
            throw NetcdfError( "nc_inq_grpname", name_, status );
        }
        groups.emplace_back( childId, std::string( nameBuffer ) );
    }
    return groups;
}

bool NetcdfGroup::hasAttribute( const std::string &attributeName ) const
{
    return detail::hasAttributeImpl( ncid_, NC_GLOBAL, attributeName );
}

std::string NetcdfGroup::readTextAttribute( const std::string &attributeName ) const
{
    return detail::readTextAttributeImpl( ncid_, NC_GLOBAL, name_, attributeName );
}

std::vector<std::string> NetcdfGroup::readStringListAttribute( const std::string &attributeName ) const
{
    return detail::readStringListAttributeImpl( ncid_, NC_GLOBAL, name_, attributeName );
}

template <typename T>
std::vector<T> NetcdfGroup::readNumericAttribute( const std::string &attributeName ) const
{
    return detail::readNumericAttributeImpl<T>( ncid_, NC_GLOBAL, name_, attributeName );
}

bool NetcdfGroup::hasVariable( const std::string &variableName ) const
{
    int varid = 0;
    const int status = nc_inq_varid( ncid_, variableName.c_str(), &varid );
    if( status == NC_NOERR ) {
        return true;
    }
    if( status == NC_ENOTVAR ) {
        return false;
    }
    throw NetcdfError( "nc_inq_varid", name_ + "." + variableName, status );
}

NetcdfVariable NetcdfGroup::variable( const std::string &variableName ) const
{
    int varid = 0;
    const int status = nc_inq_varid( ncid_, variableName.c_str(), &varid );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_varid", name_ + "." + variableName, status );
    }
    return NetcdfVariable( ncid_, varid, variableName );
}

// --- NetcdfFile ---------------------------------------------------------

NetcdfFile::NetcdfFile( const std::string &path )
    : ncid_( 0 ),
      open_( false )
{
    const int status = nc_open( path.c_str(), NC_NOWRITE, &ncid_ );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_open", path, status );
    }
    open_ = true;
}

NetcdfFile::~NetcdfFile()
{
    if( open_ ) {
        nc_close( ncid_ );
    }
}

NetcdfFile::NetcdfFile( NetcdfFile &&other ) noexcept
    : ncid_( other.ncid_ ),
      open_( other.open_ )
{
    other.open_ = false;
}

NetcdfFile &NetcdfFile::operator=( NetcdfFile &&other ) noexcept
{
    if( this != &other ) {
        if( open_ ) {
            nc_close( ncid_ );
        }
        ncid_ = other.ncid_;
        open_ = other.open_;
        other.open_ = false;
    }
    return *this;
}

NetcdfGroup NetcdfFile::rootGroup() const
{
    char nameBuffer[NC_MAX_NAME + 1] = {};
    const int status = nc_inq_grpname( ncid_, nameBuffer );
    if( status != NC_NOERR ) {
        throw NetcdfError( "nc_inq_grpname", "<root>", status );
    }
    return NetcdfGroup( ncid_, std::string( nameBuffer ) );
}

// --- Explicit instantiation ---------------------------------------------
//
// The netCDF atomic types GGRXF's own format actually stores (verified
// against GGRXF_DEFORMATION_FORMAT.md's variable/attribute tables and the
// real writer, not assumed): std::int32_t (displacement; segment
// records' segment_start_value), std::uint32_t (reference_day;
// iNodeCount/jNodeCount), std::uint16_t (segment_count_{enu}, own_end_day,
// segment_ramp_end_delta_{enu}; segment records' day_delta),
// std::uint8_t (has_end_day, function_type_{enu}; parameters.count,
// timeFunctions.count, gridPriority), double (segment_time_param_{enu};
// affineCoeffs, scale_factor, and the other float64 attributes), plus
// std::int16_t for segment records' segment_scale_factor specifically. A
// caller needing another type gets a compile error here rather than a
// silent, potentially-misinterpreted read.

template std::vector<std::int32_t> NetcdfVariable::readValues<std::int32_t>() const;
template std::vector<std::uint32_t> NetcdfVariable::readValues<std::uint32_t>() const;
template std::vector<std::uint16_t> NetcdfVariable::readValues<std::uint16_t>() const;
template std::vector<std::uint8_t> NetcdfVariable::readValues<std::uint8_t>() const;
template std::vector<double> NetcdfVariable::readValues<double>() const;

template std::vector<std::int16_t> NetcdfVariable::readCompoundField<std::int16_t>( const std::string & ) const;
template std::vector<std::uint16_t> NetcdfVariable::readCompoundField<std::uint16_t>( const std::string & ) const;
template std::vector<std::int32_t> NetcdfVariable::readCompoundField<std::int32_t>( const std::string & ) const;

template std::vector<std::int32_t> NetcdfVariable::readNumericAttribute<std::int32_t>( const std::string & ) const;
template std::vector<double> NetcdfVariable::readNumericAttribute<double>( const std::string & ) const;

template std::vector<std::uint8_t> NetcdfGroup::readNumericAttribute<std::uint8_t>( const std::string & ) const;
template std::vector<std::uint32_t> NetcdfGroup::readNumericAttribute<std::uint32_t>( const std::string & ) const;
template std::vector<double> NetcdfGroup::readNumericAttribute<double>( const std::string & ) const;

} // namespace netcdf
