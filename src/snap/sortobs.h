
/*
   $Log: sortobs.h,v $
   Revision 1.1  1996/01/03 22:11:34  CHRIS
   Initial revision

*/

#ifndef SORTOBS_H_RCSID
#define SORTOBS_H_RCSID "$Id: sortobs.h,v 1.1 1996/01/03 22:11:34 CHRIS Exp $"
#endif
/* sortobs.h - header file for routines which manage sorting of observations */
#ifndef _SORTOBS_H
#define _SORTOBS_H

void save_observation( int from, int to, int type, long loc );
void sort_observation_list( void );
void init_get_sorted_obs_loc( void );
long get_sorted_obs_loc( void );

#ifdef SORTOBS_C
int sort_obs = 0;
#else
extern int sort_obs;
#endif

#define SORTED_OBS    1
#define SORT_BY_LINE  2
#define SORT_BY_TYPE  4

#endif
