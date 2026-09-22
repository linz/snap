#ifndef IOSTRING_H
#define IOSTRING_H

/*
   $Log: iostring.h,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:51:15  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string>
#include <string_view>

#include "util/fieldscanner.hpp"

/* Definitions of an input and output string structure */

/// Called by report_string_error() to report a parse error, with whatever
/// opaque context input_string_def::source carries (e.g. a DATAFILE*, cast
/// back to its concrete type by the handler).
typedef int (*input_string_errfunc)( void *source, int status, const char *message );

/// A cursor over one piece of input text being parsed field-by-field, plus
/// enough context to attribute a parse error back to where the text came
/// from. Used both by cfgprocs.cpp's still-unconverted config-line parsing
/// and by the coordsys/ parsers (which read scanner directly for anything
/// not covered by the functions below).
struct input_string_def
{
    /// \param text must outlive this input_string_def - it is only viewed, never copied.
    explicit input_string_def( std::string_view text ) : scanner(text) {}

    FieldScanner scanner;                        ///< the input text and read cursor
    std::string sourcename;                      ///< name of the source - file name for a file source
    void *source = nullptr;                      ///< opaque context passed to report_error
    input_string_errfunc report_error = nullptr; ///< called by report_string_error(), or null for no reporting
};

/* Output string def - defines a way of sending strings to some form
   of output device */

typedef int (*output_string_func)( const char *string, void *sink );

typedef struct
{
    void *sink;
    output_string_func write;
} output_string_def;

/* Input string functions.  Return status values are as defined in
   errdef.h, ie 0 = OK, non-zero represent errors. Used by both
   cfgprocs.cpp and the coordsys/ parsers. Operate on a plain FieldScanner,
   not input_string_def, since none of them need the error-reporting
   fields (sourcename/source/report_error) - only report_string_error()
   and unread_string() below do. */

/// Reads the next field (quote-transparent) from scanner into buf,
/// truncating without error if it doesn't fit.
/// \return OK, or NO_MORE_DATA/MISSING_DATA on failure (see errdef.h).
int next_string_field( FieldScanner &scanner, char *buf, int nbuf );

/// Reads the next field (quote-transparent) from scanner and, if it
/// case-insensitively equals test, consumes it. Otherwise leaves scanner
/// unchanged.
/// \return 1 if the field matched and was consumed, 0 otherwise.
int test_next_string_field( FieldScanner &scanner, std::string_view test );

/// Reads the next field (quote-transparent) from scanner and parses it as a double.
/// \param value a double* to set on success.
/// \return OK, or NO_MORE_DATA/MISSING_DATA/INVALID_DATA on failure (see errdef.h).
int double_from_string( FieldScanner &scanner, void *value );

/// \return the remainder of def's input, unconsumed, verbatim.
char *unread_string( input_string_def &def );

void report_string_error( input_string_def &def, int status, const char *message );

int write_output_string( output_string_def *os, const char *s );
int write_output_string2( output_string_def *os, const char *s, int options, const char *prefix );
void output_string_to_file( output_string_def *os, FILE *f );

#define OSW_TRIMR      1
#define OSW_TRIML      2
#define OSW_SKIPBLANK  4

#define OSW_TRIM       3
#define OSW_CLEAN      7

#endif


