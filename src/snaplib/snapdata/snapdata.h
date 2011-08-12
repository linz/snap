#ifndef _SNAPDATA_H
#define _SNAPDATA_H

/*
   $Log: snapdata.h,v $
   Revision 1.1  1995/12/22 18:48:39  CHRIS
   Initial revision

*/

#ifndef SNAPDATA_H_RCSID
#define SNAPDATA_H_RCSID "$Id: snapdata.h,v 1.1 1995/12/22 18:48:39 CHRIS Exp $"
#endif

#ifndef _DATAFILE_H
#include "util/datafile.h"
#endif

int read_snap_data( DATAFILE *d, int (*check_progress)(DATAFILE *df) );

#endif
