#include "snapconfig.h"

#include <string.h>
#include "util/wildcard.h"


bool has_wildcard( const char *pattern )
{
    if( strchr(pattern,'*') ) return true;
    if( strchr(pattern,'?') ) return true;
    return false;
}

bool wildcard_match( const char *pattern, const char *s )
{    
    while( *pattern && *s && 
            *pattern != '*' && 
            ((*pattern == '?' && *s) || _strnicmp(pattern,s,1) == 0 )
            )
    {
        pattern++;
        s++;
    }
    if( ! *pattern && ! *s ) return true;
    if( *pattern == '*' ) 
    {
        while( *pattern == '*' ) pattern++;
        if( ! *pattern ) return true;
        while( *s )
        {
            if( wildcard_match(pattern,s) ) return true;
            s++;
        }
        return false;
    }
    return false;
}
