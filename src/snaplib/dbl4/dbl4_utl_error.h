#ifndef DBL4_UTL_ERROR_H
#define DBL4_UTL_ERROR_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_error.h,v 1.3 2003/01/06 00:43:24 ccrook Exp $
**
**************************************************************************
*/

#include <stdio.h>

/* Require data types */

#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

#ifndef DBL4_UTL_TRACE_H
#include "dbl4_utl_trace.h"
#endif

/* Error status codes */

#define STS_OK               0

#define STS_NO_CONNECTION  100
#define STS_INVALID_CONN   101
#define STS_ALLOC_FAILED   102
#define STS_INVALID_HANDLE 103
#define STS_INVALID_ID     104
#define STS_TOO_MUCH_DATA  105
#define STS_MISSING_DATA   106
#define STS_INVALID_DATA   107
#define STS_END_OF_DATA    108
#define STS_USER_ABORT     109
#define STS_REPORTED_ERROR 110
#define STS_TRUNCATED      111

#define STS_INVALID_PRM    200
#define STS_MISSING_PRM    201
#define STS_INVALID_FDATA  202
#define STS_MISSING_FDATA  203
#define STS_INVALID_FUNC   204
#define STS_MISSING_FUNC   205
#define STS_CRD_TYPE_ERR   206
#define STS_CRD_RANGE_ERR  207
#define STS_CRD_MISSING    208
#define STS_ORD_MISSING    209
#define STS_TFM_FAILED     210
#define STS_ADJ_SINGULAR   211
#define STS_ADJ_DIVERGED   212
#define STS_ADJ_FAILEDTEST 213
#define STS_XFM_INVERTED   214

#define STS_PROGRAM_ERROR 400

/* Function to abort the program */

#define THROW_EXCEPTION(m) \
   { TRACE_ABORT(( "%s: at %s line %d",m,__FILE__,(int)__LINE__)); \
    { char msg[1024]; \
      sprintf(msg,"Abort: %.100s at %.30s line %d",m,__FILE__,(int)__LINE__); \
      utlAbort(msg); \
    }}

/* Assertion macro - can be replaced with empty macro in production */

#ifndef NDEBUG
#define ASSERT(c,m) { if( ! (c) ) { THROW_EXCEPTION(m); }}
#else
#define ASSERT(c,m)
#endif

/* Function to report the changes to the error status */

#define REPORT_STATUS(x) \
     TRACE_STATUS(("Status set to %d at line %d of %s", \
                    x,__LINE__,__FILE__))

#define RETURN_STATUS(x) { if( (x) != STS_OK ) REPORT_STATUS(x); return x; }
#define SET_STATUS(v,x) { if( (x) != STS_OK ) REPORT_STATUS(x); v = x; }

/* Function to exit in the event of an unmanageable error */

void utlAbort( char *message );

#endif /* DBL4_UTL_ERROR_H not defined */
