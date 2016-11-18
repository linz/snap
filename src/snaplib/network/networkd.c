#include "snapconfig.h"
/*
   $Log: networkd.c,v $
   Revision 1.2  1998/05/21 04:00:26  ccrook
   Added geodetic coordinate system to network object and facilitated getting and setting
   coordinates in the network coordinate system.

   Revision 1.1  1995/12/22 17:36:13  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "util/dstring.h"
#include "util/errdef.h"


/*=============================================================*/
/* Routines to dump and reload a network to a binary data file */


void dump_network( network *nw, FILE *f )
{

    /* Dump the station file data */

    dump_string( nw->name, f );
    dump_string( nw->crdsysdef, f );
    fwrite(&nw->topolat, sizeof(nw->topolat), 1, f );
    fwrite(&nw->topolon, sizeof(nw->topolon), 1, f );
    fwrite(&nw->got_topocentre, sizeof(nw->got_topocentre), 1, f );
    fwrite(&nw->options, sizeof(nw->options), 1, f );
    fwrite(&nw->orderclsid, sizeof(nw->orderclsid), 1, f );

    /* Dump the station list and coordinate system */

    dump_classifications( &(nw->stnclasses), f);
    dump_station_list( nw->stnlist, f );
}



network *reload_network( FILE *f )
{
    network *nw;
    coordsys *cs;

    /* Restore the critical static information */

    nw = new_network();
    nw->name = reload_string( f );
    nw->crdsysdef = reload_string( f );
    fread(&nw->topolat, sizeof(nw->topolat), 1, f );
    fread(&nw->topolon, sizeof(nw->topolon), 1, f );
    fread(&nw->got_topocentre, sizeof(nw->got_topocentre), 1, f );
    fread(&nw->options, sizeof(nw->options), 1, f );
    fread(&nw->orderclsid, sizeof(nw->orderclsid), 1, f );

    /* Restore the station list and coordinate system */

    cs = load_coordsys( nw->crdsysdef );

    if( !cs )
    {
        /* Really want some better error handling here */
        delete_network( nw );
        nw = NULL;
    }
    else
    {
        set_network_coordsys( nw, cs, 0.0, 0, 0, 0 );
        delete_coordsys( cs );
        reload_classifications( &(nw->stnclasses), f );
        nw->stnlist = reload_station_list( f );
    }

    return nw;
}



#ifdef TESTST

#include <alloc.h>

int main( int argc, char *argv[] )
{

    FILE *bin;
    network *nw;
    int sts;

    define_coordsys_file( argv[0] );

    if( argc < 2 )
    {
        printf("Need one or two file name parameters\n");
        return 0;
    }

    bin = snaptmpfile();

    printf("Before creation mem = %ld\n",(long) coreleft() );

    nw = new_network();
    sts = read_network( nw, argv[1], 0 );
    printf("Read status = %d\n",sts);
    if( sts != OK ) return 0;

    printf("After reading = %ld\n",(long) coreleft() );

    dump_network( nw, bin );
    delete_network( nw );

    printf("After deleting mem = %ld\n",(long) coreleft() );

    fseek( bin, 0L, SEEK_SET );
    nw = reload_network( bin );

    printf("After reloading = %ld\n",(long) coreleft() );

    if( argc > 2 )
    {
        write_network( nw, argv[2], "TESTST", "today" );
    }
    delete_network( nw );

    printf("After final deletion = %ld\n",(long) coreleft() );

    fseek( bin, 0L, SEEK_SET );
    nw = reload_network( bin );

    printf("After reloading = %ld\n",(long) coreleft() );


    if( argc > 2 )
    {
        FILE *out;
        out = fopen( argv[2], "w" );
        fclose(out);
    }
    delete_network( nw );

    printf("After final deletion = %ld\n",(long) coreleft() );
    return 0;
}




#endif
