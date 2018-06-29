#ifndef TMPROJR_H
#define TMPROJR_H

/*
   $Log: tmprojr.h,v $
   Revision 1.1  1995/12/22 17:02:15  CHRIS
   Initial revision

*/

void register_tm_projection( void );
projection *create_tm_projection(  double cm, double sf, double lto,
                                   double fe, double fn, double utom );

#endif
