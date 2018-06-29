#ifndef GNPROJR_H
#define GNPROJR_H

/*
   $Log: gnprojr.h,v $
   Revision 1.1  1995/12/22 16:53:59  CHRIS
   Initial revision

*/

void register_gnomic_projection( void );
projection *create_gnomic_projection(  double orglat, double orglon,
                                       double fe, double fn );

#endif
