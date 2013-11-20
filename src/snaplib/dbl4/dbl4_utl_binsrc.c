/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_binsrc.c,v 1.2 2005/04/04 23:57:58 ccrook Exp $
**//**
** \file
**      Functions for managing a source of binary data.  This manages
**      loading of arrays of 1,2,4, and 8 byte data, and strings which
**      comprise a 2 byte integer character count followed by a set of
**      characters.
**
**      The binary format is portable between SUN and Intel DOS/Windows
**      environments which differ only in endianness
**
*************************************************************************
*/

static char sccsid[] = "%W%";

#include "dbl4_common.h"

#include <stdlib.h>
#include <string.h>

#include "dbl4_utl_binsrc.h"

#include "dbl4_utl_alloc.h"
#include "dbl4_utl_error.h"
#include "dbl4_utl_blob.h"


/*************************************************************************
** Function name: swap2
**//**
**       Reverses the byte order in a 2 byte buffer
**
**  \param b                   The buffer to reverse
**
**  \return
**
**************************************************************************
*/

static void swap2( unsigned char *b )
{
    unsigned char tmp;
    tmp = b[0];
    b[0] = b[1];
    b[1] = tmp;
}



/*************************************************************************
** Function name: swap4
**//**
**       Reverses the byte order in a 4 byte buffer
**
**  \param b                   The buffer to reverse
**
**  \return
**
**************************************************************************
*/

static void swap4( unsigned char *b )
{
    unsigned char tmp;
    tmp = b[0];
    b[0] = b[3];
    b[3] = tmp;
    tmp = b[1];
    b[1] = b[2];
    b[2] = tmp;
}



/*************************************************************************
** Function name: swap8
**//**
**       Reverses the byte order in a 8 byte buffer
**
**  \param b                   The buffer to reverse
**
**  \return
**
**************************************************************************
*/

static void swap8( unsigned char *b )
{
    unsigned char tmp;
    tmp = b[0];
    b[0] = b[7];
    b[7] = tmp;
    tmp = b[1];
    b[1] = b[6];
    b[6] = tmp;
    tmp = b[2];
    b[2] = b[5];
    b[5] = tmp;
    tmp = b[3];
    b[3] = b[4];
    b[4] = tmp;
}



/*************************************************************************
** Function name: check_endian
**//**
**          Function to test the endian-ness of the machine on which it is
**      executed.  It does this crudely by converting a short integer to
**      a character buffer, and then testing the value of the first
**      character.
**
**  \param big_wanted          True if want to use big-endian data,
**                             false otherwise
**
**  \return                    Returns true if the byte order needs
**                             swapping.
**
**************************************************************************
*/

static int check_endian( int big_wanted)
{
    unsigned short test = 0x01;
    unsigned char *b;
    int swap_bytes;
    b = (unsigned char *) (&test);
    /* If little endian then b[0] = 1, else b[0] = 0 */
    swap_bytes = b[0];
    if( ! big_wanted ) swap_bytes = ! swap_bytes;
    return swap_bytes;
}

/*************************************************************************
** Function name: utlIsBigEndian
**//**
**      Function to return the endianness of the system
**
**  \return                    Returns true if it is big endian
**
**************************************************************************
*/
int utlIsBigEndian()
{
    return check_endian(0);
}

/*************************************************************************
** Function name: utlCreateBinSrc
**//**
**       Constructs a hBinSrc object from a hBlob object.
**
**  \param blob                The hBlob object from which to read data
**  \param binsrc              Pointer to return the hBinSrc object
**                             created.
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlCreateBinSrc( hBlob blob, hBinSrc * binsrc)
{
    hBinSrc bs;

    (*binsrc) = NULL;

    /* Was coded with ASSERT(), but this crashed CC optimizer?! */
#ifndef NDEBUG
    if( sizeof(INT4) != 4 || sizeof(INT2) != 2 || sizeof(INT1) != 1 )
    {
        THROW_EXCEPTION("Binary data compilation error: sizeof INT4, INT2, INT1 not correct");
        RETURN_STATUS(STS_INVALID_DATA);
    }
#endif

    bs = (hBinSrc) utlAlloc( sizeof( BinSrc ) );
    if( ! bs )
    {
        RETURN_STATUS(STS_ALLOC_FAILED)
    };

    bs->blob = blob;
    bs->offset = 0;
    bs->seek_offset = 0;
    bs->src_endian = 0;
    bs->arch_endian = check_endian( 0 );
    bs->swap_bytes = bs->arch_endian;

    (*binsrc) = bs;

    return STS_OK;
}


/*************************************************************************
** Function name: utlCreateEmbeddedBinSrc
**//**
**       Constructs a new hBinSrc object from an existing one, adding
**       an offset for all read operations.
**
**  \param binsrc              The source hBinSrc object.
**  \param offset              The offset from which to read the
**                             embedded object
**  \param embsrc              Pointer to return the hBinSrc object
**                             created.
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlCreateEmbeddedBinSrc( hBinSrc binsrc, long offset, hBinSrc * embsrc )
{
    hBinSrc bs;

    (*embsrc) = NULL;

    if( ! binsrc )
    {
        RETURN_STATUS(STS_INVALID_DATA)
    };

    bs = (hBinSrc) utlAlloc( sizeof( BinSrc ) );
    if( ! bs )
    {
        RETURN_STATUS(STS_ALLOC_FAILED)
    };

    bs->blob = binsrc->blob;
    bs->seek_offset = binsrc->seek_offset + offset;
    bs->offset = bs->seek_offset;
    bs->src_endian = binsrc->src_endian;
    bs->arch_endian = binsrc->arch_endian;
    bs->swap_bytes = binsrc->swap_bytes;

    (*embsrc) = bs;

    return STS_OK;
}


/*************************************************************************
** Function name: utlReleaseBinSrc
**//**
**       Releases resources allocated to a hBinSrc object.
**
**  \param binsrc              Pointer to the hBinSrc object
**                             released.
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlReleaseBinSrc( hBinSrc binsrc)
{
    if( binsrc )
    {
        utlFree ( binsrc );
    }
    return STS_OK;
}

/*************************************************************************
** Function name: utlSetOffset
**//**
**       Set the read offset for a hBinSrc object (including offset for
**       embedded objects.
**
**  \param binsrc              Pointer to the hBinSrc object
**                             released.
**  \param offset              Offset to set
**
**  \return                    The return status
**
**************************************************************************
*/

static long utlSetOffset( hBinSrc binsrc, long offset )
{
    if( offset == BINSRC_CONTINUE )
    {
        offset = binsrc->offset;
    }
    else
    {
        offset += binsrc->seek_offset;
    }
    return offset;
}

/*************************************************************************
** Function name: utlBinSrcSetBigEndian
**//**
**       Sets the endianness of the data source.  This is compared with
**       the data architecture to determine whether the routines need to
**       alter the byte order.
**
**  \param binsrc              Pointer to the hBinSrc object
**                             released.
**  \param bigEndian           1 if the data is big endian, 0 otherwise
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlBinSrcSetBigEndian( hBinSrc binsrc, int bigEndian)
{
    binsrc->src_endian = bigEndian ? 1 : 0;
    binsrc->swap_bytes = binsrc->src_endian;
    if( binsrc->arch_endian ) binsrc->swap_bytes = ! binsrc->swap_bytes;
    return STS_OK;
}

/*************************************************************************
** Function name: utlBinSrcLoad1
**//**
**      Loads a specified number of 2 byte values from a blob and swaps
**      their byte order if required.
**
**  \param binsrc              The binary object data source
**  \param offset              The offset from which to start reading
**  \param nval                The number of values to read
**  \param data                The buffer to receive the data
**
**  \return
**
**************************************************************************
*/

int utlBinSrcLoad1( hBinSrc binsrc, long offset, int nval, void *data)
{
    StatusType sts;
    int size = nval;
    offset = utlSetOffset( binsrc, offset );
    sts = utlBlobReadAt( binsrc->blob, offset, size, data );
    if( sts != STS_OK ) return sts;
    binsrc->offset = offset + size;
    return sts;
}

/*************************************************************************
** Function name: utlBinSrcLoad2
**//**
**      Loads a specified number of 2 byte values from a blob and swaps
**      their byte order if required.
**
**  \param binsrc              The binary object data source
**  \param offset              The offset from which to start reading
**  \param nval                The number of values to read
**  \param data                The buffer to receive the data
**
**  \return
**
**************************************************************************
*/

int utlBinSrcLoad2( hBinSrc binsrc, long offset, int nval, void *data)
{
    StatusType sts;
    unsigned char *b;
    int size = 2*nval;
    offset = utlSetOffset( binsrc, offset );
    sts = utlBlobReadAt( binsrc->blob, offset, size, data );
    if( sts != STS_OK ) return sts;
    binsrc->offset = offset + size;
    if( binsrc->swap_bytes )
    {
        b = (unsigned char *) data;
        while(nval--)
        {
            swap2(b);
            b += 2;
        }
    }
    return sts;
}

/*************************************************************************
** Function name: utlBinSrcLoad4
**//**
**      Loads a specified number of 4 byte values from a blob and swaps
**      their byte order if required.
**
**  \param binsrc              The binary object data source
**  \param offset              The offset from which to start reading
**  \param nval                The number of values to read
**  \param data                The buffer to receive the data
**
**  \return
**
**************************************************************************
*/

int utlBinSrcLoad4( hBinSrc binsrc, long offset, int nval, void *data)
{
    StatusType sts;
    unsigned char *b;
    int size = 4*nval;
    offset = utlSetOffset( binsrc, offset );
    sts = utlBlobReadAt( binsrc->blob, offset, size, data );
    if( sts != STS_OK ) return sts;
    binsrc->offset = offset + size;
    if( binsrc->swap_bytes )
    {
        b = (unsigned char *) data;
        while(nval--)
        {
            swap4(b);
            b += 4;
        }
    }
    return sts;
}

/*************************************************************************
** Function name: utlBinSrcLoad8
**//**
**      Loads a specified number of 8 byte values from a blob and swaps
**      their byte order if required.
**
**  \param binsrc              The binary object data source
**  \param offset              The offset from which to start reading
**  \param nval                The number of values to read
**  \param data                The buffer to receive the data
**
**  \return
**
**************************************************************************
*/

int utlBinSrcLoad8( hBinSrc binsrc, long offset, int nval, void *data)
{
    StatusType sts;
    unsigned char *b;
    int size = 8*nval;
    offset = utlSetOffset( binsrc, offset );
    sts = utlBlobReadAt( binsrc->blob, offset, size, data );
    if( sts != STS_OK ) return sts;
    binsrc->offset = offset + size;
    if( binsrc->swap_bytes )
    {
        b = (unsigned char *) data;
        while(nval--)
        {
            swap8(b);
            b += 8;
        }
    }
    return sts;
}

/*************************************************************************
** Function name: utlBinSrcLoadString
**//**
**    Function to load a string from the blob.  The string is stored as a
**    2 byte length followed by the data.  The string should be stored with
**    a trailing null byte included - although this routine will add a null
**    byte for safety.
**
**    This routine use ::utlAlloc to allocate the string that will be returned.
**
**  \param binsrc              The binary source object
**  \param offset              The offset to start reading
**  \param data                Returns a pointer to the created string
**
**  \return                    Returns a pointer to the string bufer
**
**************************************************************************
*/

StatusType utlBinSrcLoadString( hBinSrc binsrc, long offset, char **data )
{
    INT2 len;
    char *s;
    StatusType sts;
    *data = NULL;
    sts = utlBinSrcLoad2( binsrc, offset, 1, (void *) (&len) );
    if( sts != STS_OK ) RETURN_STATUS(sts);
    if( len < 0 || len > BINSRC_MAX_STRING_LEN ) RETURN_STATUS(STS_INVALID_DATA);

    s = (char *) utlAlloc( len+1 );
    if( ! s ) RETURN_STATUS(STS_ALLOC_FAILED);
    if( len > 0 ) sts = utlBinSrcLoad1( binsrc, BINSRC_CONTINUE, len, s );
    if( sts != STS_OK )
    {
        utlFree(s);
        RETURN_STATUS(sts);
    }
    s[len] = 0;
    *data = s;
    return STS_OK;
}

