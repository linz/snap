/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_blob.c,v 1.5 2005/04/04 23:57:58 ccrook Exp $
**//**
** \file
**      Generic functions for reading and writing binary objects. This
**      implements binary read routines for random access to large
**      coefficient sets, and printf style output formatting for
**      generating report type output.  These are generic routines
**      which are independent of the implementation.  The actual
**      implementation is defined by the construction of the large
**      object, which inserts function pointers into the object which
**      are used to access the actual reading/writing/seek routines.
**
**      Note: The distribution of functionality between this module and
**      the modules specific to the implementation (eg datablade, file
**      system) is messy.  This is because the original design used
**      function pointers to define a virtual interface to blob handling.
**      However this has proved to be incompatible with the datablade
**      implementation.  Now the routines directly use functions in other
**      modules.  This means that a given program can only be linked with
**      one blob implementation - not a problem in the current context.
**
*************************************************************************
*/

#include "dbl4_common.h"

#include <stdio.h>
#include <string.h>

#include "dbl4_utl_blob.h"
#include "dbl4_utl_error.h"

/*************************************************************************
** Function name: utlCreateBlobHandle
**//**
**      This routine manages the generic component of blob creation,
**      allocation and initialisation of the blob object.  This is
**      called by constructor functions, which must then install the
**      object data and the function pointers.
**
**  \param conn                Context in which to create the
**                             handle
**  \param blob                Returned handle
**
**  \return                    Return status
**
**************************************************************************
*/

StatusType utlCreateBlobHandle( DBHandle conn, hBlob * blob)
{
    (*blob) = new BlobType;
    TRACE_BLBMGMT(("utlCreateBlobHandle: Blob created with handle %llX",reinterpret_cast<unsigned long long>(*blob) ));
    return STS_OK;
}

/*************************************************************************
** Function name: utlBlobClose
**//**
**      This routine destroys a blob object using the installed function
**      pointer for any specific resource deallocation (eg closing file
**      handles)
**
**  \param blob                The blob object to destroy
**
**  \return                    Return status
**
**************************************************************************
*/

StatusType utlBlobClose( hBlob blob)
{
    TRACE_BLBMGMT(("utlBlobClose: Blob closed with handle %llX",reinterpret_cast<unsigned long long>(blob) ));
    if( blob->pvBlob )
    {
        utlReleaseBlobDB( blob->pvBlob );
    }
    delete blob;
    return STS_OK;
}


/*************************************************************************
** Function name: utlBlobReadAt
**//**
**      Routine reads a specified number of bytes from a specified offset
**      in a blob data object.
**
**  \param blob                The blob to read
**  \param lngOffset           Offset to start reading
**  \param lngBufSize          Number of bytes to read
**  \param pvBuffer            The buffer to receive the blob
**
**  \return                    Return status
**
**************************************************************************
*/

StatusType utlBlobReadAt( hBlob blob, long lngOffset, long lngBufSize,
                          void * pvBuffer)
{
    TRACE_BLBREAD(("utlBlobReadAt: Blob read - handle %llX offset %ld bytes %ld",
                   reinterpret_cast<unsigned long long>(blob), lngOffset, lngBufSize ));
    if( blob->pvBlob )
    {
        utlReadBlobDB( blob->pvBlob, lngOffset, lngBufSize, pvBuffer );
    }
    else
    {
        THROW_EXCEPTION( "utlBlobReadAt called with uninitialised hBlob" );
    }
    return STS_OK;
}



/*************************************************************************
** Function name: utlBlobWrite
**//**
**   Writes a buffer to a blob object
**
**  \param blob                The blob object
**  \param lngBufSize          The number of bytes to write
**  \param buffer              The data to write
**
**  \return                    Returns success or failure
**
**************************************************************************
*/

StatusType utlBlobWrite( hBlob blob, long lngBufSize, void *buffer)
{
    TRACE_BLBREAD(("utlBlobWrite: Blob write - handle %llX",reinterpret_cast<unsigned long long>(blob) ));
    if( blob->pvBlob )
    {
        utlWriteBlobDB( blob->pvBlob, lngBufSize, buffer);
    }
    else
    {
        THROW_EXCEPTION( "utlBlobWrite called with uninitialised hBlob" );
    }
    return STS_OK;
}


/*************************************************************************
** Function name: utlBlobSeek
**//**
**      Routine moves the read/write pointer to a specified location
**      in a blob object
**
**  \param blob                The blob object
**  \param position            The offset to move to
**  \param whence              The start point for the offset
**                             (as for regular fseek function)
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlBlobSeek( hBlob blob, long position, int whence)
{
    TRACE_BLBREAD(("utlBlobSeek: Blob handle %llX",reinterpret_cast<unsigned long long>(blob) ));
    if( blob->pvBlob )
    {
        utlSeekBlobDB( blob->pvBlob, position, whence );
    }
    else
    {
        THROW_EXCEPTION( "utlBlobSeek called with uninitialised hBlob" );
    }
    return STS_OK;
}


/*************************************************************************
** Function name: utlBlobTell
**//**
**      Routine returns the current read/write pointer position in a
**      blob object
**
**  \param blob                The blob object
**  \param position            Pointer to long receiving the offset
**
**  \return                    Returns success/failure status
**
**************************************************************************
*/

StatusType utlBlobTell( hBlob blob, long *position)
{
    TRACE_BLBREAD(("utlBlobTell: Blob handle %llX",reinterpret_cast<unsigned long long>(blob) ));
    if( blob->pvBlob )
    {
        utlTellBlobDB( blob->pvBlob, position );
    }
    else
    {
        THROW_EXCEPTION( "utlBlobTell called with uninitialised hBlob" );
    }
    return STS_OK;
}


