#include "snapconfig.h"
/*
   $Log: netstns1.c,v $
   Revision 1.1  1995/12/22 17:31:54  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "util/dstring.h"
#include "util/chkalloc.h"

static char rcsid[]="$Id: netstns1.c,v 1.1 1995/12/22 17:31:54 CHRIS Exp $";

/* Procedure to dump the station coordinates to a binary file */
/* Flags are dumped separately to improve binary file compatibility
   between different compilers */


void dump_station( station *st, FILE *f )
{
    fwrite(st,sizeof(station)-sizeof(st->classval)-sizeof(st->Name)-sizeof(st->hook),1,f);
    if( st->nclass > 0 ) fwrite( st->classval, sizeof(int), st->nclass, f );
    dump_string( st->Name, f );
}

station *reload_station( FILE *f )
{
    station *st;
    int nclass;
    st = new_station();
    fread(st,sizeof(station)-sizeof(st->classval)-sizeof(st->Name)-sizeof(st->hook),1,f);
    nclass = st->nclass;
    if( nclass > 0 )
    {
        st->nclass = 0;
        st->classval = 0;
        init_station_classes( st, nclass );
        fread( st->classval, sizeof(int), nclass,f );
    }
    st->Name = reload_string( f );
    return st;
}

