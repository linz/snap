#ifndef DBL4_UTL_TRACE_H
#define DBL4_UTL_TRACE_H
/**************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_trace.h,v 1.6 2005/04/04 23:57:58 ccrook Exp $
**
**************************************************************************
*/

#include <mi.h>

/* XADPRINTF adds "XFormAdjust" to the class name  */
#define XADPRINTF(x,y,z) DPRINTF("XFormAdjust" x, y, z)


/* Predefined trace classes and levels */
/* Note: max length of trace class is 18 chars, so suffix in XADPRINTF
   can be no more than 7 chars */

#define TRACE_CRSCALL(m)   XADPRINTF("",          20, m)
#define TRACE_XFMFUNC1(m)  XADPRINTF("",          50, m)
#define TRACE_XFMFUNC2(m)  XADPRINTF("",          60, m)
#define TRACE_SQLCMD(m)    XADPRINTF("_sql",     110, m)
#define TRACE_SQLUPD(m)    XADPRINTF("_sql",     120, m)
#define TRACE_SQLCOL(m)    XADPRINTF("_sql",     130, m)
#define TRACE_UTLFUNC(m)   XADPRINTF("_utl",     130, m)
#define TRACE_BLBMGMT(m)   XADPRINTF("_blob",    130, m)
#define TRACE_BLBREAD(m)   XADPRINTF("_blob",    140, m)
#define TRACE_ALLOC(m)     XADPRINTF("_alloc",   110, m)
#define TRACE_GRID(m)      XADPRINTF("_grid",     50, m)
/*
#define TRACE_GRID2(m)     XADPRINTF("_grid",    100, m)
#define TRACE_GRID3(m)     XADPRINTF("_grid",    200, m)
*/

#define TRACE_GRID2(m)
#define TRACE_GRID3(m)

#define TRACE_TRIG(m)       XADPRINTF("_trig",      50, m)
#define TRACE_TRIG2(m)      XADPRINTF("_trig",     100, m)
#define TRACE_LNZDEF(m)     XADPRINTF("_lnzdef",    50, m)
#define TRACE_SHIFT(m)      XADPRINTF("_shift",     50, m)
#define TRACE_SHIFT2(m)     XADPRINTF("_shift",    100, m)
#define TRACE_CALCSHIFT(m)  XADPRINTF("_cshift", 50, m)
#define TRACE_CALCSHIFT2(m) XADPRINTF("_cshift",100, m)
#define TRACE_WKB(m)        XADPRINTF("_wkb",       50, m)
#define TRACE_STATUS(m)     XADPRINTF("",           20, m)
#define TRACE_ABORT(m)      XADPRINTF("",           10, m)
#define TRACE_ADJUST(m)     XADPRINTF("",           20, m)
#define TRACE_ADJPRM(m)     XADPRINTF("_adjprm",   100, m)
#define TRACE_ADJLOAD(m)    XADPRINTF("_adjlod",    50, m)
#define TRACE_ADJLOAD2(m)   XADPRINTF("_adjlod",   100, m)
#define TRACE_ADJCALC(m)    XADPRINTF("_adjcal",    50, m)
#define TRACE_ADJCALC2(m)   XADPRINTF("_adjcal",   100, m)

/* Function entry and exit */

#define TRACE_ENTER(m)    XADPRINTF("_ep",   100, ("Entering %s",m))
#define TRACE_EXIT(m)     XADPRINTF("_ep",   100, ("Exiting  %s",m))

/* To provide quick tracing of when a point in code is passed simply put in
   a TRACEXA; statement */

#define TRACEXA          XADPRINTF("",10,("Reached %s line %d",__FILE__,(int)__LINE__))
#define TRACEXAMSG(m)   {TRACEXA; XADPRINTF("",10,m);}

#endif /* DBL4_UTL_TRACE_H not defined */
