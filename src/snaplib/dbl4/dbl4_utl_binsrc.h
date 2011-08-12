#ifndef DBL4_UTL_BINSRC_H
#define DBL4_UTL_BINSRC_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_binsrc.h,v 1.2 2005/04/04 23:57:58 ccrook Exp $
**
**************************************************************************
*/

static char dbl4_utl_binsrc_h_sccsid[] = "%W%";

#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

#ifndef DBL4_UTL_BLOB_H
#include "dbl4_utl_blob.h"
#endif

typedef struct
{
    hBlob blob;           /**< The blob object from which to read data */
    long offset;          /**< The offset from which to read the next data */
    long seek_offset;     /**< Offset used for embedded binsrc objects */
    int src_endian;       /**< The endianness of the source data - 1=big */
    int arch_endian;      /**< The endianness of the architecture - 1=big */
    int swap_bytes;       /**< True if the byte order needs to be reversed */
} BinSrc, *hBinSrc;

/* The offset value to specify to continue reading from the last read
   statement */

#define BINSRC_CONTINUE -1
#define BINSRC_MAX_STRING_LEN 1024

#define INT1 signed char
#define INT2 short
#define INT4 int

StatusType utlCreateBinSrc( hBlob blob, hBinSrc * binsrc);
StatusType utlCreateEmbeddedBinSrc( hBinSrc binsrc, long offset, hBinSrc *embsrc);
StatusType utlReleaseBinSrc( hBinSrc binsrc);

StatusType utlBinSrcSetBigEndian( hBinSrc binsrc, int bigEndian );

StatusType utlBinSrcLoad1( hBinSrc binsrc, long offset, int nval, void *data );
StatusType utlBinSrcLoad2( hBinSrc binsrc, long offset, int nval, void *data );
StatusType utlBinSrcLoad4( hBinSrc binsrc, long offset, int nval, void *data );
StatusType utlBinSrcLoad8( hBinSrc binsrc, long offset, int nval, void *data );
StatusType utlBinSrcLoadString( hBinSrc binsrc, long offset, char **data );

#endif /* DBL4_UTL_BINSRC_H not defined */
