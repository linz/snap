#ifndef _GENPARAM_H
#define _GENPARAM_H

/*
   $Log: genparam.h,v $
   Revision 1.1  1995/12/22 17:44:34  CHRIS
   Initial revision

*/

#ifndef GENPARAM_H_RCSID
#define GENPARAM_H_RCSID "$Id: genparam.h,v 1.1 1995/12/22 17:44:34 CHRIS Exp $"
#endif

#ifndef _BINFILE_H
#include "util/binfile.h"
#endif

typedef struct param_s
{
    char *name;
    unsigned int hash;
    double value;
    double covar;
    int rowno;
    unsigned char flags;
    int identical;
} param;

#define PRM_ADJUST 0x01  /* Flags that parameter is to be adjusted */
#define PRM_USED   0x02  /* Flags that data have been used... */
#define PRM_LISTED 0x04  /* Flags that parameters have already been listed */

int define_param( char *name, double value, int adjust );
int find_param( char *name );
void   flag_param_used( int p);
void   flag_param_listed( int p );
double param_value( int p );
void update_param_value( int p, double v, double var );
char *param_name( int p );
int param_rowno( int p );
int identical_param( int p );
int param_count( void );
param * param_from_id( int pid );
int sorted_param_id( int n );

void define_param_value( int p, double value, int adjust );
void wildcard_param_value( char *name, double value, int adjust );
void define_param_match( int p1, int p2 );
void wildcard_param_match( char *name, int p );

int init_param_rowno( int nextprm );
int find_param_row( int row, char *name, int nlen );

void clear_param_list( void );
void dump_parameters( BINARY_FILE *b );
int reload_parameters( BINARY_FILE *b );

#endif
