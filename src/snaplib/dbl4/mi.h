#ifndef MI_H
#define MI_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
**  Description:
**      Header used for testing the dbl4_XFormAdjust datablade functions
**
**  History:
**      Date        Initials           Comment
**      10/05/2000  Chris Crook        Created
**
** $Id: mi.h,v 1.1 2003/05/28 01:40:48 ccrook Exp $
**
**************************************************************************
*/

#include <stdlib.h>

typedef int mi_boolean;
typedef long mi_integer;
typedef char mi_lvarchar;
typedef char mi_string;
typedef double mi_double_precision;
typedef void *MI_CONNECTION;
typedef void *MI_ROW;
typedef void *MI_ROW_DESC;
typedef void *MI_DATUM;
typedef long MI_LO_HANDLE;

#define MI_NORMAL_VALUE 0
#define MI_ERROR 1
#define MI_NULL_VALUE 2

typedef struct
{
    void *data;
} MI_FPARAM;

#define mi_fp_setfuncstate(p,x) (p)->data = (x)
#define mi_fp_funcstate(p) p->data

#define mi_lvarchar_to_string(x) x
#define mi_alloc malloc
#define mi_dalloc(x,y) malloc(x)
#define mi_free(x) free(x)

#define MI_SQL 0
#define MI_TRUE -1
#define MI_FALSE 0

#define mi_fp_argisnull(x,y) 0
#define mi_fp_setreturnisnull(x,y,z)
#define mi_open(x,y,z) 0


void start_trace( char *c, long l );
void add_trace( char *fmt, ... );
void end_trace( void );

#define DPRINTF(c,l,a) { \
        start_trace( c, l ); \
        add_trace a;         \
        end_trace();         \
        }

#define UDREXPORT

#endif
