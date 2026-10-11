/*
   $Log: griddata.c,v $
   Revision 1.4  2004/04/22 02:34:33  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.3  1999/05/18 14:36:01  ccrook
   *** empty log message ***

   Revision 1.2  1998/05/21 04:01:12  ccrook
   Minor bug fix

   Revision 1.1  1998/05/14 09:06:38  CHRIS
   Initial revision


*/

#include <stdio.h>
#include <stdlib.h>
#include <new>
#include <optional>
#include <string>
#include "util/fileutil.h"
#include "string.h"
#include "util/errdef.h"
#include "geoid/griddata.h"
#include "util/pi.h"

// Reads exactly one value of type T from bin. Returns true on success -
// callers decide what a failed read means for bin's lifetime (see LOAD
// below vs. load_row2's own direct calls).
template <typename T>
static bool load_field( FILE *bin, T &value )
{
    return fread( &value, sizeof(value), 1, bin ) == 1;
}

// Only for use while bin is still local to create_grid_def, before any
// grid_def exists to own it - a failed read here is the only place bin
// will ever get closed. load_row2 reads fields from an already-constructed
// grid_def's own bin instead, where a failed read must leave the object
// (and its still-open bin) alone; it calls load_field directly rather than
// using this macro.
#define LOAD(x) if( ! load_field( bin, x ) ) \
                  { fclose(bin); return INVALID_DATA; }

grid_def::~grid_def()
{
    if( bin ) fclose( bin );
    delete[] rows;
    for( int i = 0; i++ < ncache;  )
    {
        delete[] cache[i].data;
    }
    delete[] cache;
    delete[] loadbuffer;
}

// Reads a length-prefixed string from the grid file: a short byte count
// followed by that many raw bytes (consumed via c_str() by every caller, so
// an embedded null truncates output the same way the original malloc'd
// char* did). Absent (no bytes read, or zero length) is a real "not
// present" distinct from an empty string, hence optional rather than
// collapsing to "".
static std::optional<std::string> load_string( FILE *bin )
{
    short len;
    if( !fread( &len, sizeof(len), 1, bin ) ) return std::nullopt;
    if( !len ) return std::nullopt;
    std::string s( len, '\0' );
    if( !fread( s.data(), len, 1, bin ) ) return std::nullopt;
    // The text is stored as a C string, so drop the terminator and anything after it
    s.resize( std::char_traits<char>::length( s.c_str() ) );
    return s;
}

// Allocates the row cache. Value-initialization zeroes every field of
// every entry (not just data, which the destructor and get_row() use to
// tell an unused slot from a loaded one) - safe, since get_row() always
// sets lat/next/prev itself before a fresh slot's data is ever read.
static cache_row *allocate_cache( int maxcache )
{
    return new cache_row[maxcache+1]();
}

grid_def::grid_def( FILE *bin, long indexloc, double miny, double maxy, double minx, double maxx,
                     double vres, short ngrdy, short ngrdx, short ngrdval, short latlon, int rowfmt,
                     std::optional<std::string> desc1, std::optional<std::string> desc2,
                     std::optional<std::string> desc3, std::optional<std::string> crdsys,
                     file_row *rows ) :
    bin(bin),
    indexloc(indexloc),
    maxy(maxy),
    miny(miny),
    maxx(maxx),
    minx(minx),
    yres((maxy-miny)/(ngrdy-1)),
    xres((maxx-minx)/(ngrdx-1)),
    vres(vres),
    ngrdy(ngrdy),
    ngrdx(ngrdx),
    ngrdval(ngrdval),
    latlon(latlon),
    ncycle((short) (360/xres+0.5)),
    global(ncycle == ngrdx),
    desc1(std::move(desc1)),
    desc2(std::move(desc2)),
    desc3(std::move(desc3)),
    crdsys(std::move(crdsys)),
    rowfmt(rowfmt),
    rowsize(ngrdx*ngrdval),
    rows(rows),
    cache(allocate_cache(MAXCACHE))
{}


static int check_header( FILE *bin, long *indexloc )
{
    char buf[80];
    short len;
    int version;
    int iloc;
    version = 0;
    len = strlen( GRID_FILE_HEADER_1 );
    if( ! fread( buf, len, 1, bin ) ) return 0;
    if( ! fread(&iloc, sizeof(iloc), 1, bin ) ) return 0;
    *indexloc = iloc;

    if(  memcmp( buf, GRID_FILE_HEADER_1, len ) == 0 )
    {
        version = 1;
    }
    else if(  memcmp( buf, GRID_FILE_HEADER_2, len ) == 0 )
    {
        version = 2;
    }
    else if(  memcmp( buf, GRID_FILE_HEADER_4, len ) == 0 )
    {
        version = 3;
    }
    if( !indexloc ) version = 0;
    return version;
}

// Parses and validates the whole grid file header + row index into local
// variables, only constructing a grid_def once every value is already
// known good - see grid_def's own constructor, which never fails, since a
// load failure means it is never called at all.
static int create_grid_def( grid_def **defr, const std::string &filename, short dimension )
{
    FILE *bin = fopen( filename.c_str(), "rb" );
    if( !bin ) return FILE_OPEN_ERROR;
    long indexloc;
    int version = check_header( bin, &indexloc );
    if( ! version ) { fclose(bin); return INVALID_DATA; }
    fseek( bin, indexloc, SEEK_SET );
    int rowfmt = version == 3 ? 2 : 1;

    double miny, maxy, minx, maxx, vres;
    short ngrdy, ngrdx, ngrdval, latlon;

    LOAD( miny );
    LOAD( maxy );
    LOAD( minx );
    LOAD( maxx );
    LOAD( vres );
    LOAD( ngrdy );
    LOAD( ngrdx );
    if( version > 1 )
    {
        LOAD( ngrdval );
        LOAD( latlon );
    }
    else
    {
        latlon = 1;
        ngrdval = 1;
    }
    if( ngrdval != dimension ) { fclose(bin); return INCONSISTENT_DATA; }
    if( ngrdy < 4 || ngrdx < 4 ) { fclose(bin); return INVALID_DATA; }
    std::optional<std::string> desc1 = load_string( bin );
    std::optional<std::string> desc2 = load_string( bin );
    std::optional<std::string> desc3 = load_string( bin );
    std::optional<std::string> crdsys = load_string( bin );
#ifdef DEBUG_GRID
    printf("Lat %.4lf - %.4lf\n",miny,maxy);
    printf("Lon %.4lf - %.4lf\n",minx,maxx);
    printf("vres %.4lf\n",vres);
    printf("ngrdy = %d  ngrdx = %d\n",ngrdy,ngrdx);
#endif
    file_row *rows = new file_row[ngrdy];
    for( short i = 0; i < ngrdy; i++ )
    {
        int loc;
        if( fread( &loc, sizeof(loc), 1, bin ) != 1 )
        {
            delete[] rows;
            fclose(bin);
            return INVALID_DATA;
        }
        rows[i].fileloc = loc;
        rows[i].cacheloc = NULL;
#ifdef DEBUG_GRID
        printf("%03d %06ld\n",i,rows[i].fileloc);
#endif
    }

    *defr = new grid_def( bin, indexloc, miny, maxy, minx, maxx, vres, ngrdy, ngrdx, ngrdval,
                           latlon, rowfmt, std::move(desc1), std::move(desc2), std::move(desc3),
                           std::move(crdsys), rows );
    return OK;
}


static int load_row1( grid_def *def, long *data )
{
    int i;
    short *sdata;

    /* Format 1 rows - simply held as an array of shorts */

    if ( fread( data, sizeof(short), def->rowsize, def->bin ) != (size_t) def->rowsize ) return INVALID_DATA;
    sdata = (short *) data;
    for( i = def->rowsize; i-- > 0; )
    {
        long v = sdata[i];
        if( v == 0x7FFF ) v = def->undef;
        data[i] = v;
    }

    return OK;
}


/*************************************************************************
** Function name:
**      load_row2
**
**************************************************************************
*/

static short load_row2_dim( grid_def *def, long *data )
{
    short fmt;
    short bytes;
    short dif;
    short subset;
    short cont;
    short i, i0, imin, imax;
    int d1, d2;
    int undef;
    int undef2;
    int v = 0;
    int ndim;
    signed char *pc = 0;
    short *ps = 0;
    long *pl = 0;

    ndim = def->ngrdval;

    imin = 0;
    imax = -1;

    cont = 1;
    undef = def->undef;

    while(cont)
    {
        if( ! load_field( def->bin, fmt ) ) return INVALID_DATA;

        /* subset=1 means that imin and imax are specified, this isn't the
           whole row (or rest of the row) */

        subset = fmt & 1;

        /* cont!=0 means that this section is followed by another */

        cont = fmt & 2;

        /* Dif is 0, 1, or 2 to specify values defined as actual, first
           differences, or second differences */

        dif = fmt>>2 & 3;
        if( dif == 3 ) return INVALID_DATA;

        /* Bytes is the number of bytes to represent the each value/difference
          in the row, 1, 2, or 4 */

        bytes = fmt >> 4;
        if( bytes != 1 && bytes != 2 && bytes != 4 ) return INVALID_DATA;

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
            if( ! load_field( def->bin, imin ) ) return INVALID_DATA;
            if( ! load_field( def->bin, imax ) ) return INVALID_DATA;
            if( imin < i0  ) return INVALID_DATA;
            if( imax < imin || imax >= def->ngrdx ) return INVALID_DATA;
        }

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
            if( ! load_field( def->bin, d1 ) ) return INVALID_DATA;
        }
        if( dif > 1 )
        {
            if( ! load_field( def->bin, d2 ) ) return INVALID_DATA;
        }

        /* Allocate a buffer for reading if not already don.  Make this
           the largest possible size */

        if( ! def->loadbuffer )
        {
            try
            {
                def->loadbuffer = new unsigned char[def->ngrdx * 4];
            }
            catch( const std::bad_alloc & )
            {
                return MEM_ALLOC_ERROR;
            }
        }

        /* Read the values from the file */

        i = imax+1-imin;
        if( fread(def->loadbuffer,bytes,i,def->bin) != (size_t) i) return INVALID_DATA;
        if( bytes == 1 )
        {
            pc = (signed char *) def->loadbuffer;
        }
        else if ( bytes == 2 )
        {
            ps = (short *) def->loadbuffer;
        }
        else
        {
            pl = (long *) def->loadbuffer;
        }

        /* Store the valyes in the data array, accounting for differences if
           appropriate */

        if( dif )
        {
            (*data) = d1;
            imin++;
            data += ndim;
            switch( bytes )
            {
            case 1: v = *pc; pc++; break;
            case 2: v = *ps; ps++; break;
            case 4: v = *pl; pl++; break;
            }
        }

        for( i = imin; i <=imax; i++, data += ndim )
        {
            switch( bytes )
            {
            case 1: v = *pc; pc++; break;
            case 2: v = *ps; ps++; break;
            case 4: v = *pl; pl++; break;
            }
            if( v == undef2 )
            {
                v = undef;
            }
            else
            {
                switch(dif)
                {
                case 0: break;
                case 1: d1 += v; v = d1; break;
                case 2: d2 += v; d1 += d2; v = d1; break;
                }
            }
            (*data) = v;
        }
    }

    /* Set any values not yet set to undefined */

    for( i = imax+1; i<def->ngrdx; i++, data += ndim ) { (*data) = undef; }

    return OK;
}

static short load_row2( grid_def *def, long *data )
{
    int ndim = def->ngrdval;
    int idim;
    short sts;
    for( idim = 0; idim < ndim; idim++ )
    {
        sts = load_row2_dim( def, data+idim);
        if( sts != OK ) return sts;
    }
    return OK;
}


static long *get_row( grid_def *def, short lat )
{
    cache_row *cr;
    file_row *fr;
    int sts;

    fr = &def->rows[lat];
    cr = fr->cacheloc;
    if( !cr )
    {
        if( def->ncache < def->maxcache )
        {
            short loc;
            loc = ++(def->ncache);
            cr = &def->cache[loc];
            cr->data = new long[def->rowsize];
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
        fseek( def->bin, def->rows[lat].fileloc, SEEK_SET );
        if( def->rowfmt == 1 )
        {
            sts = load_row1( def, cr->data );
            if( sts != OK ) return 0;
        }
        else
        {
            sts = load_row2( def, cr->data );
            if( sts != OK ) return 0;
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
        cr->prev = NULL;
        def->cache_mru = cr;
    }
    return cr->data;
}

static int calc_grid_cell_xy( grid_def*def, double x, double y, double *cx, double *cy, short *nx, short *ny )
{
    if ( y < def->miny || y > def->maxy ) return INVALID_DATA;
    *cy = (y-def->miny)/def->yres;
    *ny = (short) *cy;
    x = (x-def->minx);
    if( def->latlon )
    {
        int maxfix=5;
        while( x > 360.0 && maxfix-- > 0 ) x -= 360.0;
        while( x < 0.0 && maxfix-- > 0 ) x += 360.0;
    }
    if( x < 0 || x > (def->maxx-def->minx)) return INVALID_DATA;
    *cx = x/def->xres;
    *nx = (short) *cx;
    return OK;
}

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

static int calc_grid_cubic( grid_def *def, double x, double y, double *value )
{
    short ny;
    short nx, xrow[4];
    double yfactor[4], xfactor[4];
    int i, j, v, sts;

    for( v = 0; v < def->ngrdval; v++ ) { value[v] = 0; }

    sts=calc_grid_cell_xy( def, x, y, &x, &y, &nx, &ny );
    if( sts != OK ) return sts;

    if( ny < 1 || ny+2 >= def->ngrdy ) return INVALID_DATA;
    if( !def->global && (nx < 1 || nx+2 >= def->ngrdx )) return INVALID_DATA;

    calc_cubic_factors( y-ny, yfactor );
    calc_cubic_factors( x-nx, xfactor );
    ny--;
    if( nx ) nx--; else nx = def->ngrdx-1;
    for( i = 0; i < 4; i++ )
    {
        xrow[i] = nx*def->ngrdval;
        nx++;
        if( nx == def->ngrdx ) nx = 0;
    }
    for( i = 0; i < 4; i++, ny++ )
    {
        long *row;
        if( yfactor[i] < 1.0e-8 && yfactor[i] >= -1.0e-8 ) continue;
        row = get_row( def, ny );
        if( !row ) return INVALID_DATA;
        for( v = 0; v < def->ngrdval; v++ )
        {
            double sum = 0.0;
            for( j = 0; j < 4; j++ )
            {
                long rv;
                if( xfactor[j] < 1.0e-8 && xfactor[j] >= -1.0e-8 ) continue;
                rv = row[xrow[j]+v];
                if( rv == def->undef ) return MISSING_DATA;
                sum += rv*xfactor[j];
            }
            value[v] += sum * yfactor[i];
        }
    }
    for( v = 0; v < def->ngrdval; v++ )
    {
        value[v] *= def->vres;
    }
    return OK;
}


static void calc_linear_factors( double x, double f[] )
{
    double x0, x1;
    x = 2*x-1;
    x0 = x+1;
    x1 = x-1;
    f[0] = -x1/2;
    f[1] = x0/2;
}

static int calc_grid_linear( grid_def *def, double x, double y, double *value )
{
    short ny;
    short nx, xrow[2];
    double yfactor[2], xfactor[2];
    int i, j, v, sts;

    for( v = 0; v < def->ngrdval; v++ ) { value[v] = 0; }

    
    sts=calc_grid_cell_xy( def, x, y, &x, &y, &nx, &ny );
    if( sts != OK ) return sts;

    if( ny < 0 || ny+1 >= def->ngrdy ) return INVALID_DATA;
    if( !def->global && (nx < 0 || nx+1 >= def->ngrdx )) return INVALID_DATA;

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
        long *row;
        if( yfactor[i] < 1.0e-8 && yfactor[i] >= -1.0e-8 ) continue;
        row = get_row( def, ny );
        if( !row ) return INVALID_DATA;
        for( v = 0; v < def->ngrdval; v++ )
        {
            double sum = 0.0;
            for( j = 0; j < 2; j++ )
            {
                long rv;
                if( xfactor[j] < 1.0e-8 && xfactor[j] >= -1.0e-8 ) continue;
                rv = row[xrow[j]+v];
                if( rv == def->undef ) return MISSING_DATA;
                sum += rv*xfactor[j];
            }
            value[v] += sum * yfactor[i];
        }
    }
    for( v = 0; v < def->ngrdval; v++ )
    {
        value[v] *= def->vres;
    }
    return OK;
}


int grd_open_grid_file( const std::string &filename, int dimension, grid_def **grid )
{
    grid_def *def;
    short status;
    *grid = NULL;
    status = create_grid_def(&def,filename,dimension);
    if( status != OK ) return status;
    *grid = def;
    return OK;
}

void grd_delete_grid( grid_def *grd )
{
    delete grd;
}

int grd_calc_cubic(  grid_def *grd, double x, double y, double *value )
{
    return calc_grid_cubic( grd, x, y, value );
}

int grd_calc_linear( grid_def *grd, double x, double y, double *value )
{
    return calc_grid_linear( grd, x, y, value );
}

const std::optional<std::string> &grid_def::title( int titleno ) const
{
    if( titleno > 3 ) titleno = 3;
    return titleno == 3 ? desc3 : titleno == 2 ? desc2 : desc1;
}

void grd_grid_spacing( grid_def *grd, double *dx, double *dy )
{
    (*dx) = grd->xres;
    (*dy) = grd->yres;
}

void grd_print_grid_data( grid_def *grd, FILE *out, char showGrid )
{
    fprintf(out,"\n\nDefinition of grid data\n\n  %s\n  %s\n  %s\n",
            grd->desc1 ? grd->desc1->c_str() : nullptr,
            grd->desc2 ? grd->desc2->c_str() : nullptr,
            grd->desc3 ? grd->desc3->c_str() : nullptr );
    fprintf(out,"  Coordinate system code: %s\n", grd->crdsys ? grd->crdsys->c_str() : nullptr );
    fprintf(out,"  X:  maximum %8.4lf  minimum  %8.4lf   increments %4d\n",
            grd->maxx, grd->minx, (int) grd->ngrdx );
    fprintf(out,"  Y: maximum %8.4lf  minimum  %8.4lf   increments %4d\n",
            grd->maxy, grd->miny, (int) grd->ngrdy );
    fprintf(out,"  Vertical resolution: %8.4lf\n", grd->vres);
    fprintf(out,"  Global longitude extents? : %s\n",grd->global ? "Yes" : "No" );
    if( showGrid )
    {
        double lonInc = (grd->maxx - grd->minx)/(grd->ngrdx-1);
        double latInc = (grd->maxy - grd->miny)/(grd->ngrdy-1);
        double lat, lon;
        short i, j, k;
        for( i = 0, lat = grd->miny; i < grd->ngrdy; i++, lat += latInc )
        {
            long *data = get_row( grd, i );
            fprintf( out,"\n");
            fprintf(out, "Row %d starts at location %ld\n",(int) i,grd->rows[i].fileloc);
            for( j = 0, lon = grd->minx; j < grd->ngrdx; j++, lon += lonInc )
            {
                fprintf( out,"  %8.4lf  %8.4lf",lat,lon);
                for( k = 0; k < grd->ngrdval; k++ )
                {
                    long v = (*data);
                    if( v == grd->undef ) fprintf(out,"     *   ");
                    else fprintf( out,"  %8.4lf", (double)(v*grd->vres) );
                    data++;
                }
                fprintf(out,"\n");
            }
        }
    }
    fprintf(out,"\n");
}

