#ifndef DBL4_UTL_PROGRESS_H
#define DBL4_UTL_PROGRESS_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
**      Function for reporting progress of the adjustment.
**
** $Id: dbl4_utl_progress.h,v 1.3 2003/01/06 00:43:25 ccrook Exp $
**
**************************************************************************
*/

#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

static char dbl4_utl_progress_h_sccsid[] = "%W%";

#define PROG_TEMP_MSG -1
#define PROG_NO_BAR   -2

StatusType utlShowProgress( char *state, int percent );

#endif

