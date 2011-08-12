#ifndef DBL4_UTL_DATE_H
#define DBL4_UTL_DATE_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_date.h,v 1.3 2003/01/06 00:43:24 ccrook Exp $
**
**************************************************************************
*/

static char dbl4_utl_date_h_sccsid[] = "%W%";


#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

void utlSetDate( DateTimeType *dt, int year, int month, int day);

void utlSetDateTime( DateTimeType *dt, int year, int month, int day, int hour,
                     int min, float sec);

void utlCopyDate( DateTimeType *tgt, DateTimeType *src);

StatusType utlParseDate( DateTimeType *tgt, char *str);

int utlCompareDate( DateTimeType *dt1, DateTimeType *dt2 );

int utlIsLeapYear( int date );

double utlDateAsYear( DateTimeType *date );

#endif /* DBL4_UTL_DATE_H not defined */
