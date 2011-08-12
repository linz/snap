#ifndef DBL4_UTL_GRID_H
#define DBL4_UTL_GRID_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_grid.h,v 1.5 2005/04/04 23:57:58 ccrook Exp $
**
**************************************************************************
*/

static char dbl4_utl_grid_h_sccsid[] = "%W%";


#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

#ifndef DBL4_UTL_BINSRC_H
#include "dbl4_utl_binsrc.h"
#endif

typedef void *hGrid;

StatusType utlCreateGrid( hBinSrc blob, hGrid * grid);

StatusType utlReleaseGrid( hGrid grid);

StatusType utlGridVectorDimension( hGrid grid, int *dimension );

StatusType utlGridCoordSysDef( hGrid grid, char ** crdsys );

StatusType utlGridTitle( hGrid grid, int nTitle, char ** title );

StatusType utlCalcGridLinear( hGrid grid, double x, double y, double * value);

StatusType utlCalcGridCubic( hGrid grid, double x, double y, double * value);


#endif /* DBL4_UTL_GRID_H not defined */
