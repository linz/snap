#include "snapconfig.h"
/* strarray.c: Management of dynamically allocated list of strings */


#include <stdio.h>
#include <string.h>

#include "util/strarray.h"
#include "util/dstring.h"
#include "util/chkalloc.h"

void strarray_init( strarray *stra )
{
    stra->strings = NULL;
    stra->nstrings = 0;
    stra->maxstrings = 0;
}

void strarray_delete( strarray *stra )
{
    if( stra->strings )
    {
        if( stra->nstrings > 0 )
        {
            int i;
            for( i = 0; i < stra->nstrings; i++ )
            {
                check_free( stra->strings[i] );
            }
        }
        check_free( stra->strings );
    }
    strarray_init( stra );
}

int strarray_count( strarray *stra )
{
    return stra->nstrings;
}

int strarray_find( strarray *stra, const char *string )
{
    int i;
    if( ! stra ) return -1;

    for( i = 0; i < stra->nstrings; i++ )
    {
        if( _stricmp( string, stra->strings[i] ) == 0 ) return i;
    }
    return STRARRAY_NOT_FOUND;
}

static int strarray_add_ptr( strarray *stra, const char *string )
{
    if( stra->nstrings >= stra->maxstrings )
    {
        int i;
        char **newstrings;
        int nstr = stra->nstrings;
        int nmax = stra->maxstrings * 2;
        if( nmax <= 0 ) nmax = 64;
        newstrings = (char **) check_malloc( nmax * sizeof( char *) );
        for( i = 0; i < nstr; i++ )
        {
            newstrings[i] = stra->strings[i];
        }
        check_free( stra->strings );
        stra->strings = newstrings;
        stra->maxstrings = nmax;
    }
    stra->strings[stra->nstrings] = copy_string( string );
    stra->nstrings++;
    return stra->nstrings - 1;
}

int strarray_add( strarray *stra, const char *string )
{
    return strarray_add_ptr( stra, copy_string( string ) );
}

const char *strarray_get( strarray *stra, int i )
{
    if( stra && i >= 0 && i < stra->nstrings )
    {
        return stra->strings[i];
    }
    return NULL;
}

void dump_strarray( strarray *stra, FILE *f )
{
    int i;
    int nstr = stra->nstrings;
    fwrite( &nstr, sizeof(nstr), 1, f );
    for( i = 0; i < stra->nstrings; i++ )
    {
        dump_string( stra->strings[i], f  );
    }
}

void reload_strarray( strarray *stra, FILE *f )
{
    int nstr;
    int i;
    strarray_delete( stra );
    fread( &nstr, sizeof( nstr ), 1 ,f );
    for( i = 0; i < nstr; i++ )
    {
        char *c = reload_string( f );
        if( c ) strarray_add_ptr( stra, c );
    }
}

