#ifndef CRDSYS_PARSE_FIELD_H
#define CRDSYS_PARSE_FIELD_H

#include <string>
#include <string_view>

#include "util/errdef.h"
#include "util/fieldscanner.hpp"

/// Converts the outcome of reading a field into the status code the
/// coordinate system parsers carry.
inline int field_status(
    FieldResult result )  ///< the outcome of the read
{
    switch( result )
    {
    case FieldResult::Ok: return OK;
    case FieldResult::NoMoreData: return NO_MORE_DATA;
    case FieldResult::MalformedQuote: return MISSING_DATA;
    case FieldResult::InvalidValue: return INVALID_DATA;
    }
    return INVALID_DATA;
}

/// Reads the next field as a string, unless status already records a
/// failure. Sets bad to fieldname before reading, so that it names the field
/// that failed if the read does.
/// \return the new status.
inline int read_crdsys_string(
    FieldScanner &scanner,      ///< the scanner to read from
    int status,                 ///< the status so far, which is returned unchanged if not OK
    std::string &value,         ///< set to the field if it is read
    size_t maxlength,           ///< the most characters to keep
    std::string_view fieldname, ///< the name to report if the read fails
    std::string_view &bad )     ///< set to fieldname if a read is attempted
{
    if( status != OK ) return status;
    bad = fieldname;
    return field_status( read_string_field( scanner, value, maxlength ) );
}

/// Reads the next field as a string, unless status already records a
/// failure, for parsers that do not report which field failed.
/// \return the new status.
inline int read_crdsys_string(
    FieldScanner &scanner,  ///< the scanner to read from
    int status,             ///< the status so far, which is returned unchanged if not OK
    std::string &value,     ///< set to the field if it is read
    size_t maxlength )      ///< the most characters to keep
{
    std::string_view bad;
    return read_crdsys_string( scanner, status, value, maxlength, std::string_view(), bad );
}

/// Reads the next field as a number, unless status already records a
/// failure. Sets bad to fieldname before reading, so that it names the field
/// that failed if the read does.
/// \return the new status.
inline int read_crdsys_double(
    FieldScanner &scanner,      ///< the scanner to read from
    int status,                 ///< the status so far, which is returned unchanged if not OK
    double &value,              ///< set to the number if it is read
    std::string_view fieldname, ///< the name to report if the read fails
    std::string_view &bad )     ///< set to fieldname if a read is attempted
{
    if( status != OK ) return status;
    bad = fieldname;
    return field_status( read_double_field( scanner, value ) );
}

/// Reads the next field as a number, unless status already records a
/// failure, for parsers that do not report which field failed.
/// \return the new status.
inline int read_crdsys_double(
    FieldScanner &scanner,  ///< the scanner to read from
    int status,             ///< the status so far, which is returned unchanged if not OK
    double &value )         ///< set to the number if it is read
{
    std::string_view bad;
    return read_crdsys_double( scanner, status, value, std::string_view(), bad );
}

#endif
