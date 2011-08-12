#ifndef ISOMETRC_H
#define ISOMETRC_H

/*
   $Log: isometrc.h,v $
   Revision 1.1  1995/12/22 16:54:43  CHRIS
   Initial revision

*/

#ifndef ISOMETRC_H_RCSID
#define ISOMETRC_H_RCSID "$Id: isometrc.h,v 1.1 1995/12/22 16:54:43 CHRIS Exp $"
#endif

double isometric_from_geodetic( double lat, double e );
double geodetic_from_isometric( double q, double e );

#endif
