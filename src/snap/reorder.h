#ifndef _REORDER_H
#define _REORDER_H

/*
   $Log: reorder.h,v $
   Revision 1.1  1996/01/03 22:06:11  CHRIS
   Initial revision

*/

#define SKIP_REORDERING    0
#define FORCE_REORDERING   1
#define DEFAULT_REORDERING 2

int  init_connections( int nnodes );
void term_connections();
void add_connection( int stn1, int stn2 );
int get_connection_count( int stn );
int get_connection( int stn, int nconn );
void add_observation_parameter_count( int nprm );
int setup_parameters( FILE *lst );

#ifdef REORDER_C
int save_connections = 1;
int reorder_stations = DEFAULT_REORDERING;
double proximity_test = 0.0;
#else
extern int save_connections;
extern int reorder_stations;
extern double proximity_test;
#endif

#endif
