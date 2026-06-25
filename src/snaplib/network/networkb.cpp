#include "snapconfig.h"
/* Routines to save and restore the network data to a BINARY_FILE */

/*
   $Log: networkb.c,v $
   Revision 1.1  1995/12/22 17:32:55  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "network/networkb.h"
#include "util/errdef.h"
#include "util/chkalloc.h"

#define SECTION_NAME "Network"

void dump_network_to_bin( network *net, BINARY_FILE *b )
{
    create_section( b, SECTION_NAME );
    dump_network( net, b->f );
    end_section( b );
}

int reload_network_from_bin( network *net, BINARY_FILE *b )
{
    network *nt;

    if( find_section( b, SECTION_NAME ) != OK ) return MISSING_DATA;

    nt = reload_network( b->f );

    if( !nt || check_end_section(b) != OK ) return INVALID_DATA;

    /* Copy the globals read in to the network structure supplied */

    memcpy( net, nt, sizeof(network) );
    check_free( nt );

    return OK;
}

