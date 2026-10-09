#ifndef DBL4_TYPES_H
#define DBL4_TYPES_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
**  Description
**      Defines the basic types used by the datablade functions
**
** $Id: dbl4_types.h,v 1.5 2005/04/04 23:57:57 ccrook Exp $
**
**************************************************************************
*/



/* Integer types */

typedef long IdType;
typedef int  StatusType;        /* Used for function return values */

/* Date types */

struct DateTimeType
{
    double years;
    float dtSec;
    short dtYear;
    short dtMon;
    short dtDay;
    short dtHour;
    short dtMin;
};

/* Database handle types */

typedef void *DBHandle;
typedef void *DBRowHandle;

/* Integers of specific sizes */

#define INT1 signed char
#define INT2 short
#define INT4 int

#endif /* DBL4_TYPES_H not defined */
