#ifndef _GEODDATA_H
#define _GEODDATA_H

/*
   $Log: geoddata.h,v $
   Revision 1.1  1995/12/22 18:45:09  CHRIS
   Initial revision

*/

#ifndef GEODDATA_H_RCSID
#define GEODDATA_H_RCSID "$Id: geoddata.h,v 1.1 1995/12/22 18:45:09 CHRIS Exp $"
#endif

#ifndef _DATAFILE_H
#include "util/datafile.h"
#endif

int read_gb_data( DATAFILE *d, int (*check_progress)( DATAFILE *d ) );

#endif
