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

using mi_boolean = int;
using mi_integer = long;
using mi_lvarchar = char;
using mi_string = char;
using mi_double_precision = double;
using MI_CONNECTION = void *;
using MI_ROW = void *;
using MI_ROW_DESC = void *;
using MI_DATUM = void *;
using MI_LO_HANDLE = long;

#define MI_NORMAL_VALUE 0
#define MI_ERROR 1
#define MI_NULL_VALUE 2

struct MI_FPARAM
{
    void *data;
};

#define mi_fp_setfuncstate(p,x) (p)->data = (x)
#define mi_fp_funcstate(p) p->data

#define mi_lvarchar_to_string(x) x

#define MI_SQL 0
#define MI_TRUE -1
#define MI_FALSE 0

#define mi_fp_argisnull(x,y) 0
#define mi_fp_setreturnisnull(x,y,z)
#define mi_open(x,y,z) 0


void start_trace( const char *c, long l );
void add_trace( const char *fmt, ... );
void end_trace( void );

#define DPRINTF(c,l,a) { \
        start_trace( c, l ); \
        add_trace a;         \
        end_trace();         \
        }

#define UDREXPORT

#endif
