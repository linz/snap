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

#include "dbl4_common.h"

#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "dbl4_utl_binsrc.h"

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
** Function name: architecture_endian
**//**
**          Function to test the byte order of the machine on which it is
**      executed.  It does this crudely by converting a short integer to
**      a character buffer, and then testing the value of the first
**      character.
**
**  \return                    The byte order of the machine
**
**************************************************************************
*/

static Endian architecture_endian()
{
    const unsigned short test = 0x01;
    /* If little endian then the first byte is 1, else it is 0 */
    return *reinterpret_cast<const unsigned char *>( &test ) == 1 ? Endian::Little : Endian::Big;
}

BinSrc::BinSrc( const hBlob source )
    : blob( source ),
      offset( 0 ),
      seek_offset( 0 ),
      arch_endian( architecture_endian() )
{
}

BinSrc::BinSrc( const BinSrc &parent, const long embeddedOffset )
    : blob( parent.blob ),
      offset( parent.seek_offset + embeddedOffset ),
      seek_offset( parent.seek_offset + embeddedOffset ),
      src_endian( parent.src_endian ),
      arch_endian( parent.arch_endian )
{
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
    (*binsrc) = NULL;

    /* Was coded with ASSERT(), but this crashed CC optimizer?! */
    #ifndef NDEBUG
    if( sizeof(INT4) != 4 || sizeof(INT2) != 2 || sizeof(INT1) != 1 )
    {
        THROW_EXCEPTION("Binary data compilation error: sizeof INT4, INT2, INT1 not correct");
        RETURN_STATUS(STS_INVALID_DATA);
    }
    #endif

    (*binsrc) = new BinSrc( blob );

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
    (*embsrc) = NULL;

    if( ! binsrc )
    {
        RETURN_STATUS(STS_INVALID_DATA)
    };

    (*embsrc) = new BinSrc( *binsrc, offset );

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
    delete binsrc;
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
** Function name: utlBinSrcSetEndian
**//**
**       Sets the byte order of the data source.  This is compared with
**       the byte order of the architecture to determine whether the
**       routines need to alter the byte order.
**
**  \param binsrc              Pointer to the hBinSrc object
**                             released.
**  \param endian              The byte order of the data
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlBinSrcSetEndian( hBinSrc binsrc, const Endian endian)
{
    binsrc->src_endian = endian;
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
    if( binsrc->swap_bytes() )
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
    if( binsrc->swap_bytes() )
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
    if( binsrc->swap_bytes() )
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
**    a trailing null byte included.  The string returned ends at the first
**    null byte, as the C string it replaces did.
**
**  \param binsrc              The binary source object
**  \param offset              The offset to start reading
**  \param data                Returns the string, empty if the read fails
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlBinSrcLoadString( hBinSrc binsrc, long offset, std::string &data )
{
    INT2 len;
    data.clear();
    StatusType sts = utlBinSrcLoad2( binsrc, offset, 1, (void *) (&len) );
    if( sts != STS_OK ) RETURN_STATUS(sts);
    if( len < 0 ) RETURN_STATUS(STS_INVALID_DATA);

    std::string text( len, '\0' );
    if( len > 0 ) sts = utlBinSrcLoad1( binsrc, BINSRC_CONTINUE, len, text.data() );
    if( sts != STS_OK ) RETURN_STATUS(sts);
    text.resize( std::min( text.find( '\0' ), text.size() ) );
    data = std::move( text );
    return STS_OK;
}
