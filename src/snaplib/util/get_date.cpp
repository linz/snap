#include "snapconfig.h"

/*
   $Log: get_date.c,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:48:51  CHRIS
   Initial revision

*/

#include <stdlib.h>
#include <time.h>
#include <array>
#include <iomanip>
#include <sstream>
#include <string_view>
#include "util/get_date.h"

static constexpr std::array<std::string_view,12> mon = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG",
                      "SEP", "OCT", "NOV", "DEC"
                     };

std::string get_date()
{
    // Test tooling needs deterministic .bin output to diff byte-for-byte
    // against a committed golden copy. The real current time can't do
    // that, so this override returns a fixed value instead. The field
    // writer pads it to the full GETDATELEN bytes, so no output byte is
    // left non-deterministic.
    const char *const fixed_date = getenv( "SNAP_TEST_FIXED_DATE" );
    if( fixed_date )
    {
        return std::string( fixed_date ).substr( 0, GETDATELEN-1 );
    }

    const time_t now = time( nullptr );
    const struct tm *const lt = localtime( &now );
    std::ostringstream text;
    text << std::setfill( ' ' ) << std::setw( 2 ) << lt->tm_mday << '-' << mon[lt->tm_mon] << '-'
         << std::setw( 4 ) << 1900+lt->tm_year << ' ' << std::setfill( '0' )
         << std::setw( 2 ) << lt->tm_hour << ':' << std::setw( 2 ) << lt->tm_min << ':' << std::setw( 2 ) << lt->tm_sec;
    return text.str();
}

void write_run_date_field( FILE *const f, const std::string &runDate )
{
    std::string field = runDate.substr( 0, GETDATELEN-1 );
    field.resize( GETDATELEN, '\0' );
    fwrite( field.data(), GETDATELEN, 1, f );
}

bool read_run_date_field( FILE *const f, std::string &runDate )
{
    std::array<char,GETDATELEN> field{};
    if( fread( field.data(), GETDATELEN, 1, f ) != 1 ) return false;
    const std::string_view text( field.data(), field.size() );
    runDate = std::string( text.substr( 0, text.find( '\0' ) ) );
    return true;
}
