#ifndef NETWORKB_H
#define NETWORKB_H

/*
   $Log: networkb.h,v $
   Revision 1.1  1995/12/22 17:35:39  CHRIS
   Initial revision

*/

#include "network/network.h"
#include "util/binfile.h"

void dump_network_to_bin( network *net, BINARY_FILE *b );
int reload_network_from_bin( network *net, BINARY_FILE *b );

#endif
