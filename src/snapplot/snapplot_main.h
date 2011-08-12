#ifndef SNAPPLOT_H
#define SNAPPLOT_H

/*
   $Log: snapplot.h,v $
   Revision 1.1  1996/01/03 22:31:26  CHRIS
   Initial revision

*/

#ifndef SNAPPLOT_H_RCSID
#define SNAPPLOT_H_RCSID "$Id: snapplot.h,v 1.1 1996/01/03 22:31:26 CHRIS Exp $"
#endif

#include <stdio.h>    /* For definition of FILE */
#include "snapdata/datatype.h"  /* For definition of NOBSTYPE */

#ifdef MAIN
#define SCOPE
#else
#define SCOPE extern
#endif

SCOPE char highlight_all_connections;

#undef SCOPE


#define NTEXTLINES 4

#endif


