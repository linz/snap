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
#include <ctype.h>

#include "util/dstring.h"
#include "util/chkalloc.h"

static char rcsid[]="$Id: dstring.c,v 1.2 2004/04/22 02:35:24 ccrook Exp $";

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

void dump_string( const char *string, FILE *b )
{
    int len;
    len = string ? strlen(string) : -1;
    fwrite(&len,sizeof(len),1,b);
    if( len > 0 ) fwrite(string,len,1,b);
}

char *reload_string( FILE *b )
{
    int len;
    char *s;
    fread(&len,sizeof(len),1,b);
    if( len < 0 ) return 0;
    s = (char *) check_malloc( len+1 );
    fread( s, len, 1, b );
    s[len] = 0;
    return s;
}

int ismatch( const char *string1, const char *string2 )
{
    static const char *map =
        "________________________________"
        "_!\"#$%&'()*+,-./0123456789:;<=>?"
        "@abcdefghijklmnopqrstuvwxyz[\\]^_"
        "`abcdefghijklmnopqrstuvwxyz{|}~_"
        "________________________________"
        "________________________________"
        "________________________________"
        "________________________________";

    const char *s1;
    const char *s2;
    if( ! string1 || ! string2 ) return 0;
    for( s1 = string1, s2=string2; ; s1++, s2++ )
    {
        if( *s1 != *s2 && map[*s1] != map[*s2] ) return 0;
        if( ! *s1 ) break;
    }
    return 1;
}

