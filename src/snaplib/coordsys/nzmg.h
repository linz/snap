#ifndef _NZMG_H
#define _NZMG_H

/*
   $Log: nzmg.h,v $
   Revision 1.1  1995/12/22 16:57:06  CHRIS
   Initial revision

*/

#ifndef NZMG_H_RCSID
#define NZMG_H_RCSID "$Id: nzmg.h,v 1.1 1995/12/22 16:57:06 CHRIS Exp $"
#endif

#define NZMG_A  6378388.0
#define NZMG_RF 297.0

void nzmg_geod( double e, double n, double *lt, double *ln );
void geod_nzmg( double lt, double ln, double *e, double *n );

#endif
