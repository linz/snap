#ifndef _PROBFUNC_H
#define _PROBFUNC_H

/*
   $Log: probfunc.h,v $
   Revision 1.3  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  1998/06/15 02:22:16  ccrook
   Modified to handle long count parameters

   Revision 1.1  1995/12/22 19:54:30  CHRIS
   Initial revision

*/

/* Cumulative probability distribution functions and their inverses */
/* These functions deal with the probability of obtaining a value of
   x greater than that specified */

double normal_distn( double value ) ;
double inv_normal_distn( double prob ) ;
double chi2_distn( double value, long dof ) ;
double inv_chi2_distn( double prob, long dof ) ;
double students_t_distn( double value, long dof ) ;
double inv_students_t_distn( double prob, long dof ) ;
double f_distn( double value, long dofn, long dofd ) ;
double inv_f_distn( double prob, long dofn, long dofd ) ;
double tau_distn( double value, long dof );
double inv_tau_distn( double prob, long dof );

#endif

