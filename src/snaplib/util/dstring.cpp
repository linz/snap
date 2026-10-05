#include "snapconfig.h"
/* dstring.c: Management of dynamically allocated strings */

/*
   $Log: dstring.c,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:57:37  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <limits>
#include <stdexcept>
#include "util/snapctype.h"

#include "util/dstring.h"
#include "util/chkalloc.h"

char *copy_string( const char *string )
{
    return copy_string_nch( string, string ? strlen(string) : 0 );
}

char *copy_string_nch( const char *string, int nch )
{
    char *s;
    if( ! string || nch < 0 ) return 0;
    s = (char *) check_malloc( nch + 1 );
    strncpy( s, string, nch );
    s[nch]=0;
    return s;
}

// Throws std::overflow_error if string.size() doesn't fit in the int32_t length
// prefix, rather than silently truncating it - matching write_raw_long32's
// precedent (util/binfile.h).
void dump_string( const std::string &string, FILE *b )
{
    if( string.size() > static_cast<size_t>( std::numeric_limits<int>::max() ) )
    {
        throw std::overflow_error(
            "string of length " + std::to_string( string.size() ) +
            " exceeds int32_t range while writing .bin file" );
    }
    int len = static_cast<int>( string.size() );
    fwrite(&len,sizeof(len),1,b);
    if( len > 0 ) fwrite(string.data(),len,1,b);
}

std::string reload_string( FILE *b )
{
    int len;
    fread(&len,sizeof(len),1,b);
    if( len <= 0 ) return std::string();
    std::string s( len, '\0' );
    fread( &s[0], len, 1, b );
    return s;
}

void dump_string( const std::optional<std::string> &string, FILE *b )
{
    if( !string )
    {
        int len = -1;
        fwrite(&len,sizeof(len),1,b);
        return;
    }
    dump_string( *string, b );
}

std::optional<std::string> reload_optional_string( FILE *b )
{
    int len;
    fread(&len,sizeof(len),1,b);
    if( len < 0 ) return std::nullopt;
    if( len == 0 ) return std::string();
    std::string s( len, '\0' );
    fread( &s[0], len, 1, b );
    return s;
}
