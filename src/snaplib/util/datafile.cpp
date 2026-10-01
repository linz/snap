#include "snapconfig.h"
/* Routines for reading a data file.  These routines provide the following
   facilities:

   1) Counting of line numbers
   2) Skipping blank lines
   3) Skipping comments - anything after ! on a line
   4) Reporting errors with a file name and line number
   5) Clearing out control characters which differ between operating systems.
*/


/*
   $Log: datafile.c,v $
   Revision 1.4  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  1996/10/25 22:54:34  CHRIS
   Enlarged size of field used to read numbers from 20 characters to 30 chars.

   Revision 1.1  1995/12/22 18:55:56  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <string_view>
#include "util/snapctype.h"

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#include "util/fileutil.h"
#include "util/filelist.h"
#include "util/datafile.h"
#include "util/errdef.h"

static int default_reclen_capacity = 256;

#define ISSPACEZ(x) (ISSPACE(x) || (x)=='\x1A')

int DATAFILE::default_reclen( int newlen )
{
    const int oldlen = default_reclen_capacity;
    if( newlen > 80 ) default_reclen_capacity = newlen;
    return oldlen;
}

/// Converts a file description such as "station coordinate file" to the file
/// type recorded for it, "station_coordinate". Spaces become underscores and a
/// trailing "_file" is dropped.
static std::string description_filetype( std::string_view description )
{
    std::string filetype( description.substr( 0, 79 ) );
    std::replace( filetype.begin(), filetype.end(), ' ', '_' );
    constexpr std::string_view suffix = "_file";
    if( filetype.size() > suffix.size() &&
        std::string_view( filetype ).substr( filetype.size() - suffix.size() ) == suffix )
    {
        filetype.resize( filetype.size() - suffix.size() );
    }
    return filetype;
}

std::unique_ptr<DATAFILE> DATAFILE::open( std::string_view fname, std::string_view description )
{
    const std::string filename( fname );
    FILE *f = fopen( filename.c_str(), "rb" );
    if( f == nullptr )
    {
        const std::string msg = "Unable to open " + std::string( description.substr( 0, 60 ) );
        handle_error( FILE_OPEN_ERROR, msg.c_str(), filename.c_str() );
        return nullptr;
    }

    record_filename( filename, description_filetype( description ) );

    char header[80];
    const std::string_view start( header, fread( header, 1, sizeof( header ), f ) );
    const bool unicode = start.substr( 0, DATAFILE_UTF8_BOM.size() ) == DATAFILE_UTF8_BOM ||
                         start.substr( 0, DATAFILE_UTF16_BOM.size() ) == DATAFILE_UTF16_BOM;
    const bool binary = ! unicode && std::any_of( start.begin(), start.end(),
        []( char c ){ return c == 0 || static_cast<unsigned char>( c ) >= 0x80; } );
    if( unicode )
    {
        fclose( f );
        handle_error( FILE_OPEN_ERROR, "Cannot use unicode file - convert to ASCII", filename.c_str() );
        return nullptr;
    }
    if( binary )
    {
        fclose( f );
        handle_error( FILE_OPEN_ERROR, "File appears to contain binary data", filename.c_str() );
        return nullptr;
    }
    fseek( f, 0L, SEEK_SET );

    std::unique_ptr<DATAFILE> d( new DATAFILE );
    d->_fname = filename;
    d->_f = f;
    d->_inrec.reserve( default_reclen_capacity );
    d->start_scanner();
    return d;
}

DATAFILE::~DATAFILE()
{
    if( _f ) fclose( _f );
}

void DATAFILE::start_scanner()
{
    // Constructs _instr afresh, whether or not it already held a value.
    _instr.emplace( _inrec );
    _instr->sourcename = _fname;
    _instr->source = this;
    _instr->report_error = report_input_string_error;
}

int DATAFILE::report_input_string_error( void *source, int sts, std::string_view errmsg )
{
    return static_cast<DATAFILE *>( source )->error( sts, errmsg );
}

int DATAFILE::skip_to_blank_line()
{
    bool blank = true;
    int c;

    while( EOF != (c = fgetc( _f )) )
    {
        if( c == '\n' )
        {
            _lineno++;
            if( blank ) return OK;
            blank = true;
        }
        else if( ! ISSPACEZ(c) )
        {
            blank = false;
        }
    }
    return NO_MORE_DATA;
}

/// Appends the next line of the file, including its newline, to text. A line
/// is cut short at any null character, as fgets and strlen always have. Returns
/// false if the end of the file was reached.
static bool append_line( FILE *f, std::string &text )
{
    char chunk[256];
    while( fgets( chunk, sizeof( chunk ), f ) )
    {
        const size_t len = strlen( chunk );
        if( ! len ) return true;
        text.append( chunk, len );
        if( chunk[len-1] == '\n' ) return true;
    }
    return false;
}

int DATAFILE::read_record()
{
    bool eof = false;

    _startlineno = _lineno;
    _startloc = ftell( _f );
    _inrec.clear();
    _reclineno = 0;

    // Until we get to a non-blank line...
    while( _inrec.empty() && ! eof )
    {
        bool continued = true;
        _reclineno = 0;

        while ( continued && ! eof )
        {
            const size_t lineoffset = _inrec.size();
            if( ! append_line( _f, _inrec ) ) eof = true;
            _lineno++;
            if( ! _reclineno ) _reclineno = _lineno;
            // Remove everything after a comment character
            if( _comment_char )
            {
                const size_t end = _inrec.find( _comment_char, lineoffset );
                if( end != std::string::npos ) _inrec.erase( end );
            }
            // Trim whitespace
            while( _inrec.size() > lineoffset && ISSPACEZ( _inrec.back() ) ) _inrec.pop_back();
            // Check for line continuation
            const size_t nch = _inrec.size() - lineoffset;
            if( nch >= 2 && _inrec.back() == _continuation_char && ISSPACEZ( _inrec[_inrec.size()-2] ) )
            {
                _inrec.pop_back();
            }
            else
            {
                continued = false;
            }
        }
        // Retrim in case continuation only adds blanks
        while( _inrec.size() > 1 && ISSPACEZ( _inrec.back() ) ) _inrec.pop_back();
    }

    start_scanner();
    return _inrec.empty() ? NO_MORE_DATA : OK;
}

int DATAFILE::error( int sts, std::string_view errmsg )
{
    std::string location;
    if( ! _inrec.empty() )
    {
        location = "Line: " + std::to_string( _reclineno ) + "  ";
    }
    location += "File: " + _fname.substr( 0, MAX_FILENAME_LEN );
    handle_error( sts, std::string( errmsg ).c_str(), location.c_str() );
    if( sts >= WARNING_ERROR ) _errcount++;
    return sts;
}

void DATAFILE::save_loc( datafile_loc &dl ) const
{
    dl.line = _startlineno;
    dl.loc = _startloc;
}

void DATAFILE::reset_loc( const datafile_loc &dl )
{
    _lineno = dl.line;
    fseek( _f, dl.loc, SEEK_SET );
    read_record();
}
