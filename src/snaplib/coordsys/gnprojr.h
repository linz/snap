#ifndef GNPROJR_H
#define GNPROJR_H

/*
   $Log: gnprojr.h,v $
   Revision 1.1  1995/12/22 16:53:59  CHRIS
   Initial revision

*/

#ifndef GNPROJR_H_RCSID
#define GNPROJR_H_RCSID "$Id: gnprojr.h,v 1.1 1995/12/22 16:53:59 CHRIS Exp $"
#endif

void register_gnomic_projection( void );
projection *create_gnomic_projection(  double orglat, double orglon,
                                       double fe, double fn );

#endif
