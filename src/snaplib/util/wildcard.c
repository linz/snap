#include "snapconfig.h"

#include <string.h>
#include "util/wildcard.h"


bool has_wildcard( const char *pattern )
{
    if( strchr(pattern,'*') ) return true;
    if( strchr(pattern,'?') ) return true;
    return false;
}

static bool wildcard_match_imp( const char *pattern, const char *s, const char *notwild )
{    
    while( *pattern && *s && 
            *pattern != '*' && 
            (  _strnicmp(pattern,s,1) == 0 ||
               (*pattern == '?' && ( ! notwild || ! strchr(notwild,*s)))
            )
         )
    {
        pattern++;
        s++;
    }
    if( ! *pattern && ! *s ) return true;
    if( *pattern == '*' ) 
    {
        const char *nw = notwild;
        while(1)
        {
            pattern++;
            if( *pattern != '*' ) break;
            nw = nullptr;
        }
        while( *s )
        {
            if( *pattern && wildcard_match_imp(pattern,s,nw) ) return true;
            if( nw && strchr(nw,*s) ) return false;
            s++;
        }
        return *pattern ? false : true;
    }
    return false;
}

bool wildcard_match( const char *pattern, const char *s )
{
    return wildcard_match_imp( pattern, s, nullptr );
}

bool filename_wildcard_match( const char *pattern, const char *filename )
{
    const char *s = filename;
    static const char *pathdelim="/\\";
    while( *s )
    {
        if( wildcard_match_imp(pattern,s,pathdelim) ) return true;
        while( *s )
        {
            if( strchr(pathdelim,*s) ){ s++; break; }
            s++;
        }
    }
    return false;
}
