#include "snapconfig.h"
/* Routines to save and restore the network data to a BINARY_FILE */

/*
   $Log: networkb.c,v $
   Revision 1.1  1995/12/22 17:32:55  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <utility>

#include "network/networkb.h"
#include "util/errdef.h"

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

    /* Move the globals read in to the network structure supplied. net is
       always freshly-constructed here (every real caller does
       net = new_network() immediately before this call) - swapping is
       safe specifically because of that: network cannot be copied, so
       std::swap exchanges the two through its move constructor and move
       assignment. Since net starts empty, nt ends up holding net's old
       (empty) state afterward, safe to delete, while net ends up the
       sole owner of nt's real data. */

    std::swap( *net, *nt );
    delete nt;

    return OK;
}

