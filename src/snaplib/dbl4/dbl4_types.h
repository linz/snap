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



/* Character array types */

#define SYSCODE_LEN 4
#define NAME_LEN    100
#define STRING_LEN  255

typedef char SysCodeType[SYSCODE_LEN+1];
typedef char NameType[NAME_LEN+1];
typedef char StringType[STRING_LEN+1];

/* Integer types */

typedef unsigned char Boolean;
typedef long IdType;
typedef int  StatusType;        /* Used for function return values */

#define BLN_FALSE ((Boolean) 0)
#define BLN_TRUE  ((Boolean) 1)

/* Date types */

typedef struct
{
    double years;
    float dtSec;
    short dtYear;
    short dtMon;
    short dtDay;
    short dtHour;
    short dtMin;
}  DateTimeType;

/* Database handle types */

typedef void *DBHandle;
typedef void *DBRowHandle;

/* Integers of specific sizes */

#define INT1 signed char
#define INT2 short
#define INT4 int

#endif /* DBL4_TYPES_H not defined */
