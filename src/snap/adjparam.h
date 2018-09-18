#ifndef _ADJPARAM_H
#define _ADJPARAM_H

/*
   $Log: adjparam.h,v $
   Revision 1.1  1996/01/03 21:55:55  CHRIS
   Initial revision

*/

void set_param_obseq( int p, void *hA, int irow, double v );
void update_params( int get_covariance );

void list_calculated_parameters( FILE *out );
void print_adjusted_parameters( FILE *out );

#endif
