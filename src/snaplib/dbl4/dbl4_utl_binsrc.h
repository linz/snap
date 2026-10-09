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

#ifndef DBL4_TYPES_H
#include "dbl4_types.h"
#endif

#ifndef DBL4_UTL_BLOB_H
#include "dbl4_utl_blob.h"
#endif

#include <string>

/// The byte order of binary values
enum class Endian { Little, Big };

struct BinSrc
{
    hBlob const blob;           ///< The blob object from which to read data
    long offset;                ///< The offset from which to read the next data
    const long seek_offset;     ///< Offset used for embedded binsrc objects
    Endian src_endian = Endian::Little;  ///< The byte order of the source data
    const Endian arch_endian;   ///< The byte order of the architecture

    /// Creates a source reading from the start of a blob, with little endian data
    explicit BinSrc( hBlob source );  ///< The blob object from which to read data

    /// Creates a source reading from within another one, with the same byte order
    BinSrc( const BinSrc &parent,    ///< The source being embedded in
            long embeddedOffset );   ///< The offset in parent from which to read

    /// True if the byte order of the values read has to be reversed
    bool swap_bytes() const { return src_endian != arch_endian; }
};
typedef BinSrc *hBinSrc;

/* The offset value to specify to continue reading from the last read
   statement */

#define BINSRC_CONTINUE -1

StatusType utlCreateBinSrc( hBlob blob, hBinSrc * binsrc);
StatusType utlCreateEmbeddedBinSrc( hBinSrc binsrc, long offset, hBinSrc *embsrc);
StatusType utlReleaseBinSrc( hBinSrc binsrc);

StatusType utlBinSrcSetEndian( hBinSrc binsrc, Endian endian );

StatusType utlBinSrcLoad1( hBinSrc binsrc, long offset, int nval, void *data );
StatusType utlBinSrcLoad2( hBinSrc binsrc, long offset, int nval, void *data );
StatusType utlBinSrcLoad4( hBinSrc binsrc, long offset, int nval, void *data );
StatusType utlBinSrcLoad8( hBinSrc binsrc, long offset, int nval, void *data );
StatusType utlBinSrcLoadString( hBinSrc binsrc, long offset, std::string &data );

#endif /* DBL4_UTL_BINSRC_H not defined */
