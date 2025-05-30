#ifndef DATASTAT_H
#define DATASTAT_H

/*
   $Log: datastat.h,v $
   Revision 1.1  1996/02/23 17:00:02  CHRIS
   Initial revision

*/

void set_mde_level( double sig, double power );
double calculate_mde( double sobs, double sres );
double residual_significance( double sres, int used, int rank );
double prob_of_maximum( double prob, int rank );

#endif
