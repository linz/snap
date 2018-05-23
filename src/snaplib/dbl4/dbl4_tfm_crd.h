#ifndef DBL4_TFM_CRD_H
#define DBL4_TFM_CRD_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_tfm_crd.h,v 1.3 2003/01/06 00:43:22 ccrook Exp $
**
**************************************************************************
*/




#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

/* Structure used to define a coordinate in the blade */

typedef struct
{
    double ordinate[3];       /**< COO.VALUE_n or ODT.VALUE_n */
    Boolean blnDefined[3];    /**< True if the ordinate type is defined */
    Boolean blnHeightUnknown; /**< Height invented by setting to 0  - applies
                                 for reference datum coordinates only*/
} CrdType, *hCrd;

/* Definitions of convention for datablade ordinate storage */

#define CRD_E   0
#define CRD_N   1
#define CRD_LON 0
#define CRD_LAT 1
#define CRD_HGT 2
#define CRD_X   0
#define CRD_Y   1
#define CRD_Z   2


#endif /* DBL4_TFM_CRD_H not defined */
