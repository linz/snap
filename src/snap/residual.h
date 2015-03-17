#ifndef _RESIDUAL_H
#define _RESIDUAL_H

/*
   $Log: residual.h,v $
   Revision 1.1  1996/01/03 22:07:02  CHRIS
   Initial revision

*/

#ifndef RESIDUAL_H_RCSID
#define RESIDUAL_H_RCSID "$Id: residual.h,v 1.1 1996/01/03 22:07:02 CHRIS Exp $"
#endif

const char *residual_flag( int unused, int rank, double sres );
void save_residual( int from, int to, int id, int type,
                    int file, int line, char unused, int rank, double sres, long loc );
void print_worst_residuals( FILE *out );

#endif
