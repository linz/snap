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
#include "util/fileutil.h"
#include "string.h"
#include "util/chkalloc.h"
#include "util/errdef.h"
#include "geoid/griddata.h"
#include "util/pi.h"

#define MAXCACHE 60

#define LOAD(x) if( fread( &(def->x), sizeof(def->x), 1, def->bin ) != 1 ) \
                  { delete_grid_def( def ); return INVALID_DATA; }
#define LOADV(x) if( fread( &(x), sizeof(x), 1, def->bin ) != 1 ) \
                  { delete_grid_def( def ); return INVALID_DATA; }

static void delete_grid_def( grid_def *def )
{
    if( def->bin ) { fclose( def->bin ); def->bin = NULL; }
    if( def->rows ) check_free( def->rows );
    if( def->cache )
    {
        int i;
        for( i = 0; i++ < def->ncache;  )
        {
            if( def->cache[i].data ) check_free( def->cache[i].data );
        }
        check_free( def->cache );
    }
    def->rows = NULL;
    def->cache = NULL;
    if( def->desc1 ) { check_free( def->desc1 ); def->desc1 = NULL; }
    if( def->desc2 ) { check_free( def->desc2 ); def->desc2 = NULL; }
    if( def->desc3 ) { check_free( def->desc3 ); def->desc3 = NULL; }
    if( def->crdsys ) { check_free( def->crdsys ); def->crdsys = NULL; }
    if( def->loadbuffer ) { check_free( def->loadbuffer ); def->loadbuffer = 0; }
    check_free( def );
}


static char *load_string( FILE *bin )
{
    short len;
    char *s;
    if( !fread( &len, sizeof(len), 1, bin ) ) return NULL;
    if( !len ) return NULL;
    s = (char *) check_malloc( len );
    if( !fread( s, len, 1, bin ) ) { check_free(s); s = NULL; }
    return s;
}


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

static int create_grid_def( grid_def **defr, const char *filename, short dimension )
{
    FILE *bin;
    grid_def *def;
    int version;
    long indexloc;
    int i;
    bin = fopen( filename, "rb" );
    if( !bin ) return FILE_OPEN_ERROR;
    version = check_header(bin,&indexloc);
    if( ! version ) { fclose(bin); return INVALID_DATA; }
    def = (grid_def *) check_malloc( sizeof( grid_def ) );
    def->indexloc = indexloc;
    fseek(bin, indexloc, SEEK_SET);
    def->bin = bin;

    def->rows = NULL;
    def->cache = NULL;
    def->maxcache = MAXCACHE;
    def->cache = (cache_row *) check_malloc( sizeof(cache_row) * (def->maxcache+1) );
    for( i = 0; i <= def->maxcache; i++ ) { def->cache[i].data = 0; }
    def->ncache = 0;
    def->cache_mru = NULL;
    def->cache_lru = NULL;
    def->loadbuffer = 0;
    def->rowfmt = version == 3 ? 2 : 1;
    def->undef = 0x7FFFFFFF;

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
        def->latlon = 1;
        def->ngrdval = 1;
    }
    if( def->ngrdval != dimension ) { delete_grid_def(def); return INCONSISTENT_DATA; }
    if( def->ngrdy < 4 || def->ngrdx < 4 ) { delete_grid_def(def); return INVALID_DATA;}
    def->desc1 = load_string( bin );
    def->desc2 = load_string( bin );
    def->desc3 = load_string( bin );
    def->crdsys = load_string( bin );
    def->rows = (file_row *) check_malloc( sizeof(file_row) * def->ngrdy );
#ifdef DEBUG_GRID
    printf("Lat %.4lf - %.4lf\n",def->miny,def->maxy);
    printf("Lon %.4lf - %.4lf\n",def->minx,def->maxx);
    printf("vres %.4lf\n",def->vres);
    printf("ngrdy = %d  ngrdx = %d\n",def->ngrdy,def->ngrdx);
#endif
    for( i = 0; i < def->ngrdy; i++ )
    {
        int loc;
        LOADV( loc );
        def->rows[i].fileloc = loc;
        def->rows[i].cacheloc = NULL;
#ifdef DEBUG_GRID
        printf("%03d %06ld\n",i,def->rows[i].fileloc);
#endif
    }
    def->rowsize = def->ngrdx * def->ngrdval;
    def->yres = (def->maxy - def->miny)/(def->ngrdy-1);
    def->xres = (def->maxx - def->minx)/(def->ngrdx-1);
    def->ncycle = (short) (360/def->xres+0.5);
    def->global = (def->ncycle == def->ngrdx);


    *defr = def;
    return OK;
}


static int load_row1( grid_def *def, long *data )
{
    int i;
    short *sdata;

    /* Format 1 rows - simply held as an array of shorts */

    if ( fread( data, sizeof(short), def->rowsize, def->bin ) != def->rowsize ) return INVALID_DATA;
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
        LOADV(fmt);

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
            LOADV(imin);
            LOADV(imax);
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
            LOADV(d1);
        }
        if( dif > 1 )
        {
            LOADV(d2);
        }

        /* Allocate a buffer for reading if not already don.  Make this
           the largest possible size */

        if( ! def->loadbuffer )
        {
            def->loadbuffer = check_malloc( def->ngrdx * 4 );
            if( ! def->loadbuffer ) return MEM_ALLOC_ERROR;
        }

        /* Read the values from the file */

        i = imax+1-imin;
        if( fread(def->loadbuffer,bytes,i,def->bin) != i) return INVALID_DATA;
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
            cr->data = (long *) check_malloc( def->rowsize * sizeof(long) );
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
    int i, j, v;

    for( v = 0; v < def->ngrdval; v++ ) { value[v] = 0; }
    y = (y-def->miny)/def->yres;
    ny = (short) y;
    if( ny < 1 || ny+2 >= def->ngrdy ) return INVALID_DATA;
    x = (x-def->minx);
    if( def->latlon )
    {
        while( x > 360.0 ) x -= 360.0;
        while( x < 0.0 ) x += 360.0;
    }
    x = x/def->xres;
    nx = (short) x;
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
    int i, j, v;

    for( v = 0; v < def->ngrdval; v++ ) { value[v] = 0; }
    y = (y-def->miny)/def->yres;
    ny = (short) y;
    if( ny < 0 || ny+1 >= def->ngrdy ) return INVALID_DATA;
    x = (x-def->minx);
    if( def->latlon )
    {
        while( x > 360.0 ) x -= 360.0;
        while( x < 0.0 ) x += 360.0;
    }
    x = x/def->xres;
    nx = (short) x;
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


int grd_open_grid_file( const char *filename, int dimension, grid_def **grid )
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
    delete_grid_def( grd );
}

int grd_calc_cubic(  grid_def *grd, double x, double y, double *value )
{
    return calc_grid_cubic( grd, x, y, value );
}

int grd_calc_linear( grid_def *grd, double x, double y, double *value )
{
    return calc_grid_linear( grd, x, y, value );
}

const char *grd_coordsys_def( grid_def *grd )
{
    return grd->crdsys;
}

const char *grd_title( grid_def *grd, int titleno )
{
    char *desc;
    if( titleno > 3 ) titleno = 3;
    switch (titleno )
    {
    case 3:  desc = grd->desc3; break;
    case 2:  desc = grd->desc2; break;
    default: desc = grd->desc1; break;
    }
    return desc;
}

void grd_grid_spacing( grid_def *grd, double *dx, double *dy )
{
    (*dx) = grd->xres;
    (*dy) = grd->yres;
}

void grd_print_grid_data( grid_def *grd, FILE *out, char showGrid )
{
    fprintf(out,"\n\nDefinition of grid data\n\n  %s\n  %s\n  %s\n",
            grd->desc1, grd->desc2, grd->desc3 );
    fprintf(out,"  Coordinate system code: %s\n", grd->crdsys );
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

