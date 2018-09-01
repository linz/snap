#include "snapconfig.h"
/* netlist1.c: Dump and reload station list from a binary file */

/*
   $Log: netlist1.c,v $
   Revision 1.1  1995/12/22 17:30:10  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "util/linklist.h"
#include "util/chkalloc.h"
#include "util/errdef.h"


void dump_station_list( station_list *sl, FILE *f )
{
    station *st;

    /* Dump the critical information */

    fwrite(&sl->count, sizeof(sl->count), 1, f );

    /* Dump the station definitions */

    sl_reset_station_list( sl, 0 );

    while( NULL != (st = sl_next_station( sl ) )  )
    {
        dump_station( st, f );
    }
}



station_list *reload_station_list( FILE *f )
{
    station_list *sl;
    station *st;
    int istn;

    /* Restore the critical static information */

    fread(&istn, sizeof( istn ), 1, f );

    /* Dump the station definitions */

    sl = new_station_list();
    while( istn-- )
    {
        st = reload_station( f );
        sl_add_station_at_id( sl, st );

    }

    return sl;
}
