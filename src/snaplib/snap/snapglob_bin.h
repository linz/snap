#ifndef _SNAPGLOB_BIN_H
#define _SNAPGLOB_BIN_H

#include "util/binfile.h"

void dump_snap_globals( BINARY_FILE *b );
int reload_snap_globals( BINARY_FILE *b );
void dump_obs_classes( BINARY_FILE *b );
int reload_obs_classes( BINARY_FILE *b );

#endif
