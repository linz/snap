#ifndef DBL4_UTL_TRIG_H
#define DBL4_UTL_TRIG_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_trig.h,v 1.2 2005/04/04 23:57:59 ccrook Exp $
**
**************************************************************************
*/

static char dbl4_utl_trig_h_sccsid[] = "%W%";


#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

#ifndef DBL4_UTL_BINSRC_H
#include "dbl4_utl_binsrc.h"
#endif

typedef void *hTrig;

StatusType utlCreateTrig( hBinSrc blob, hTrig * trig);

StatusType utlReleaseTrig( hTrig trig);

StatusType utlTrigVectorDimension( hTrig trig, int *dimension );

StatusType utlTrigCoordSysDef( hTrig trig, char ** crdsys );

StatusType utlTrigTitle( hTrig trig, int nTitle, char ** title );

StatusType utlCalcTrig( hTrig trig, double x, double y, double * value);

#endif /* DBL4_UTL_TRIG_H not defined */
