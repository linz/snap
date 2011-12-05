#ifndef DBL4_UTL_SHIFTMODEL_H
#define DBL4_UTL_SHIFTMODEL_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
**************************************************************************
*/

static char dbl4_utl_shiftmodel_h_sccsid[] = "%W%";


#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

#ifndef DBL4_UTL_BINSRC_H
#include "dbl4_utl_binsrc.h"
#endif

/* Note: code assumes headers are all the same length */

#define SHIFTMODEL_FILE_HEADER_1 "LINZ shift model v1.0L\r\n\x1A"
#define SHIFTMODEL_FILE_HEADER_2 "LINZ shift model v1.0B\r\n\x1A"

typedef void *hPointShiftModel;

StatusType utlCreateShiftModel( hBinSrc blob, hPointShiftModel *def );

StatusType utlReleaseShiftModel( hPointShiftModel def );

StatusType utlShiftModelCoordSysDef( hPointShiftModel def, char ** crdsys );

/* nTitle = 1 for name, 2 for description, 3 for version */

StatusType utlShiftModelTitle( hPointShiftModel def, int nTitle, char ** title );

StatusType utlShiftModelRange( hPointShiftModel def,
                               double *xmin, double *xmax, double *ymin, double *ymax );

StatusType utlCalcShiftModel( hPointShiftModel def, double x, double y,
                              double * value);

StatusType utlApplyShiftModel( hPointShiftModel def, double *crd );

#endif /* DBL4_UTL_SHIFTMODEL_H not defined */
