#ifndef DBL4_UTL_LNZDEF_H
#define DBL4_UTL_LNZDEF_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_lnzdef.h,v 1.2 2005/04/04 23:57:58 ccrook Exp $
**
**************************************************************************
*/

static char dbl4_utl_linzdef_h_sccsid[] = "%W%";


#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

#ifndef DBL4_UTL_BINSRC_H
#include "dbl4_utl_binsrc.h"
#endif

typedef void *hLinzDefModel;

StatusType utlCreateLinzDef( hBinSrc blob, hLinzDefModel *def );

StatusType utlReleaseLinzDef( hLinzDefModel def );

StatusType utlLinzDefCoordSysDef( hLinzDefModel def, char ** crdsys );

/* nTitle = 1 for name, 2 for description, 3 for version */

StatusType utlLinzDefTitle( hLinzDefModel def, int nTitle, char ** title );

StatusType utlCalcLinzDef( hLinzDefModel def, double date, double x, double y,
                           double * value);

#endif /* DBL4_UTL_LNZDEF_H not defined */
