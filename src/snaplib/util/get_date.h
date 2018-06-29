#ifndef _GET_DATE_H
#define _GET_DATE_H

/*
   $Log: get_date.h,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:49:05  CHRIS
   Initial revision

*/

/* get_date returns a string defining the current date and time.  If a
   string variable is supplied, the date is written to that, otherwise
   it is written to a static area.  */

#define GETDATELEN 21     /* Length of string required to hold date */
char *get_date( char *datestr );

#endif

