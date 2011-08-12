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

#include "util/dstring.h"
#include "util/chkalloc.h"

static char rcsid[]="$Id: dstring.c,v 1.2 2004/04/22 02:35:24 ccrook Exp $";

char *copy_string( const char *string )
{
    char *s;

    if( ! string ) return 0;
    s = (char *) check_malloc( strlen(string) + 1 );
    strcpy( s, string );
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

