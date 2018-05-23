/*************************************************************************
**
**  Filename:    %M%
**
**  Version:     %I%
**
**  What string: %W%
**
** $Id: dbl4_utl_grid.c,v 1.6 2005/04/04 23:57:58 ccrook Exp $
**//**
** \file
**      Functions for managing a grid definition stored in a binary data source.
**      The grid data is formatted as binary data using the conventions of
**      the machine on which it is executed.  (It may be desirable to make
**      this machine independent storage ultimately for CRS).  The data for
**      each row is stored as a set of short integers, with a scale
**      factor defined in the header to be applied.
**      The precise data format is still to be documented.
**
**      The functions interpolate values on the grid.  For efficiency
**      rows of the grid are cached using a least recently used algorithm
**      for eliminating old rows.
**
**      The binary format is portable between SUN and Intel DOS/Windows
**      environments which differ only in endianness
**
*************************************************************************
*/

#include "dbl4_common.h"

#include <stdlib.h>
#include <string.h>

#include "dbl4_utl_grid.h"

#include "dbl4_utl_alloc.h"
#include "dbl4_utl_error.h"

/* Note: code below assumes headers are all the same length */

#define GRID_FILE_HEADER_1 "SNAP geoid binary file\r\n\x1A"
#define GRID_FILE_HEADER_2 "SNAP grid binary v1.0 \r\n\x1A"
#define GRID_FILE_HEADER_3 "CRS grid binary v1.0  \r\n\x1A"
#define GRID_FILE_HEADER_4 "SNAP grid binary v2.0 \r\n\x1A"
#define GRID_FILE_HEADER_5 "CRS grid binary v2.0  \r\n\x1A"

/* Maximum number of rows stored in grid row cache */

#define MAXCACHE 60

/* Marker value used to check validity of grid handle */

#define MAGIC_NUMBER 0xA1387B59

typedef struct file_row_s file_row;
typedef struct cache_row_s cache_row;

typedef struct
{
    unsigned int magic;
    hBinSrc binsrc;
    INT4   indexloc;
    double maxy;
    double miny;
    double maxx;
    double minx;
    double yres;
    double xres;
    double vres;
    short ngrdy;
    short ngrdx;
    short ngrdval;    /**< Number of values at each point */
    short latlon;     /**< True if is a lat/long grid */
    short ncycle;     /**< Increments to get around the globe */
    char global;
    char *desc1;
    char *desc2;
    char *desc3;
    char *crdsys;
    int  rowfmt;      /**< 1 for simple array of shorts, 2 for compressed longs */
    int  rowsize;     /**< Size of a row in bytes */
    int  maxcache;    /**< Maximum number of cache entries */
    int  ncache;      /**< Current number of cache entries */
    INT4 undef;       /**< Value used to represent missing data */
    file_row *rows;   /**< Definition of rows */
    cache_row *cache; /**< The cache */
    cache_row *cache_mru;  /**< Most recently used */
    cache_row *cache_lru;  /**< Least recently used */
    void *loadbuffer;
} grid_def_crs;

struct file_row_s
{
    INT4 fileloc;
    cache_row *cacheloc;
};

struct cache_row_s
{
    INT4 *data;
    file_row *lat;
    cache_row *next;      /* For LRU cache */
    cache_row *prev;
};


/*************************************************************************
** Function name: delete_grid_def
**//**
**         Deletes a grid definition and deallocates the resources it uses
**
**  \param def                     The grid definition to delete
**
**  \return
**
**************************************************************************
*/

static void delete_grid_def( grid_def_crs *def)
{
    def->magic = 0;
    if( def->rows ) utlFree( def->rows );
    if( def->cache )
    {
        int i;
        for( i = 0; i++ < def->ncache;  )
        {
            if( def->cache[i].data ) utlFree( def->cache[i].data );
        }
        utlFree( def->cache );
    }
    def->rows = 0;
    def->cache = 0;
    if( def->desc1 )
    {
        utlFree( def->desc1 );
        def->desc1 = 0;
    }
    if( def->desc2 )
    {
        utlFree( def->desc2 );
        def->desc2 = 0;
    }
    if( def->desc3 )
    {
        utlFree( def->desc3 );
        def->desc3 = 0;
    }
    if( def->crdsys )
    {
        utlFree( def->crdsys );
        def->crdsys = 0;
    }
    if( def->loadbuffer )
    {
        utlFree( def->loadbuffer );
        def->loadbuffer = 0;
    }
    utlFree( def );
}


/*************************************************************************
** Function name: check_header
**//**
**    Function to read the header and check that it is compatible with the
**    one of the defined valid headers, each defining a version of the grid
**    format
**
**  \param binsrc              The binary data source to read from
**  \param indexloc            Returns the location of the grid index
**                             data
**
**  \return                    Returns the grid format version number
**
**************************************************************************
*/

static int check_header( hBinSrc binsrc, INT4 *indexloc)
{
    char buf[80];
    INT4 len;
    int version;
    int big_endian=0;
    version = 0;
    len = strlen( GRID_FILE_HEADER_1 );
    if( utlBinSrcLoad1( binsrc, 0, len, buf ) != STS_OK ) return 0;
    buf[len] = 0;
    TRACE_GRID(("Grid signature: %s",buf));

    if(  memcmp( buf, GRID_FILE_HEADER_1, len ) == 0 )
    {
        version = 1;
        big_endian = 0;
    }
    else if(  memcmp( buf, GRID_FILE_HEADER_2, len ) == 0 )
    {
        version = 2;
        big_endian = 0;
    }
    else if(  memcmp( buf, GRID_FILE_HEADER_3, len ) == 0 )
    {
        version = 2;
        big_endian = 1;
    }
    else if(  memcmp( buf, GRID_FILE_HEADER_4, len ) == 0 )
    {
        version = 3;
        big_endian = 0;
    }
    else if(  memcmp( buf, GRID_FILE_HEADER_5, len ) == 0 )
    {
        version = 3;
        big_endian = 1;
    }
    utlBinSrcSetBigEndian( binsrc, big_endian );

    if( utlBinSrcLoad4( binsrc, BINSRC_CONTINUE, 1, indexloc ) != STS_OK ) return 0;
    TRACE_GRID(("Index location: %ld",(long)(*indexloc)));
    if( ! *indexloc ) version = 0;
    return version;
}




/*************************************************************************
** Function name: create_grid_def
**//**
**   Function to load the data defining a grid (ie extents, number of cells,
**   etc)
**
**  \param defr                    The grid object to load
**  \param binsrc              The binary data source to load the
**                             grid from
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType create_grid_def( grid_def_crs** defr, hBinSrc binsrc)
{
    grid_def_crs *def;
    int version;
    INT4 indexloc;
    INT2 ival;
    int i;
    StatusType sts;
    TRACE_GRID(("Loading grid definition"));
    version = check_header(binsrc, &indexloc);
    if( ! version )
    {
        RETURN_STATUS(STS_INVALID_DATA);
    }
    TRACE_GRID(("Grid format version: %d",(int) version));
    def = (grid_def_crs *) utlAlloc( sizeof( grid_def_crs ) );
    if( ! def ) RETURN_STATUS(STS_ALLOC_FAILED);
    def->indexloc = indexloc;
    def->rows = 0;
    def->desc1 = 0;
    def->desc2 = 0;
    def->desc3 = 0;
    def->crdsys = 0;
    def->cache = 0;
    def->loadbuffer = 0;
    def->rowfmt = version == 3 ? 2 : 1;
    def->undef = 0x7FFFFFFF;
    def->binsrc = binsrc;
    sts = STS_OK;

    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, indexloc, 1, (void *) (&(def->miny)) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, (void *) (&(def->maxy)) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, (void *) (&(def->minx)) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, (void *) (&(def->maxx)) );
    if( sts == STS_OK ) sts = utlBinSrcLoad8( binsrc, BINSRC_CONTINUE, 1, (void *) (&(def->vres)) );

    if( sts == STS_OK )
    {
        sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &ival );
        def->ngrdy = ival;
    }
    if( sts == STS_OK )
    {
        sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &ival );
        def->ngrdx = ival;
    }

    if( version > 1 )
    {
        if( sts == STS_OK )
        {
            sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &ival );
            def->ngrdval = ival;
        }
        if( sts == STS_OK )
        {
            sts = utlBinSrcLoad2( binsrc, BINSRC_CONTINUE, 1, &ival );
            def->latlon = ival;
        }
    }
    else
    {
        def->ngrdval = 1;
        def->latlon = 1;
    }

    /* Do some basic integrity checks on the grid specifications, mainly
       to make sure that the file hasn't got corrupted.  Note that this
       assumes that the grid data fills the space between the file header
       info and the grid specifications (which is not required by the
       format strictly) */

    if( sts == STS_OK && ( def->ngrdx < 2 || def->ngrdy < 2 )) SET_STATUS(sts,STS_INVALID_DATA);


    if( sts == STS_OK )
        sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, &(def->desc1) );
    if( sts == STS_OK )
        sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, &(def->desc2) );
    if( sts == STS_OK )
        sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, &(def->desc3) );
    if( sts == STS_OK )
        sts = utlBinSrcLoadString( binsrc, BINSRC_CONTINUE, &(def->crdsys) );

    if( sts == STS_OK )
    {
        def->rows = (file_row *) utlAlloc( sizeof(file_row) * def->ngrdy );
        if( ! def->rows ) SET_STATUS(sts,STS_ALLOC_FAILED);
    }

    if( sts != STS_OK )
    {
        delete_grid_def(def);
        RETURN_STATUS(sts);
    }

    TRACE_GRID(("Grid loaded: %s",def->desc1));
    TRACE_GRID(("             %s",def->desc2));
    TRACE_GRID(("             %s",def->desc3));
    TRACE_GRID(("             %s",def->crdsys));
    TRACE_GRID(("        Size: = %d x %d",(int)(def->ngrdy),(int)(def->ngrdx)));
    TRACE_GRID(("        Lat: %.8lf - %.8lf",def->miny,def->maxy));
    TRACE_GRID(("        Lon: %.8lf - %.8lf",def->minx,def->maxx));
    TRACE_GRID(("        Res: %.8lf",def->vres));
    TRACE_GRID(("        Dim: %d",(int)(def->ngrdval)));
    TRACE_GRID(("     LatLon: %d",(int)(def->latlon)));
    TRACE_GRID2(("   Offsets:"));


    for( i = 0; i < def->ngrdy; i++ )
    {
        if( sts == STS_OK ) sts = utlBinSrcLoad4( binsrc, BINSRC_CONTINUE, 1,
                                      (void *) (&(def->rows[i].fileloc)) );
        def->rows[i].cacheloc = 0;

        TRACE_GRID2(("            %03d %06ld",i,def->rows[i].fileloc));
    }

    def->rowsize = def->ngrdx * def->ngrdval;
    def->yres = (def->maxy - def->miny)/(def->ngrdy-1);
    def->xres = (def->maxx - def->minx)/(def->ngrdx-1);
    def->ncycle = (short) (360/def->xres+0.5);
    def->global = (def->ncycle == def->ngrdx);

    def->maxcache = MAXCACHE;
    def->cache = (cache_row *) utlAlloc( sizeof(cache_row) * (def->maxcache+1) );
    if( !def->cache )
    {
        delete_grid_def( def );
        RETURN_STATUS(STS_ALLOC_FAILED);
    }
    def->ncache = 0;
    def->cache_mru = 0;
    def->cache_lru = 0;

    def->magic = MAGIC_NUMBER;

    *defr = def;
    return STS_OK;
}


/*************************************************************************
** Function name: load_row1
**//**
**   Function to load a row of the grid in the LINZ1 format. (Packed
**   as a simple array of INT2 values)
**
**  \param def                 The grid object to load
**  \param fileloc             The location of the data in the file
**  \param data                Array into which the row is loaded
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType load_row1( grid_def_crs *def, long fileloc, INT4 *data )
{
    StatusType sts;

    /* Allocate a buffer for reading if not already done.  Make this
           the largest possible size */

    if( ! def->loadbuffer )
    {
        def->loadbuffer = utlAlloc( def->rowsize * 2 );
        if( ! def->loadbuffer ) RETURN_STATUS(STS_ALLOC_FAILED);
    }
    /* Format 1 rows - simply held as an array of shorts */
    sts = utlBinSrcLoad2( def->binsrc, fileloc, def->rowsize, def->loadbuffer );
    TRACE_GRID2(("Loading row using LINZ1 format"));
    if( sts == STS_OK )
    {
        int i;
        INT2 *sdata = (INT2 *) def->loadbuffer;
        for( i = 0; i < def->rowsize; i++ )
        {
            INT4 v = sdata[i];
            if( v == 0x7FFF ) v = def->undef;
            data[i] = v;
            TRACE_GRID3((" Column %d: value %ld",(int) i, (long) v ));
        }
    }
    return sts;
}

/*************************************************************************
** Function name: load_row2_dim
**//**
**   Function to load the data for one dimension of data values for
**   a row of the grid in the LINZ2 format.
**
**  \param def                 The grid object to load
**  \param fileloc             The location of the data in the file
**  \param data                Array into which the row is loaded
**
**  \return                    The return status
**
**************************************************************************
*/

static StatusType load_row2_dim( grid_def_crs *def, long fileloc, INT4 *data )
{
    StatusType sts;
    INT2 fmt;
    INT2 bytes;
    INT2 dif;
    INT2 subset;
    INT2 cont;
    INT2 i, i0, imin, imax;
    INT4 d1, d2;
    INT4 undef;
    INT4 undef2;
    INT4 v = 0;
    int ndim;
    INT1 *pc = 0;
    INT2 *ps = 0;
    INT4 *pl = 0;
    hBinSrc binsrc;


    binsrc = def->binsrc;
    ndim = def->ngrdval;

    imin = 0;
    imax = -1;

    cont = 1;
    undef = def->undef;

    while(cont)
    {
        TRACE_GRID2(("Loading format2 row dimension from %ld",(long) fileloc ));
        sts = utlBinSrcLoad2( binsrc, fileloc, 1, &fmt );
        if( sts != STS_OK ) RETURN_STATUS(sts);
        fileloc = BINSRC_CONTINUE;

        /* subset=1 means that imin and imax are specified, this isn't the
           whole row (or rest of the row) */

        subset = fmt & 1;

        /* cont!=0 means that this section is followed by another */

        cont = fmt & 2;

        /* Dif is 0, 1, or 2 to specify values defined as actual, first
           differences, or second differences */

        dif = fmt>>2 & 3;
        if( dif == 3 ) RETURN_STATUS(STS_INVALID_DATA);

        /* Bytes is the number of bytes to represent the each value/difference
          in the row, 1, 2, or 4 */

        bytes = fmt >> 4;
        if( bytes != 1 && bytes != 2 && bytes != 4 ) RETURN_STATUS(STS_INVALID_DATA);

        /* Doesn't make sense if this is continued (followed by another section),
           but not flagged as subset, so we use this as flag that all remaining
           values are undefined */

        if( cont && ! subset ) break;

        /* Get the first and last value for this section */

        i0 = imax+1;
        imin = i0;
        imax = def->ngrdx-1;

        if ( subset == 1 )
        {
            sts = utlBinSrcLoad2( binsrc, fileloc, 1, &imin );
            if( sts != STS_OK ) RETURN_STATUS(sts);
            sts = utlBinSrcLoad2( binsrc, fileloc, 1, &imax );
            if( sts != STS_OK ) RETURN_STATUS(sts);
            if( imin < i0  ) RETURN_STATUS(STS_INVALID_DATA);
            if( imax < imin || imax >= def->ngrdx ) RETURN_STATUS(STS_INVALID_DATA );
        }

        TRACE_GRID2(("Format: subset %d cont %d dif %d bytes %d imin %d imax %d",
                     (int) subset, (int) cont, (int) dif, (int) bytes, (int) imin, (int) imax ));

        undef2 = (1L << (bytes*8-1))-1;

        /* Fill from the end of the last section to the start of this with
           undefined values */

        if( imin > i0 )
        {
            for( i = i0; i < imin; i++, data += ndim ) (*data) = undef;
        }

        /* If first differences read first value, if second differences read
           first value and first first difference value - read slowly! */

        d1 = d2 = 0;
        if( dif > 0 )
        {
            sts = utlBinSrcLoad4(binsrc,fileloc,1,&d1);
            if( sts != STS_OK ) RETURN_STATUS(sts);
        }
        if( dif > 1 )
        {
            sts = utlBinSrcLoad4(binsrc,fileloc,1,&d2);
            if( sts != STS_OK ) RETURN_STATUS(sts);
        }

        /* Allocate a buffer for reading if not already done.  Make this
           the largest possible size */

        if( ! def->loadbuffer )
        {
            def->loadbuffer = utlAlloc( def->ngrdx * 4 );
            if( ! def->loadbuffer ) RETURN_STATUS(STS_ALLOC_FAILED);
        }

        /* Read the values from the file */

        if( bytes == 1 )
        {
            sts = utlBinSrcLoad1(binsrc,fileloc,imax+1-imin,def->loadbuffer);
            pc = (INT1 *) def->loadbuffer;
        }
        else if ( bytes == 2 )
        {
            sts = utlBinSrcLoad2(binsrc,fileloc,imax+1-imin,def->loadbuffer);
            ps = (INT2 *) def->loadbuffer;
        }
        else
        {
            sts = utlBinSrcLoad4(binsrc,fileloc,imax+1-imin,def->loadbuffer);
            pl = (INT4 *) def->loadbuffer;
        }
        if( sts != STS_OK ) RETURN_STATUS(sts);

        /* Store the valyes in the data array, accounting for differences if
           appropriate */

        if( dif )
        {
            (*data) = d1;
            imin++;
            data += ndim;
            switch( bytes )
            {
            case 1:
                v = *pc;
                pc++;
                break;
            case 2:
                v = *ps;
                ps++;
                break;
            case 4:
                v = *pl;
                pl++;
                break;
            }
        }

        TRACE_GRID3(("Starting row: d1 %ld v %ld",(long) d1, (long) v));
        for( i = imin; i <=imax; i++, data += ndim )
        {
            switch( bytes )
            {
            case 1:
                v = *pc;
                pc++;
                break;
            case 2:
                v = *ps;
                ps++;
                break;
            case 4:
                v = *pl;
                pl++;
                break;
            }
            TRACE_GRID3(("Reading at %d: v %ld",(int) i, (long) v));
            if( v == undef2 )
            {
                v = undef;
            }
            else
            {
                switch(dif)
                {
                case 0:
                    break;
                case 1:
                    d1 += v;
                    v = d1;
                    break;
                case 2:
                    d2 += v;
                    d1 += d2;
                    v = d1;
                    break;
                }
            }
            (*data) = v;
            TRACE_GRID3(("Value set to %ld", (long) v));
        }
    }

    /* Set any values not yet set to undefined */

    for( i = imax+1; i<def->ngrdx; i++, data += ndim )
    {
        (*data) = undef;
    }

    return STS_OK;
}

/*************************************************************************
** Function name: load_row2
**//**
**   Function to load the data for for a row of the grid in the LINZ2
**   format.  Simply calls ::load_row2_dim for each dimension of data.
**
**  \param def                 The grid object to load
**  \param fileloc             The location of the data in the file
**  \param data                Array into which the row is loaded
**
**  \return                    The return status
**
**************************************************************************
*/
static StatusType load_row2( grid_def_crs *def, long fileloc, INT4 *data )
{
    int ndim = def->ngrdval;
    int idim;
    StatusType sts;
    for( idim = 0; idim < ndim; idim++ )
    {
        sts = load_row2_dim( def, fileloc, data+idim);
        if( sts != STS_OK ) RETURN_STATUS(sts);
        fileloc = BINSRC_CONTINUE;
    }
    return STS_OK;
}

/*************************************************************************
** Function name: get_row
**//**
**   Function to return a row of data from the grid.  This manages
**   the LRU cache
**
**  \param def                     The grid file definition
**  \param lat                 The number of the row to read
**  \param row                 The buffer to receive the row
**
**  \return                    Returns the success/failure status
**
**************************************************************************
*/

static StatusType get_row( grid_def_crs *def, short lat, INT4** row)
{
    cache_row *cr;
    file_row *fr;
    StatusType sts=STS_OK;
    INT4 fileloc;
    fr = &def->rows[lat];
    cr = fr->cacheloc;
    TRACE_GRID2(("Retrieving row %d",(int)lat));

    if( !cr )
    {
        TRACE_GRID2(("Row not in cache - loading from source"));
        if( def->ncache < def->maxcache )
        {
            int loc;
            loc = ++(def->ncache);
            cr = &def->cache[loc];
            cr->data = (INT4 *) utlAlloc( def->rowsize * sizeof(INT4) );
            if( ! cr->data ) RETURN_STATUS(STS_ALLOC_FAILED);
            cr->next = def->cache_mru;
            cr->prev = 0;
            if( cr->next )
            {
                cr->next->prev = cr;
            }
            def->cache_mru = cr;
            if( !def->cache_lru ) def->cache_lru = cr;
        }
        else
        {
            cr = def->cache_lru;
            cr->lat->cacheloc = 0;
        }
        cr->lat = fr;
        fr->cacheloc = cr;
        fileloc = def->rows[lat].fileloc;

        if( def->rowfmt == 1 )
        {
            sts = load_row1( def, fileloc, cr->data );
        }
        else
        {
            sts = load_row2( def, fileloc, cr->data );
        }

    }
    if( def->cache_mru != cr )
    {
        if( cr->next )
        {
            cr->next->prev = cr->prev;
        }
        else
        {
            def->cache_lru = cr->prev;
        }
        cr->prev->next = cr->next;
        def->cache_mru->prev = cr;
        cr->next = def->cache_mru;
        cr->prev = 0;
        def->cache_mru = cr;
    }
    *row = cr->data;
    return sts;
}



/*************************************************************************
** Function name: calc_cubic_factors
**//**
**        Function calculates the factors required to perform a cubic
**    interpolation from four equally spaced points (at values -3, -1
**    1, 3)
**
**  \param x                   The evaluation point
**  \param f                   The interpolation factors
**
**  \return
**
**************************************************************************
*/

static void calc_cubic_factors( double x, double f[] )
{
    double x0, x1, x2, x3;
    x = 2*x-1;
    x0 = x+3;
    x1 = x+1;
    x2 = x-1;
    x3 = x-3;
    f[0] = -(x1*x2*x3/48.0);
    f[1] = x0*x2*x3/16.0;
    f[2] = -(x0*x1*x3/16.0);
    f[3] = x0*x1*x2/48.0;
}



/*************************************************************************
** Function name: calc_grid_cubic
**//**
**   Do a bicubic interpolation on the grid
**
**  \param def                     The grid definition
**  \param x                   The x coordinate of the evaluation point
**  \param y                   The y coordinate of the evaluation point
**  \param value               The returned grid value
**
**  \return
**
**************************************************************************
*/

static StatusType calc_grid_cubic( grid_def_crs *def, double x, double y,
                                   double *value)
{
    short ny;
    short nx, xrow[4];
    double yfactor[4], xfactor[4];
    int i, j, v;

    for( v = 0; v < def->ngrdval; v++ )
    {
        value[v] = 0;
    }
    y = (y-def->miny)/def->yres;
    ny = (short) y;
    if( ny < 1 || ny+2 >= def->ngrdy ) RETURN_STATUS(STS_INVALID_DATA);
    x = (x-def->minx);
    if( def->latlon )
    {
        while( x > 360.0 ) x -= 360.0;
        while( x < 0.0 ) x += 360.0;
    }
    x = x/def->xres;
    nx = (short) x;
    if( !def->global && (nx < 1 || nx+2 >= def->ngrdx )) RETURN_STATUS(STS_INVALID_DATA);
    calc_cubic_factors( y-ny, yfactor );
    calc_cubic_factors( x-nx, xfactor );
    ny--;
    if( nx ) nx--;
    else nx = def->ngrdx-1;
    for( i = 0; i < 4; i++ )
    {
        xrow[i] = nx*def->ngrdval;
        nx++;
        if( nx == def->ngrdx ) nx = 0;
    }
    for( i = 0; i < 4; i++, ny++ )
    {
        INT4 *row;
        StatusType sts;
        if( yfactor[i] < 1.0e-8 && yfactor[i] >= -1.0e-8 ) continue;
        sts = get_row( def, ny, &row );
        if( sts != STS_OK ) return sts;
        for( v = 0; v < def->ngrdval; v++ )
        {
            double sum = 0.0;
            for( j = 0; j < 4; j++ )
            {
                long rv;
                if( xfactor[j] < 1.0e-8 && xfactor[j] >= -1.0e-8 ) continue;
                rv = row[xrow[j]+v];
                if( rv == def->undef ) RETURN_STATUS(STS_MISSING_DATA);
                sum += rv*xfactor[j];
            }
            value[v] += sum * yfactor[i];
        }
    }
    for( v = 0; v < def->ngrdval; v++ )
    {
        value[v] *= def->vres;
    }

    return STS_OK;
}

/*************************************************************************
** Function name: calc_linear_factors
**//**
**        Function calculates the factors required to perform a linear
**    interpolation from two equally spaced points (at values -1, 1)
**
**  \param x                   The evaluation point
**  \param f                   The interpolation factors
**
**  \return
**
**************************************************************************
*/

static void calc_linear_factors( double x, double f[] )
{
    double x0, x1;
    x = 2*x-1;
    x0 = x+1;
    x1 = x-1;
    f[0] = -x1/2;
    f[1] = x0/2;
}



/*************************************************************************
** Function name: calc_grid_linear
**//**
**
**   Apply bilinear interpolation on the grid
**
**  \param def                     The grid definition
**  \param x                   The x coordinate of the evaluation point
**  \param y                   The y coordinate of the evaluation point
**  \param value               The returned grid value
**
**  \return                    The success/failure status
**
**************************************************************************
*/

static StatusType calc_grid_linear( grid_def_crs *def, double x, double y,
                                    double *value)
{
    short ny;
    short nx, xrow[2];
    double yfactor[2], xfactor[2];
    int i, j, v;

    for( v = 0; v < def->ngrdval; v++ )
    {
        value[v] = 0;
    }
    if( y < def->miny || y > def->maxy ) RETURN_STATUS(STS_CRD_RANGE_ERR);
    y = (y-def->miny)/def->yres;
    ny = (short) y;
    if( ny < 0 ) ny=0;
    if( ny >= def->ngrdy-1 ) ny=def->ngrdy-2;
    x = (x-def->minx);
    if( def->latlon )
    {
        while( x > 360.0 ) x -= 360.0;
        while( x < 0.0 ) x += 360.0;
    }
    if( ! def->global && x > (def->maxx-def->minx)) RETURN_STATUS(STS_CRD_RANGE_ERR);
    x = x/def->xres;
    nx = (short) x;
    if( nx < 0 ) nx=0;
    if( nx >= def->ngrdx-1 ) nx=def->ngrdx-2;
    calc_linear_factors( y-ny, yfactor );
    calc_linear_factors( x-nx, xfactor );

    for( i = 0; i < 2; i++ )
    {
        xrow[i] = nx*def->ngrdval;
        nx++;
        if( nx == def->ngrdx ) nx = 0;
    }
    for( i = 0; i < 2; i++, ny++ )
    {
        INT4 *row;
        StatusType sts;
        if( yfactor[i] < 1.0e-8 && yfactor[i] >= -1.0e-8 ) continue;
        sts = get_row( def, ny, &row );
        if( sts != STS_OK ) return sts;
        for( v = 0; v < def->ngrdval; v++ )
        {
            double sum = 0.0;
            for( j = 0; j < 2; j++ )
            {
                long rv;
                if( xfactor[j] < 1.0e-8 && xfactor[j] >= -1.0e-8 ) continue;
                rv = row[xrow[j]+v];
                if( rv == def->undef ) RETURN_STATUS(STS_MISSING_DATA);
                sum += rv*xfactor[j];
            }
            value[v] += sum * yfactor[i];
        }
    }
    for( v = 0; v < def->ngrdval; v++ )
    {
        value[v] *= def->vres;
    }
    return STS_OK;
}



/*************************************************************************
** Function name: grid_def_from_handle
**//**
**        Casts a void pointer to a grid_def_crs *, and checks that the "magic
**    number" (ie signature) is correct
**
**  \param handle              pointer to cast
**
**  \return                    Returns the grid_def_crs or null
**
**************************************************************************
*/

static grid_def_crs * grid_def_from_handle( void *handle)
{
    grid_def_crs *def;
    if( ! handle ) return 0;
    def = (grid_def_crs *) handle;
    if( def->magic != MAGIC_NUMBER ) return 0;
    return def;
}

/*************************************************************************
** Function name: utlCreateGrid
**//**
**       Function to create a grid handle.  This function uses a hBinSrc to
**       identify a data source.  It does not take over management of the
**       hBlob.  It is up the the user to ensure that the hBlob remains
**       valid for the duration of the grid handle.
**
**  \param binsrc              Binary data source for grid data
**  \param grid                Grid handle returned (0 if fails)
**
**  \return                    Returned status
**
**************************************************************************
*/

StatusType utlCreateGrid( hBinSrc binsrc, hGrid *grid)
{
    grid_def_crs *def;
    StatusType sts;
    *grid = 0;
    sts = create_grid_def( &def, binsrc );
    if( sts == STS_OK ) *grid =  (void *) def;
    return sts;
}


/*************************************************************************
** Function name: utlReleaseGrid
**//**
**    Function releases a grid.
**
**  \param grid                The grid to release
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlReleaseGrid( hGrid grid)
{
    grid_def_crs *def;
    def = grid_def_from_handle( grid );
    if( ! def ) RETURN_STATUS(STS_INVALID_HANDLE);
    delete_grid_def(def);
    return STS_OK;
}


/*************************************************************************
** Function name: utlGridVectorDimension
**//**
**       Return the number of elements defined at each grid point (ie
**       dimension of the vector field that the grid defines).
**
**  \param grid                Grid handle
**  \param dimension           Returns the dimension
**
**  \return                    Return status
**
**************************************************************************
*/

StatusType utlGridVectorDimension( hGrid grid, int *dimension)
{
    grid_def_crs *def;
    def = grid_def_from_handle( grid );
    if( ! def ) RETURN_STATUS(STS_INVALID_HANDLE);
    *dimension = def->ngrdval;
    return STS_OK;
}


/*************************************************************************
** Function name: utlGridCoordSysDef
**//**
**       Returns a coordinate system code stored in the grid.  This is not
**       particularly relevent to CRS as it is currently defined.
**
**  \param grid                The grid handle
**  \param crdsys              Returns a pointer to the string
**
**  \return                    Return status
**
**************************************************************************
*/

StatusType utlGridCoordSysDef( hGrid grid, char** crdsys)
{
    grid_def_crs *def;
    def = grid_def_from_handle( grid );
    if( ! def ) RETURN_STATUS(STS_INVALID_HANDLE);
    *crdsys = def->crdsys;
    return STS_OK;
}


/*************************************************************************
** Function name: utlGridTitle
**//**
**       Returns one of up to three lines of text defined with the grid.
**       Generally used for descriptive information about the grid.
**
**  \param grid                The grid handle
**  \param nTitle              The number of the text required (1-3)
**  \param title               Returns a pointer to the text
**
**  \return                    Return status
**
**************************************************************************
*/

StatusType utlGridTitle( hGrid grid, int nTitle, char** title)
{
    grid_def_crs *def;
    def = grid_def_from_handle( grid );
    if( ! def ) RETURN_STATUS(STS_INVALID_HANDLE);
    *title = 0;
    switch( nTitle )
    {
    case 1:
        *title = def->desc1;
        break;
    case 2:
        *title = def->desc2;
        break;
    case 3:
        *title = def->desc3;
        break;
    default:
        RETURN_STATUS(STS_INVALID_DATA);
    }
    return STS_OK;
}


/*************************************************************************
** Function name: utlCalcGridLinear
**//**
**       Calculates the value at a location within the grid using bilinear
**       interpolation.
**
**  \param grid                The grid handle
**  \param x                   The x coordinate at which to calculate
**  \param y                   The y coordinate at which to calculate
**  \param value               The array of returned values (the
**                             number of values is as specified in
**                             utlGridVectorDimension.
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlCalcGridLinear( hGrid grid, double x, double y, double *value)
{
    grid_def_crs *def;
    StatusType sts;
    def = grid_def_from_handle( grid );
    if( ! def ) RETURN_STATUS(STS_INVALID_HANDLE);

    sts = calc_grid_linear( def, x, y, value );

    TRACE_GRID(("utlCalcGridLinear: %10.5lf %10.5lf value %12.8lf",x,y,*value));
    if( def->ngrdval > 1)
    {
        TRACE_GRID(("utlCalcGridLinear: %27s %12.8f","",value[1]));
    }
    return sts;
}


/*************************************************************************
** Function name: utlCalcGridCubic
**//**
**       Calculates the value at a location within the grid using bicubic
**       interpolation.
**
**  \param grid                The grid handle
**  \param x                   The x coordinate at which to calculate
**  \param y                   The y coordinate at which to calculate
**  \param value               The array of returned values (the
**                             number of values is as specified in
**                             utlGridVectorDimension.
**
**  \return                    The return status
**
**************************************************************************
*/

StatusType utlCalcGridCubic( hGrid grid, double x, double y, double *value)
{
    grid_def_crs *def;
    StatusType sts;

    def = grid_def_from_handle( grid );
    if( ! def ) RETURN_STATUS(STS_INVALID_HANDLE);

    sts = calc_grid_cubic( def, x, y, value );

    TRACE_GRID(("utlCalcGridCubic: %10.5lf %10.5lf value %12.8lf",x,y,*value));
    if( def->ngrdval > 1)
    {
        TRACE_GRID(("utlCalcGridCubic: %27s %12.8f","",value[1]));
    }
    return sts;
}

