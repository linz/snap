#ifndef PSPROJR_H
#define PSPROJR_H

/*
   $Log: psprojr.h,v $
   Revision 1.1  1995/12/22 17:00:57  CHRIS
   Initial revision

*/

#ifndef PSPROJR_H_RCSID
#define PSPROJR_H_RCSID "$Id: psprojr.h,v 1.1 1995/12/22 17:00:57 CHRIS Exp $"
#endif

void register_ps_projection( void );
projection *create_ps_projection(  double cm, double sf,
                                   double fe, double fn, char south );

#endif
