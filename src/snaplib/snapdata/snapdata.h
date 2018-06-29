#ifndef _SNAPDATA_H
#define _SNAPDATA_H

/*
   $Log: snapdata.h,v $
   Revision 1.1  1995/12/22 18:48:39  CHRIS
   Initial revision

*/

#ifndef _DATAFILE_H
#include "util/datafile.h"
#endif

int read_snap_data( DATAFILE *d, int (*check_progress)(DATAFILE *df) );

#endif
