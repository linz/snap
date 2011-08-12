#ifndef TMPROJR_H
#define TMPROJR_H

/*
   $Log: tmprojr.h,v $
   Revision 1.1  1995/12/22 17:02:15  CHRIS
   Initial revision

*/

#ifndef TMPROJR_H_RCSID
#define TMPROJR_H_RCSID "$Id: tmprojr.h,v 1.1 1995/12/22 17:02:15 CHRIS Exp $"
#endif

void register_tm_projection( void );
projection *create_tm_projection(  double cm, double sf, double lto,
                                   double fe, double fn, double utom );

#endif
