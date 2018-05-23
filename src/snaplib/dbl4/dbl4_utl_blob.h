#ifndef DBL4_UTL_BLOB_H
#define DBL4_UTL_BLOB_H
/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_blob.h,v 1.3 2003/01/06 00:43:24 ccrook Exp $
**
**************************************************************************
*/

/* Require data types */

#include "dbl4_types.h"

/* Data types used to define a generic binary data source.  DataType stores
   a pointer to an undefined structure defining a data source, and provides
   methods to read and destroy the data.  These methods were defined as
   function pointers in the hBlob, but are now simple function calls linked
   to the implementation.
*/

StatusType utlReadBlobDB( void *pvBlob, long lngOffset,
                          long lngBufSize, void *pvBuffer );

void utlReleaseBlobDB( void *pvBlob );

StatusType utlSeekBlobDB( void *pvBlob, long position, int whence );

StatusType utlTellBlobDB( void *pvBlob, long *position );

StatusType utlWriteBlobDB( void *blob, long lngBufSize, void *pvBuffer );

typedef struct
{
    void *pvBlob;                /**< Data blob read/write functions */
    char *buffer;                /**< Used for formatted output routines */
} BlobType, *hBlob;

#define BLOB_SEEK_SET    0
#define BLOB_SEEK_CUR    1
#define BLOB_SEEK_END    2

/* Generic functions using the BlobType object.  The utlCreateHandle function
   allocates the BlobType object, but doesn't set any useful information within
   it - all data is set to NULL.  This is to be used by specific blob
   constructor functions.  If the blnOutput is set then a write buffer is
   created for the Printf function.
*/

StatusType utlCreateBlobHandle( DBHandle conn, hBlob * blob, Boolean blnOutput );

StatusType utlBlobClose( hBlob blob);

StatusType utlBlobReadAt( hBlob blob, long lngOffset, long lngBufSize,
                          void * pvBuffer);

StatusType utlBlobWrite( hBlob blob, long lngBufSize, void *buffer );

StatusType utlBlobSeek( hBlob blob, long position, int whence );

StatusType utlBlobTell( hBlob blob, long *position );

/* Note that the printf function is constrained to the size of buffer -
   use with care! */

StatusType utlBlobPrintf( hBlob blob, char *format, ... );

#endif /* DBL4_UTL_BLOB_H not defined */
