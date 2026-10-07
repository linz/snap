#include "snapconfig.h"
/*
   $Log: plotconn.c,v $
   Revision 1.6  2001/07/26 00:46:36  ccrook
   Modified to get classifications ordered alphabetically (previous implementation
   inconsistent with definition of qsort in Borland libraries).

   Revision 1.5  2001/07/18 00:57:15  ccrook
   Updated to display lists of classifications and data files in alphabetical order
   in the key.

   Revision 1.4  1999/10/19 09:05:48  ccrook
   More fixing of obs listing.

   Revision 1.3  1999/10/19 08:17:29  ccrook
   Removed %n to find length of buffer.  This resulted in missing the S.R from
   the display.

   Revision 1.2  1997/04/28 11:00:44  CHRIS
   Added code to support observation listing options - selection of
   fields to display and order of observations.

   Revision 1.1  1996/01/03 22:24:39  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/numeric/conversion/cast.hpp>
using boost::numeric_cast;
#include "util/snapctype.h"
#include "util/fieldscanner.hpp"
#include "util/textformat.hpp"

#ifdef _WIN32
#include <crtdbg.h>
#define assert _ASSERTE
#else
#include <assert.h>
#endif

/* #include <ccwin/cccolour.h> */

#include "snap/snapglob.h"
#include "util/classify.h"
#include "snap/survfile.h"
#include "snapdata/survdata.h"
#include "snap/genparam.h"
#include "snap/rftrans.h"
#include "plotbin.h"
#include "plotconn.h"
#include "plotstns.h"
#include "plotpens.h"
#include "plotfunc.h"
#include "plotscal.h"
#include "snap/stnadj.h"
#include "util/chkalloc.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "snapdata/gpscvr.h"
#include "util/dateutil.h"
#include "util/dms.h"
#include "util/pi.h"

typedef union              /* Location of data on file or in memory */
{
    long floc;
    char *mloc;
} conn_location;

struct conn_ptr
{
    conn_location l;        /* Location of connections (conn_data) */
    int nconn;            /* Number of connections saved   */
    int nalloc;           /* Number of connections allocated */
    char visible;
};

/* conn_ptr's are allocated in blocks of CONN_BLOCK_SIZE */

#define CONN_BLOCK_SIZE 100

static int nspare_conn;
static conn_ptr *conn_block;
static void * conn_block_alloc;

struct conn_cvr
{
    double emin, emax, az;   /* Error ellipse definition */
    double sehgt;
};

/* Data structures used in the binary file holding the definitions of the
   connections.  These are stored in blocks of initial size
   CONN_DATA_INIT, and are doubled each time more is required */

struct conn_data
{
    unsigned char flags;   /* Unused, Reverse direction */
    unsigned char type;    /* data type */
    int file;            /* file id */
    int line;            /* line in file */
    int id;              /* observation id from source file */
    float sres;            /* standardised residual */
    float rfac;            /*  factor */
    double date;           /*  date of observation */
    int64_t bloc;          /* location of binary data in data source */
    int idata;           /* Index of data in data block in file */
    int cclass[1];        /* Classifications */
};

#define CONN_DATA_INIT 4

/* tconn_ptr's are located in dynamically expanded arrays in fconn_ptr */
/* The initial size is TCONN_ARRAY_INIT and the expansion is in units of
   TCONN_ARRAY_INC */

struct tconn_ptr
{
    conn_ptr *conn;
    int to;
};

#define TCONN_ARRAY_INIT 5
#define TCONN_ARRAY_INC 5

struct fconn_ptr
{
    int nconn;
    int max_conn;
    int eastmost_conn;
    tconn_ptr *to;
};

/* Flags used in definitions of connections */

#define CONN_UNUSED      0x01
#define CONN_REJECTED    0x02
#define CONN_REVERSE     0x04  /* point obs set none, directional obs set one */
#define CONN_FORWARD     0x08  /* non-directional obs set both.               */
#define CONN_OBS_REVERSE 0x10  /* Observation reversed from file definition   */

#define MAX_TICK   20

static int nclass = 0;
static int conn_data_size;
static fconn_ptr *connlst = NULL;
static FILE *conn_file = NULL;
static conn_data *connection;
static conn_data *connection2;
static int max_conn = 0;
static int max_obs = 0;
static long ndata;

static double relcvrmaxh = 0.0;
static double relcvrmaxv = 0.0;

static int binary_data = 0;
static int aposteriori_sres = 1;

static int highlight_type;
static double highlight_threshhold;

static double hlt_offset = 0.0;   /* Used to show individual connections */

static char use_conn_file = 0;

/* Code to set up all the information for the connections between observations.

   Each station has a list of connected stations in a fconn_ptr structure.

   This contains an array of tconn_ptr's which contain the target station id
   and a pointer to the conn_ptr structure defining the actual data.
   Pointers to the conn_ptr are placed in both the instrument and target
   station lists.

   The fconn_ptr also contains the coordinate of the most easterly connected
   station.  This is to facilitate searching for the nearest connection to
   a point.  This member is not initialised until it is first needed.
   (as the station plot coordinates are not defined until then).

   The conn_ptr in turn is a pointer to the actual data, stored in a
   conn_data structure.  This may be either in memory or in a temporary
   file, according to the setting of the variable use_conn_file.

   The conn_data structure is not of a fixed size, since it must
   accommodate an arbitrary number of classifications (fixed for each
   adjustment - it holds a value for every possible classification).

   The size of the object is conn_data_size.
*/

/* Initialise the list, determine the size of a connection data block, and
   allocate one to be used for loading the connection list. Open the
   temporary file */

static void init_connection_list( void )
{
    int i, maxstn;
    maxstn = number_of_stations( net ) + 1;
    connlst = (fconn_ptr *) check_malloc( maxstn * sizeof(fconn_ptr) );
    for( i = 0; i<maxstn; i++ )
    {
        connlst[i].nconn = connlst[i].max_conn = 0;
        connlst[i].eastmost_conn = -1;
        connlst[i].to = NULL;
    }
    if( use_conn_file )
    {
        conn_file = snaptmpfile();
        if( !conn_file )
        {
            handle_error( FATAL_ERROR,
                          "Cannot open scratch file for connection list\n", NO_MESSAGE );
        }
    }

    nclass = obs_classes.count();
    conn_data_size = sizeof( conn_data ) + (nclass - 1) * sizeof( int );
    connection = (conn_data *) check_malloc( conn_data_size );
    connection2 = (conn_data *) check_malloc( conn_data_size );
    conn_block_alloc = NULL;
    conn_block = NULL;
    nspare_conn = 0;
    ndata = 0;
}

static void free_connection_blocks();

static void free_connection_list()
{
    int i;
    int maxstn;

    check_free( connection );
    connection = NULL;
    check_free( connection2 );
    connection2 = NULL;

    free_connection_blocks();

    if( connlst )
    {
        maxstn = number_of_stations( net ) + 1;
        for(  i = 0; i < maxstn; i++ )
        {
            check_free( connlst[i].to );
            connlst[i].to = NULL;
            connlst[i].nconn = 0;
            connlst[i].max_conn = 0;
            connlst[i].eastmost_conn = -1;
        }
        check_free( connlst );
    }
    connlst = NULL;
}


static void create_conn_data( conn_ptr *cp );

static conn_ptr *allocate_conn_ptr( void )
{
    conn_ptr *c;
    if( nspare_conn <= 0 )
    {
        void *old_conn_block_alloc = conn_block_alloc;
        conn_block_alloc = (void *) check_malloc( sizeof( void *) + CONN_BLOCK_SIZE * sizeof( conn_ptr ) );
        *(void **)conn_block_alloc = (void *) old_conn_block_alloc;
        conn_block = (conn_ptr *)(((char *)conn_block_alloc) + sizeof(void *));
        nspare_conn = CONN_BLOCK_SIZE;
    }
    c = conn_block++;
    nspare_conn--;
    create_conn_data( c );
    return c;
}

static void free_connection_blocks()
{
    while( conn_block_alloc )
    {
        void *nextblock = *(void **)conn_block_alloc;
        check_free( conn_block_alloc );
        conn_block_alloc = nextblock;
    }
}


static void add_tconn_to_fconn( int from, int to, conn_ptr *cp )
{
    fconn_ptr *fp;
    tconn_ptr *tp;
    fp = &connlst[from];
    if( fp->nconn >= fp->max_conn )
    {
        if( fp->max_conn )
        {
            fp->max_conn += TCONN_ARRAY_INC;
            fp->to = (tconn_ptr *) check_realloc( fp->to, fp->max_conn * sizeof(tconn_ptr) );
        }
        else
        {
            fp->max_conn = TCONN_ARRAY_INIT;
            fp->to = (tconn_ptr *) check_malloc( fp->max_conn * sizeof(tconn_ptr) );
        }
    }
    tp = &fp->to[fp->nconn++];
    if( fp->nconn > max_conn ) max_conn = fp->nconn;
    tp->to = to;
    tp->conn = cp;
}


static conn_ptr *get_connection( int from, int to )
{
    fconn_ptr *fp;
    tconn_ptr *tp;
    conn_ptr *cp;
    int i;

    fp = &connlst[from];
    for( i = fp->nconn, tp = fp->to; i--; tp++ )
    {
        if( tp->to == to ) return tp->conn;
    }
    cp = allocate_conn_ptr();
    add_tconn_to_fconn( from, to, cp );
    if( to ) add_tconn_to_fconn( to, from, cp );
    return cp;
}


/* Create a data block at the end of the file */

static void create_data_space( conn_location *l, int nalloc )
{
    conn_cvr cvr = { -1.0, -1.0, -1.0, -1.0 };
    if( use_conn_file )
    {
        fseek( conn_file, 0L, SEEK_END );
        l->floc = ftell( conn_file );
        fwrite( &cvr, sizeof(cvr), 1, conn_file );
        while( nalloc-- )
        {
            fwrite( connection, conn_data_size, 1, conn_file );
        }
    }
    else
    {
        l->mloc = (char *) check_malloc( sizeof(cvr) + nalloc * conn_data_size );
        memcpy( l->mloc, &cvr, sizeof(cvr) );
    }
}


static void expand_data_space( conn_location *l, int oldsize, int newsize )
{
    if( use_conn_file )
    {
        int dsize;
        unsigned char *dblock;

        dsize = sizeof(conn_cvr) + oldsize * conn_data_size;
        dblock = (unsigned char *) check_malloc( dsize );

        fseek( conn_file, l->floc, SEEK_SET );
        fread( dblock, dsize, 1, conn_file);

        fseek( conn_file, 0L, SEEK_END );
        l->floc = ftell( conn_file );
        fwrite( dblock, dsize, 1, conn_file );
        fwrite( dblock, dsize-sizeof(conn_cvr), 1, conn_file );
        check_free( dblock );
        fseek( conn_file, 0L, SEEK_END );
        while( oldsize++ < newsize )
        {
            fwrite( connection, conn_data_size, 1, conn_file );
        }
    }
    else
    {
        l->mloc = (char *) check_realloc( l->mloc,
                                          sizeof(conn_cvr) + newsize * conn_data_size );
    }
}

/*
static void free_conn_data( conn_ptr *cp )
{
    if( ! use_conn_file )
    {
        cp->nconn = 0;
        cp->nalloc = 0;
        check_free( cp->l.mloc );
        cp->l.mloc = 0;
    }
}
*/


static void create_conn_data( conn_ptr *cp )
{
    create_data_space( &cp->l, CONN_DATA_INIT );
    cp->nconn = 0;
    cp->nalloc = CONN_DATA_INIT;
}


/* Copy the data block to the end of the file and expand it */

static void expand_conn_data( conn_ptr *cp )
{
    expand_data_space( &cp->l, cp->nalloc, cp->nalloc*2 );
    cp->nalloc *= 2;
}

/* Get the conn_ptr structure for a connection */

/* Add a block of data to the end of the file */

static void add_fconn_data( int from, int to, conn_data *data )
{
    conn_ptr *ptr;

    if( !connlst ) init_connection_list();


    if( from == 0 ) { from = to; to = 0; }
    if( datatype[data->type].ispoint )
    {
        to = 0;
    }
    else if( from < to )
    {
        int tmp;
        tmp = from; from = to; to = tmp;
        data->flags |= CONN_REVERSE | CONN_OBS_REVERSE;
    }

    else
    {
        data->flags |= CONN_FORWARD;
    }

    if( to && ! datatype[data->type].isdirectional )
    {
        data->flags |= (CONN_REVERSE | CONN_FORWARD );
    }

    ptr = get_connection( from, to );
    if( ptr->nconn >= ptr->nalloc ) expand_conn_data( ptr );

    if( use_conn_file )
    {
        long loc;
        loc = ptr->l.floc + sizeof(conn_cvr) + ptr->nconn * conn_data_size;
        fseek( conn_file, loc, SEEK_SET );
        fwrite( data, conn_data_size, 1, conn_file );
    }
    else
    {
        char *loc;
        loc = ptr->l.mloc + sizeof(conn_cvr) + ptr->nconn * conn_data_size;
        memcpy( loc, data, conn_data_size );
    }

    ptr->nconn++;
    if( ptr->nconn > max_obs ) max_obs = ptr->nconn;
    ndata++;
}


void add_relative_covariance( int from, int to, double cvr[] )
{
    conn_cvr covar;
    conn_ptr *ptr;
    if( from < to ) { int tmp = from; from = to; to = tmp; }

    calc_error_ellipse( cvr, &covar.emax, &covar.emin,
                        &covar.az );
    if( covar.emax > relcvrmaxh ) relcvrmaxh = covar.emax;
    covar.sehgt = cvr[5] > 0.0 ? sqrt(cvr[5]) : 0.0;
    if( covar.sehgt > relcvrmaxv ) relcvrmaxv = covar.sehgt;

    if( !connlst ) init_connection_list();

    ptr = get_connection( from, to );
    if( use_conn_file )
    {
        fseek( conn_file, ptr->l.floc, SEEK_SET );
        fwrite( &covar, sizeof(covar), 1, conn_file );
    }
    else
    {
        memcpy( ptr->l.mloc, &covar, sizeof(covar) );
    }
}

void maximum_relative_covariance( double *h, double *v )
{
    if( h ) *h = relcvrmaxh;
    if( v ) *v = relcvrmaxv;
}


/* Get the data from a conn_data structure */

static int get_connection_data( int from, tconn_ptr *tp, conn_data *cd, int first )
{
    static int nleft = 0;
    static char *mloc = NULL;
    static char reverse;
    conn_ptr *p = tp->conn;
    if( p->nconn == 0 ) return 0;
    if( first )
    {
        if( use_conn_file )
        {
            fseek( conn_file, p->l.floc + sizeof(conn_cvr), SEEK_SET );
        }
        else
        {
            mloc = p->l.mloc + sizeof(conn_cvr);
        }
        nleft = p->nconn;
        reverse = from < tp->to ? 1 : 0;
    }

    if( nleft <= 0 ) return 0;
    nleft--;
    if( use_conn_file )
    {
        fread( cd, conn_data_size, 1, conn_file );
    }
    else
    {
        memcpy( cd, mloc, conn_data_size );
        mloc += conn_data_size;
    }
    if( reverse )
    {
        cd->flags ^= CONN_OBS_REVERSE;
        if( datatype[cd->type].isdirectional )
            cd->flags ^= CONN_FORWARD | CONN_REVERSE;
    }
    return 1;
}

static void get_connection_data_by_id( int from, int to_id, int obs_id, conn_data *cd  )
{
    fconn_ptr *fp;
    tconn_ptr *tp;
    int offset;
    conn_ptr *p;
    fp = &connlst[from];
    tp = &fp->to[to_id];
    p = tp->conn;
    offset = sizeof(conn_cvr) + obs_id * conn_data_size;
    if( use_conn_file )
    {
        fseek( conn_file, p->l.floc + offset, SEEK_SET );
        fread( cd, conn_data_size, 1, conn_file );
    }
    else
    {
        memcpy( cd, p->l.mloc + offset, conn_data_size );
    }
    if( from < tp->to )
    {
        cd->flags ^= CONN_OBS_REVERSE;
        if( datatype[cd->type].isdirectional )
            cd->flags ^= CONN_FORWARD | CONN_REVERSE;
    }
}


static int get_connection_to_id( int from, int to )
{
    fconn_ptr *fp;
    int to_id;
    if( from == 0 ) { from = to; to = 0; }
    fp = &connlst[from];
    for( to_id = 0; to_id < fp->nconn; to_id++ )
    {
        if( fp->to[to_id].to == to ) return to_id;
    }
    return -1;
}


static int get_connection_obs_id( int from, int to_id, int64_t bloc, int index )
{
    fconn_ptr *fp;
    tconn_ptr *tp;
    int obs_id;

    fp = &connlst[from];
    tp = &fp->to[to_id];
    for( obs_id = 0;
            get_connection_data( from, tp, connection, (obs_id ? 0 : 1) );
            obs_id++ )
    {

        if( connection->bloc == bloc &&
                connection->idata == index ) return obs_id;
    }
    return 0;
}


void get_relative_covariance( conn_ptr *ptr, conn_cvr *cvr )
{
    if( use_conn_file )
    {
        fseek( conn_file, ptr->l.floc, SEEK_SET );
        fread( cvr, sizeof(conn_cvr), 1, conn_file );
    }
    else
    {
        memcpy( cvr, ptr->l.mloc, sizeof(conn_cvr) );
    }
}

/* Record an observation in the connection list */

void set_binary_data( int status )
{
    binary_data = status;
}

int have_binary_data()
{
    return binary_data;
}

void add_survdata_connections( survdata *sd, int64_t bloc )
{
    trgtdata *t;
    int i, j, iclass;

    if( !connlst ) init_connection_list();

    connection->bloc = bloc;

    for( i = 0; i < sd->nobs; i++ )
    {
        t = get_trgtdata( sd, i );
        obstypecount[t->type]++;
        connection->idata = i;
        connection->flags = t->unused ? CONN_UNUSED : 0;
        if( t->unused & REJECT_OBS_BIT ) connection->flags |= CONN_REJECTED;
        connection->type = t->type;
        connection->file = sd->file;
        connection->line = t->lineno;
        connection->date = sd->date;
        connection->id = t->id;
        connection->rfac = 1.0;    /* Completely redundant if not used */
        if( binary_data )
        {
            switch( sd->format )
            {
            case SD_OBSDATA:
            {
                obsdata *o;
                o = &sd->obs.odata[i];
                if(  !t->unused && !rejected_station(sd->from) &&
                        !rejected_station(t->to) && o->error > 0.0 )
                {
                    connection->rfac = o->reserr/o->error;
                    if( connection->rfac < 0.0 ) connection->rfac = 0.0;
                }
                break;
            }
            case SD_VECDATA:
            {
                /* This one is tricky !!!! How can I put a single
                   number to the three components.... */
                double oe, re;
                double cvr[6];
                calc_vecdata_vector( sd, VD_REF_STN, i, VD_OBSVEC, NULL, cvr );
                oe = cvr[0] + cvr[2] + cvr[5];
                calc_vecdata_vector( sd, VD_REF_STN, i, VD_RESVEC, NULL, cvr );
                re = cvr[0] + cvr[2] + cvr[5];
                if( ! t->unused && ! rejected_station(sd->from) && ! rejected_station(t->to) )
                {
                    connection->rfac = 0.0;
                    if( re > 0.0 && oe > 0.0 )connection->rfac = sqrt( re/oe);
                }
                break;
            }
            case SD_PNTDATA:
            {
                pntdata *p;
                p = &sd->obs.pdata[i];
                if(  !t->unused && !rejected_station(sd->from)
                        && p->error > 0.0 )
                {
                    connection->rfac = p->reserr/p->error;
                    if( connection->rfac < 0.0 ) connection->rfac = 0.0;
                }

                break;
            }
            }
        }

        connection->sres = 0.0;
        if( binary_data )
        {
            switch( sd->format )
            {
            case SD_OBSDATA: connection->sres = sd->obs.odata[i].sres; break;
            case SD_VECDATA: connection->sres = sd->obs.vdata[i].vsres; break;
            case SD_PNTDATA: connection->sres = sd->obs.pdata[i].sres; break;
            }
        }
        if( nclass )
        {
            for( iclass = 0; iclass < nclass; iclass++ )
            {
                connection->cclass[iclass] = 0;
            }
            iclass = t->nclass;
            if( iclass )
            {
                classdata *cd;
                for( cd = sd->clsf + t->iclass; iclass--; cd++ )
                {
                    if( cd->class_id <= nclass )
                        connection->cclass[cd->class_id - 1] = cd->name_id;
                }
            }
        }
        add_fconn_data( sd->from, t->to, connection );
    }

    /* For multistation GPS data - add missing lines between
       stations.  Point data is excluded because sd->from = 0 */

    if( sd->format == SD_VECDATA && sd->nobs > 1 && sd->from )
    {
        for( i = 0; i < sd->nobs; i++ ) for ( j = i+1; j < sd->nobs; j++ )
            {
                trgtdata *t2;
                char unused;
                t = get_trgtdata( sd, i );
                t2 = get_trgtdata( sd, j );
                obstypecount[t->type]++;
                connection->idata = i + j * sd->nobs;
                unused = t->unused | t2->unused;
                connection->flags = unused ? CONN_UNUSED : 0;
                if( unused & REJECT_OBS_BIT ) connection->flags |= CONN_REJECTED;
                connection->type = t->type;
                connection->file = sd->file;
                connection->line = t->lineno;
                connection->rfac = 1.0;    /* Completely redundant if not used */
                if( binary_data )
                {
                    /* This one is tricky !!!! How can I put a single
                       number to the three components.... */
                    double oe, re;
                    double cvr[6];
                    calc_vecdata_vector( sd, i, j, VD_OBSVEC, NULL, cvr );
                    oe = cvr[0] + cvr[2] + cvr[5];
                    calc_vecdata_vector( sd, i, j, VD_RESVEC, NULL, cvr );
                    re = cvr[0] + cvr[2] + cvr[5];
                    if( oe > 0.0 ) connection->rfac = sqrt( re/oe);
                }

                connection->sres = 0.0;
                if( binary_data )
                {
                    double vec[3], cvr[6];
                    int rank;
                    calc_vecdata_vector( sd, i, j, VD_RESVEC, vec, cvr );
                    connection->sres = vector_standardised_residual( vec, cvr, &rank );
                }
                if( nclass )
                {
                    for( iclass = 0; iclass < nclass; iclass++ )
                    {
                        connection->cclass[iclass] = 0;
                    }
                    iclass = t->nclass;
                    if( iclass )
                    {
                        classdata *cd;
                        for( cd = sd->clsf + t->iclass; iclass--; cd++ )
                        {
                            if( cd->class_id <= nclass )
                                connection->cclass[cd->class_id - 1] = cd->name_id;
                        }
                    }
                }
                add_fconn_data( t->to, t2->to, connection );
            }
    }
}

/*===============================================================*/
/* Code to manage the pens used to plot data                     */

static int data_pen_type;

// Which Display-by dimensions are currently enabled as filters - on by
// default, user-toggleable.
static std::set<int> display_by_enabled = { DISPLAYBY_DATATYPE, DISPLAYBY_DATAFILE, DISPLAYBY_OBSSTATUS };

bool is_displayby_enabled( const int id )
{
    return display_by_enabled.count( id ) > 0;
}

void set_displayby_enabled( const int id, const bool enabled )
{
    if( enabled ) {
        display_by_enabled.insert( id );
    } else {
        display_by_enabled.erase( id );
    }
}

// Range i runs up to range_values[i]. The names are in key order, which is
// the reverse of range_values order if reverse_range is set.
static std::vector<double> range_values;
static std::vector<std::string> range_names;
static bool reverse_range = false;

static double maxsres = 3.0;
static int  nsres = 6;
static int  nrfac = 5;
static double sresmult;

static void free_range_pens()
{
    range_names.clear();
    range_values.clear();
}

static void setup_ranges( double maxval, int ninterval, const bool reverse, const std::string &name, const std::string_view prefix )
{
    if( maxval < 0.0 ) maxval = 1.0;
    if( maxval > 100.0 ) maxval = 100.0;
    if( ninterval < 2 || ninterval > 20 ) ninterval = 5;
    ninterval++;
    free_range_pens();
    reverse_range = reverse;
    range_values.resize( ninterval );
    range_names.resize( ninterval );

    for( int i = 0; i < ninterval; i++ )
    {
        range_values[i] = (maxval * (i+1))/(ninterval-1);
        std::string label = std::string( prefix ) + std::to_string( i ) + "|";
        if( i == 0 )
        {
            label += " 0.00 - " + format_fixed( range_values[i], 2, 5 );
        }
        else if( i == ninterval-1 )
        {
            label += format_fixed( range_values[i-1], 2, 5 ) + " - ";
        }
        else
        {
            label += format_fixed( range_values[i-1], 2, 5 ) + " - " + format_fixed( range_values[i], 2, 5 );
        }
        range_names[reverse_range ? ninterval-1-i : i] = std::move( label );
    }

    // Only give the list its default rainbow colouring when it's freshly
    // built - a cache hit means copy_layer() has already restored whatever
    // colours were previously chosen, and this would overwrite them.
    if( setup_data_layers( range_names, name, false ) ) {
        set_pen_colour_range();
    }
}

static int get_range_pen( double value )
{
    const int nranges = boost::numeric_cast<int>( range_values.size() );
    int i;
    for( i=0; i<nranges-1; i++ )
    {
        if( range_values[i] > value ) break;
    }
    if( reverse_range ) i = nranges - i - 1;
    return i;
}

static void setup_datatype_pens( void )
{
    setup_data_layers( {}, "", false );
}

static const char *nmApostStdRes = "Aposteriori std residuals";
static const char *nmStdRes = "Apriori std residuals";
static const char *cdStdRes = "SRS_";
static const char *nmRedundancy = "Redundancy";
static const char *cdRedundancy = "RDC_";

// Data file's content now lives in data_file_layers, the always-shown filter
// list, instead of a cached data_user_layers entry - this just needs
// data_user_layers left null, the same way setup_datatype_pens() does.
static void setup_datafile_pens( void )
{
    setup_data_layers( {}, "", false );
}

// Evicts the cached residual list before overwriting maxsres/nsres if either
// is actually changing, so it rebuilds fresh with the new bins next time
// residual colouring is selected. Both header variants are evicted since
// apriori/aposteriori share these parameters.
void setup_sres_pens( double max, int apost, int npens )
{
    if( max != maxsres || npens != nsres ) {
        invalidate_data_user_layer_cache( nmApostStdRes );
        invalidate_data_user_layer_cache( nmStdRes );
    }
    maxsres = max;
    nsres = npens;
    aposteriori_sres = apost;
}

void get_sres_pen_options( double *max, int *apost, int *npens )
{
    *max = maxsres;
    *apost = aposteriori_sres;
    *npens = nsres;
}

// Evicts the cached redundancy list before overwriting nrfac if it's
// actually changing, so it rebuilds fresh with the new bins next time
// redundancy colouring is selected.
void setup_rfac_pens( int npens )
{
    if( npens != nrfac ) {
        invalidate_data_user_layer_cache( nmRedundancy );
    }
    nrfac = npens;
}

void get_rfac_pen_options( int *npens )
{
    *npens = nrfac;
}

// data_pen_type is set before each setup call below, not after - the setup
// calls chain through to setup_snapplot_symbology(), which reads
// get_data_pen_type() to decide which list's colour swatches are currently
// editable, so it needs the new mode, not the one being switched away from.
void setup_data_pens( int type )
{
    if( type == DPEN_BY_SRES )
    {
        data_pen_type = DPEN_BY_SRES;
        setup_ranges( maxsres, nsres, true, aposteriori_sres ? nmApostStdRes : nmStdRes, cdStdRes );
        sresmult = 1.0;
        if( aposteriori_sres ) sresmult = 1.0/seu;
    }

    else if( type == DPEN_BY_RFAC )
    {
        data_pen_type = DPEN_BY_RFAC;
        setup_ranges( 1.0, nrfac, false, nmRedundancy, cdRedundancy );
    }

    else if( type == DPEN_BY_FILE )
    {
        data_pen_type = DPEN_BY_FILE;
        setup_datafile_pens();
    }
    else if( type > 0 && type <= nclass )
    {
        data_pen_type = type;
        setup_classification_pens_layers( type );
    }
    else
    {
        data_pen_type = DPEN_BY_TYPE;
        setup_datatype_pens();
    }
}

// Returns the pen-type code of the currently active observation colour-by mode.
int get_data_pen_type()
{
    return data_pen_type;
}

/// Parses the whole of field as an int, with no trailing characters.
static std::optional<int> parse_whole_int( const std::string_view field )
{
    const auto parsed = parse_leading_field<int>( field );
    if( !parsed || parsed->result.ptr != field.data() + field.size() ) return std::nullopt;
    return parsed->value;
}

int set_datapen_definition( const std::string_view def )
{
    constexpr std::string_view delimiters = " \t\n\r";
    FieldScanner scanner( def );
    auto fld = scanner.nextToken( delimiters );
    if( !fld ) return MISSING_DATA;
    int pen_data_type;
    if( boost::algorithm::iequals( *fld, "by_type" ) )
    {
        pen_data_type = DPEN_BY_TYPE;
    }
    else if( boost::algorithm::iequals( *fld, "by_file" ) )
    {
        pen_data_type = DPEN_BY_FILE;
    }
    else if( boost::algorithm::iequals( *fld, "by_redundancy" ) )
    {
        int ngrp = 5;
        fld = scanner.nextToken( delimiters );
        if( fld )
        {
            const auto parsedGroups = parse_whole_int( *fld );
            if( !parsedGroups ) return INVALID_DATA;
            ngrp = *parsedGroups;
        }
        pen_data_type = DPEN_BY_RFAC;
        nrfac = ngrp;
    }
    else if( boost::algorithm::iequals( *fld, "by_std_residual" ) )
    {
        double maxsr = 3.0;
        int ngrp = 6;
        int apost = 1;
        fld = scanner.nextToken( delimiters );
        if( fld && boost::algorithm::iequals( *fld, "aposteriori" ) )
        {
            fld = scanner.nextToken( delimiters );
        }
        else if( fld && boost::algorithm::iequals( *fld, "apriori" ) )
        {
            apost = 0;
            fld = scanner.nextToken( delimiters );
        }
        if( fld )
        {
            const auto parsedMaxsr = parse_double( *fld );
            if( !parsedMaxsr ) return INVALID_DATA;
            maxsr = *parsedMaxsr;
            fld = scanner.nextToken( delimiters );
            if( fld )
            {
                const auto parsedGroups = parse_whole_int( *fld );
                if( !parsedGroups ) return INVALID_DATA;
                ngrp = *parsedGroups;
            }
        }

        pen_data_type = DPEN_BY_SRES;
        maxsres = maxsr;
        aposteriori_sres = apost;
        nsres = ngrp;
    }
    else if( boost::algorithm::iequals( *fld, "by_classification" ) )
    {
        int i, found;
        fld = scanner.nextToken( delimiters );
        if( ! fld ) return MISSING_DATA;
        found = 0;
        for( i = 0; i++ < obs_classes.count(); )
        {
            if( boost::algorithm::iequals( *fld, obs_classes.name(i)) )
            {
                found = 1;
                break;
            }
        }
        if( !found ) return INVALID_DATA;
        pen_data_type = i;
    }
    else
    {
        return INVALID_DATA;
    }

    setup_data_pens( pen_data_type );
    /* pd_redraw(); */
    return OK;
}

std::string get_datapen_definition()
{
    switch( data_pen_type )
    {
    case DPEN_BY_TYPE:  return "by_type";
    case DPEN_BY_FILE:  return "by_file";
    case DPEN_BY_RFAC:  return "by_redundancy " + std::to_string( nrfac ) + "\n";
    case DPEN_BY_SRES:  return std::string( "by_std_residual " ) +
                               ( aposteriori_sres ? "aposteriori " : "apriori " ) +
                               format_fixed( maxsres, 3 ) + " " + std::to_string( nsres ) + "\n";
    default:            return "by_classification " + obs_classes.name( data_pen_type );
    }
}

static void plot_connection( map_plotter *plotter, int from, int to, int pen, unsigned char flags,
                             double offset_along, double offset_perp )
{
    double x1, y1, x2, y2;

    get_station_coordinates( from, &x1, &y1 );
    get_station_coordinates( to, &x2, &y2 );

    if( offset_along > 0.0 || offset_perp != 0.0 )
    {
        double dx, dy, dh;
        dx = x2 - x1;
        dy = y2 - y1;
        dh = _hypot( dx, dy );
        if( dh <= 0 || dh < offset_along*2.0 ) return;
        dx /= dh;
        dy /= dh;
        x1 += offset_along * dx;
        x2 -= offset_along * dx;
        y1 += offset_along * dy;
        y2 -= offset_along * dy;
        if( dx < 0 ) offset_perp = -offset_perp;
        x1 -= offset_perp * dy;
        x2 -= offset_perp * dy;
        y1 += offset_perp * dx;
        y2 += offset_perp * dx;
    }

    if( flags & CONN_FORWARD && flags & CONN_REVERSE )
    {
        LINE(plotter, x1, y1, pen );
        LINE(plotter, x2, y2, CONTINUE_LINE );
        return;
    }
    else
    {
        double xm = (x1+x2)/2;
        double ym = (y1+y2)/2;
        if( flags & CONN_REVERSE )
        {
            double tmp = x1; x1 = x2; x2 = tmp;
            tmp = y1; y1 = y2; y2 = tmp;
        }
        LINE(plotter, x1, y1, pen );
        LINE(plotter, xm, ym, CONTINUE_LINE );
        DASHED_LINE(plotter, xm, ym, pen );
        LINE(plotter,x2,y2,CONTINUE_LINE);
    }

}


void set_obs_highlight_option( int type, double value )
{
    highlight_type = type;
    highlight_threshhold = value;
}

void get_obs_highlight_option( int *type, double *value )
{
    *type = highlight_type;
    *value = highlight_threshhold;
}

static int obs_station_showable( int istn )
{
    return show_hidden_stn_obs || station_showable(istn);
}

struct pendef
{
    int pen;
    unsigned char flags;
    unsigned char highlight;
};

static int max_pens;
static pendef *pens = NULL;

int plot_connections( map_plotter *plotter, int first, int offset_opt, double offset, int redraw )
{
    int hltpen, count, maxstn;
    int pltused, pltrejected, pltunused;
    int pltrelerr, pltrelhgt;
    int ellpen = 0, hgtpen = 0;
    int i, j;
    int highlight;
    char from_highlighted, highlight_line;
    int hlttype;
    int show_oneway_obs;
    double offset_prp;
    double eemult = 0.0;
    double mide, midn;
    int npendef, nhlt;
    int from;
    fconn_ptr *fp;
    tconn_ptr *tp;
    int ntp;
    int first_conn;
    conn_cvr cvr;

    if( !connlst ) return ALL_DONE;

    highlight = redraw == PCONN_REDRAW_HIGHLIGHT ? 1 : 0;

    pltused = option_selected( USED_OBS_OPT );
    pltrejected = option_selected(REJECTED_OBS_OPT);
    pltunused = option_selected(UNUSED_OBS_OPT);

    pltrelerr = option_selected( REL_ELL_OPT );
    if( pltrelerr )
    {
        ellpen = get_pen( REL_ELL_PEN );
        if( !pen_visible(ellpen) ) pltrelerr = 0;
        eemult = errell_factor * rel_errell_scale;
    }

    pltrelhgt = option_selected( REL_HGT_OPT );
    if( pltrelhgt )
    {
        hgtpen = get_pen( REL_HGT_PEN );
        if( !pen_visible(hgtpen) ) pltrelhgt = 0;
    }

    maxstn = station_count();

    hltpen = get_pen( HIGHLIGHT_PEN );
    hlttype = highlight_type;
    if( !pen_visible(hltpen) ) hlttype = PCONN_HIGHLIGHT_NONE;

    if( highlight )
    {
        if( hlttype == PCONN_HIGHLIGHT_NONE ) return ALL_DONE;
        if( ( hlttype == PCONN_HIGHLIGHT_IF_BOTH ||
                hlttype == PCONN_HIGHLIGHT_IF_EITHER ) &&
                !highlighted_stations() ) return ALL_DONE;
        if( hlttype == PCONN_HIGHLIGHT_REJECTED && !pltrejected ) return ALL_DONE;
        if( hlttype == PCONN_HIGHLIGHT_UNUSED && !pltunused ) return ALL_DONE;
    }

    if( !pltused && !pltrejected && !pltunused ) return ALL_DONE;

    if( !pens ) pens = (pendef *) check_malloc( sizeof(pendef) * max_obs );

    if( first < 0 ) { first = 0; count = maxstn; }
    else count = 1;

    if( first == 0 && !redraw) max_pens = 0;

    show_oneway_obs = offset_opt & PCONN_SHOW_ONEWAY_OBS;
    offset_opt &= PCONN_MERGE_FLAGS;
    /* if( offset == 0.0 ) offset_opt = PCONN_ONE_CONNECTION;
       Don't do this as may want all layers in a DXF file */

    for( from = first; from++ < maxstn; )
    {

        if( !count-- ) return from-1;

        if( ! obs_station_showable( from ) ) continue;

        if( ! station_in_view( from ) ) continue;

        from_highlighted = station_highlighted( from );

        if( highlight &&
                hlttype == PCONN_HIGHLIGHT_IF_BOTH &&
                ! from_highlighted ) continue;

        /* Plot the connections observations */

        fp = &connlst[from];

        for( tp = fp->to, ntp = fp->nconn; ntp--; tp++ )
        {

            /* Skip over point data */

            if( !tp->to ) continue;

            /* Don't plot connections to lower numbered visible stations -
               avoids plotting lines twice */

            if( tp->to < from && station_in_view(tp->to) ) continue;

            /* If not selected then skip it */

            if( ! obs_station_showable(tp->to) ) continue;

            /* Flag the line as not visible - will reset flag if subsequently
               plot it */

            if( !redraw) tp->conn->visible = 0;

            /* If highlighting, then check whether both ends must be
               highlighted or only one, and skip if it fails on these
               conditions */

            highlight_line = 1;
            if( station_highlighted(tp->to) )
            {
                if( hlttype == PCONN_HIGHLIGHT_IF_BOTH && !from_highlighted )
                {
                    highlight_line = 0;
                }
            }
            else
            {
                if( hlttype == PCONN_HIGHLIGHT_IF_BOTH ||
                        (hlttype == PCONN_HIGHLIGHT_IF_EITHER &&
                         !from_highlighted) )
                {
                    highlight_line = 0;
                }
            }
            if( highlight && !highlight_line ) continue;

            /* Read the connections and form a sorted list of pens */

            npendef = 0;
            nhlt = 0;
            for( first_conn = 1;
                    get_connection_data( from, tp, connection, first_conn);
                    first_conn = 0 )
            {
                int cpen;
                unsigned char cflags;

                /* Skip if it is not to be used */
                if( connection->flags & CONN_REJECTED )
                {
                    if( !pltrejected ) continue;
                }
                else if( connection->flags & CONN_UNUSED )
                {
                    if( !pltunused ) continue;
                }
                else
                {
                    if( !pltused ) continue;
                }
                if( !datatype_selected( connection->type ) ) continue;
                if( !filetype_selected( connection->file ) ) continue;

                // Also filter by every classification currently enabled as
                // a Display-by filter, other than whichever one (if any) is
                // the active Colour-by mode - checked below via
                // pen_selected(cpen).
                bool classificationFilteredOut = false;
                for( int classType = 1; classType <= nclass; classType++ ) {
                    if( classType == data_pen_type ) {
                        continue;
                    }
                    if( ! is_displayby_enabled( classType ) ) {
                        continue;
                    }
                    const bool valueSelected = classification_value_selected( classType, connection->cclass[classType-1] );
                    if( ! valueSelected ) {
                        classificationFilteredOut = true;
                        break;
                    }
                }
                if( classificationFilteredOut ) {
                    continue;
                }

                /* Otherwise determine which pen is to be used */
                switch( data_pen_type )
                {
                case DPEN_BY_TYPE: cpen = connection->type; break;
                case DPEN_BY_FILE: cpen = connection->file; break;
                case DPEN_BY_SRES: cpen = get_range_pen( fabs(connection->sres) *sresmult ); break;
                case DPEN_BY_RFAC: cpen = get_range_pen( connection->rfac ); break;
                default:           cpen = connection->cclass[data_pen_type-1]; break;
                }
                cpen = data_pen(cpen);
                if( !pen_selected(cpen)) continue;
                if( !pen_visible( cpen ) ) continue;

                /* Merge this into the list of pens */

                if( show_oneway_obs )
                {
                    cflags = connection->flags & (CONN_FORWARD | CONN_REVERSE );
                }
                else
                {
                    cflags = CONN_FORWARD | CONN_REVERSE;
                }

                for( j = npendef; j--; )
                {
                    if( pens[j].pen < cpen ) break;
                    if( pens[j].pen == cpen &&
                            pens[j].flags < cflags ) break;
                    pens[j+1] = pens[j];
                }
                j++;
                pens[j].pen = cpen;
                pens[j].flags = cflags;
                pens[j].highlight = 0;
                switch( hlttype )
                {
                case PCONN_HIGHLIGHT_IF_EITHER:
                case PCONN_HIGHLIGHT_IF_BOTH:
                    pens[j].highlight = highlight_line;
                    break;
                case PCONN_HIGHLIGHT_SRES:
                    if( fabs(connection->sres) > highlight_threshhold )
                        pens[j].highlight = 1;
                    break;
                case PCONN_HIGHLIGHT_APOST_SRES:
                    if( fabs(connection->sres) > highlight_threshhold*seu )
                        pens[j].highlight = 1;
                    break;
                case PCONN_HIGHLIGHT_RFAC:
                    if( fabs(connection->rfac) < highlight_threshhold )
                        pens[j].highlight = 1;
                    break;
                case PCONN_HIGHLIGHT_REJECTED:
                    if( connection->flags & CONN_REJECTED )
                        pens[j].highlight = 1;
                    break;
                case PCONN_HIGHLIGHT_UNUSED:
                    if(  (connection->flags & CONN_UNUSED) &&
                            !(connection->flags & CONN_REJECTED ) )
                        pens[j].highlight = 1;
                    break;
                }
                if( pens[j].highlight ) nhlt++;
                npendef++;
            }

            if( !npendef ) continue;
            if( highlight && !nhlt ) continue;

            /* Merge common observations */
            if( offset_opt != PCONN_ALL_CONNECTIONS )
            {
                j = 0;
                for( i=1; i<npendef; i++ )
                {
                    if( offset_opt == PCONN_ONE_CONNECTION || pens[i].pen == pens[j].pen )
                    {
                        pens[j].flags |= pens[i].flags;
                        pens[j].highlight |= pens[i].highlight;
                        if( redraw && pens[i].pen == redraw ) pens[j].pen = redraw;
                    }
                    else
                    {
                        pens[++j] = pens[i];
                    }
                }
                npendef = j+1;
            }


            /* Now actually plot the line(s) */

            if( npendef > max_pens ) max_pens = npendef;
            offset_prp = -(offset * (npendef-1))/2;

            if( station_in_view(from) ) flag_station_visible( from );
            if( station_in_view(tp->to) ) flag_station_visible( tp->to );

            while( npendef-- )
            {
                if( !redraw
                        || (pens[npendef].pen == redraw )
                        || (highlight && pens[npendef].highlight) )
                {
                    plot_connection( plotter, from, tp->to,
                                     pens[npendef].highlight ? hltpen : pens[npendef].pen,
                                     pens[npendef].flags, 0.0, offset_prp );
                }
                offset_prp += offset;
            }

            tp->conn->visible = 1;

            /* Plot relative ellipses here */

            if( redraw || (!pltrelerr && !pltrelhgt) ) continue;
            get_relative_covariance( tp->conn, &cvr );

            {
                double e1, n1, e2, n2;
                get_station_coordinates( from, &e1, &n1 );
                get_station_coordinates( tp->to, &e2, &n2 );
                mide = (e1 + e2)/2.0;
                midn = (n1 + n2)/2.0;
            }
            if( mide < plot_emin || mide > plot_emax ) continue;
            if( midn < plot_nmin || midn > plot_nmax ) continue;

            if( pltrelerr && cvr.emax > 0.0 )
            {
                double a, b;
                a = cvr.emax * eemult;
                b = cvr.emin * eemult;

                ELLIPSE( plotter, mide, midn, a, b, PI/2.0 - cvr.az, ellpen );
            }

            if( pltrelhgt && cvr.sehgt > 0.0 )
            {

                double he = cvr.sehgt  * hgterr_factor * rel_hgterr_scale;

                if( he > 0 )
                {
                    double s1 = stn_symbol_size/2;
                    LINE(plotter, mide-s1, midn+he, hgtpen);
                    LINE(plotter, mide+s1, midn+he, CONTINUE_LINE );
                    LINE(plotter, mide, midn+he, hgtpen);
                    LINE(plotter, mide, midn-he, CONTINUE_LINE );
                    LINE(plotter, mide-s1, midn-he, hgtpen);
                    LINE(plotter, mide+s1, midn-he, CONTINUE_LINE );
                }
            }

        }
    }

    hlt_offset = ((max_pens+2)*offset)/2;

    return ALL_DONE;
}

double get_obs_highlight_offset()
{
    return hlt_offset;
}

/*****************************************************************/
/* Create and maintain a sorted index of the standardised        */
/* residuals                                                     */

struct SresDef
{
    int from;
    int to_id;
    int obs_id;
    char unused;

    /// Recomputes the sort key - derived from this record's own
    /// from/to_id/obs_id - for the given display field.
    void set_sort_key( int order );

    /// Compares this record's sort key to another's, returning a
    /// strcmp-style result (negative/zero/positive). Both records must
    /// have been given a sort key for the same display field.
    int compare( const SresDef &other ) const;

    /// Compares two records by instrument station, using this record's
    /// sort key as a station code. Ties are broken by each record's
    /// target station, looked up via connlst using its own from/to_id.
    int compare_from( const SresDef &other ) const;

    /// Compares two records by the two stations they connect, treating a
    /// connection and its reverse as the same line. Each record's own two
    /// endpoints are its sort key (as a station code) and its target
    /// station, looked up via connlst using its own from/to_id.
    int compare_line( const SresDef &other ) const;

private:
    std::variant<std::string, double, long> cmpval;
};

#define MAX_DISPLAY_FIELDS 32

static std::vector<SresDef> srList;
static long *srIndex = NULL;
static long *srIndex2 = NULL;
static long srListCount = 0;
static long srIndexCount = 0;
static int srListMode = SRL_ALL;
static int srListOrder = SRF_SRES;
static int indexValid = 0;
static int displayFields[MAX_DISPLAY_FIELDS] = {SRF_FROM, SRF_TO, SRF_OBSID, SRF_DATE, SRF_TYPE, SRF_STATUS, SRF_SRES, SRF_RFAC};
static int displayFieldWidths[MAX_DISPLAY_FIELDS];
static int nDisplayFields = 8;

struct
{
    const char *name;
    int code;
    int width;
} displayFieldDefs[] =
{
    {"from",SRF_FROM,0},
    {"to",SRF_TO,0},
    {"data_type", SRF_TYPE,6},
    {"date", SRF_DATE,19},
    {"status", SRF_STATUS,10},
    {"std_residual", SRF_SRES,12},
    {"redundancy", SRF_RFAC, 12},
    {"data_file", SRF_FILE, 0 },
    {"line_number", SRF_LINENO, 8},
    {"line_length",SRF_LENGTH, 10},
    {"id",SRF_OBSID,8},
    {NULL,0,0}
};


int get_display_field_code( const std::string_view field )
{
    int i;
    int fieldCode = 0;
    for( i = 0; displayFieldDefs[i].name; i++ )
    {
        if( boost::algorithm::iequals( field, displayFieldDefs[i].name ) )
        {
            fieldCode = displayFieldDefs[i].code;
            break;
        }
    }
    if( fieldCode == 0 )
    {
        for( i = 0; i++ < obs_classes.count(); )
        {
            if( boost::algorithm::iequals( field, obs_classes.name(i)) )
            {
                fieldCode = i;
                break;
            }
        }
    }
    return fieldCode;
}

std::optional<std::string> get_display_field_name( int fieldCode )
{
    for( int j = 0; displayFieldDefs[j].name; j++ )
    {
        if( displayFieldDefs[j].code == fieldCode )
        {
            return displayFieldDefs[j].name;
        }
    }
    if( fieldCode > 0 )
    {
        return obs_classes.name(fieldCode);
    }
    return std::nullopt;
}


int read_display_fields_definition( const std::string_view def )
{
    constexpr std::string_view delimiters = " \t\n\r";
    FieldScanner scanner( def );
    nDisplayFields = 0;
    while( nDisplayFields < MAX_DISPLAY_FIELDS )
    {
        const auto fld = scanner.nextToken( delimiters );
        if( !fld ) break;
        const int fieldCode = get_display_field_code( *fld );
        if( fieldCode != 0 )
        {
            displayFields[nDisplayFields++] = fieldCode;
        }
    }
    return OK;
}


std::string write_display_fields_definition()
{
    std::string def;
    for( int i = 0; i < nDisplayFields; i++ )
    {
        const auto fieldName = get_display_field_name( displayFields[i] );
        if( fieldName )
        {
            if( !def.empty() ) def += ' ';
            def += *fieldName;
        }
    }
    return def;
}


static void set_display_field_widths( void )
{
    int i;
    for( i = 0; i < nDisplayFields; i++ )
    {
        int fldCode = displayFields[i];
        int width = 0;
        if( fldCode == SRF_FROM || fldCode == SRF_TO )
        {
            width = stn_name_width + 5;
        }
        else if( fldCode == SRF_FILE )
        {
            int j;
            for( j = 0; j < survey_data_file_count(); j++ )
            {
                std::string filnam = survey_data_file_name(j);
                int dfw;
                dfw = filnam.size();
                if( dfw > width ) width = dfw;
            }
            width  += 5;  /* Leave a bit of space */
        }

        else if ( fldCode > 0 )
        {
            int j;
            width = 7;  /* Default string for "Default" is 7 chrs */
            for( j = 0; j < obs_classes.value_count(fldCode); j++ )
            {
                std::string valnam = obs_classes.value_name( fldCode, j );
                int dfw = numeric_cast<int>(valnam.size());
                if( dfw > width ) width = dfw;
            }
            width += 5; /* Leave a bit of space! */
        }

        else
        {
            int j;
            for( j = 0; displayFieldDefs[j].name; j++ )
            {
                if( displayFieldDefs[j].code == fldCode )
                {
                    width = displayFieldDefs[j].width;
                    break;
                }
            }
        }
        displayFieldWidths[i] = width;
    }
}

void init_displayed_fields()
{
    if( ! have_obs_ids )
    {
        int nf;
        int nt=0;
        for( nf = 0; nf < nDisplayFields; nf++ )
        {
            if( displayFields[nf] != SRF_OBSID )
            {
                displayFields[nt++] = displayFields[nf];
            }
        }
        nDisplayFields=nt;
    }
}

void set_displayed_fields( int *fields, int nFields )
{
    for( nDisplayFields = 0;
            nDisplayFields < nFields && nDisplayFields < MAX_DISPLAY_FIELDS;
            nDisplayFields++ )
    {
        displayFields[nDisplayFields] = fields[nDisplayFields];
    }
    set_display_field_widths();
}


int get_displayed_fields( int *fields, int maxFields )
{
    int i;
    for( i = 0; i < nDisplayFields && i < maxFields ; i++ )
    {
        fields[i] = displayFields[i];
    }
    return nDisplayFields;
}
/* Note: all sorts include comparison of srIndex2 values to retain original sort order
   where new is no different */

int SresDef::compare( const SresDef &other ) const
{
    if( std::holds_alternative<double>(cmpval) )
    {
        double diff = std::get<double>(cmpval) - std::get<double>(other.cmpval);
        return diff < 0.0 ? -1 : diff > 0.0 ? 1 : 0;
    }
    if( std::holds_alternative<long>(cmpval) )
    {
        long diff = std::get<long>(cmpval) - std::get<long>(other.cmpval);
        return diff < 0 ? -1 : diff > 0 ? 1 : 0;
    }
    return stncodecmp( std::get<std::string>(cmpval), std::get<std::string>(other.cmpval) );
}

int SresDef::compare_from( const SresDef &other ) const
{
    int cmp = compare( other );
    if( cmp != 0 ) return cmp;
    int to1 = connlst[from].to[to_id].to;
    int to2 = connlst[other.from].to[other.to_id].to;
    if( to1 == to2 ) return 0;
    if( to1 == 0 || to2 == 0 ) return to1 - to2;
    return stncodecmp( stnptr(to1)->Code, stnptr(to2)->Code );
}

int SresDef::compare_line( const SresDef &other ) const
{
    std::string_view f1 = std::get<std::string>(cmpval);
    std::string_view f2 = std::get<std::string>(other.cmpval);
    int to1 = connlst[from].to[to_id].to;
    int to2 = connlst[other.from].to[other.to_id].to;
    std::string_view t1 = to1 ? std::string_view( stnptr(to1)->Code ) : std::string_view();
    std::string_view t2 = to2 ? std::string_view( stnptr(to2)->Code ) : std::string_view();

    if( stncodecmp(f1,t1) > 0 ) std::swap( t1, f1 );
    if( stncodecmp(f2,t2) > 0 ) std::swap( t2, f2 );

    int cmp = stncodecmp( f1, f2 );
    if( cmp == 0 ) cmp = stncodecmp(t1,t2);
    return cmp;
}

static int cmp_srdef_generic( const void *p1, const void *p2 )
{
    long i1 = * (long *) p1;
    long i2 = * (long *) p2;
    int cmp = srList[i1].compare(srList[i2]);
    if( cmp == 0 ) cmp = srIndex2[i1] - srIndex2[i2];
    return cmp;
}

static int cmp_srdef_reversefloat( const void *p1, const void *p2 )
{
    long i1 = * (long *) p1;
    long i2 = * (long *) p2;
    int cmp = -srList[i1].compare(srList[i2]);
    if( cmp == 0 ) cmp = srIndex2[i1] - srIndex2[i2];
    return cmp;
}

/* Specialised sort orders */

/* Sort based on instrument station */

static int cmp_srdef_from( const void *p1, const void *p2 )
{
    long i1 = * (long *) p1;
    long i2 = * (long *) p2;
    int cmp = srList[i1].compare_from(srList[i2]);
    if( cmp == 0 ) cmp = srIndex2[i1] - srIndex2[i2];
    return cmp;
}

/* Sort based on line .. brings together lines with same endpoints */

static int cmp_srdef_line( const void *p1, const void *p2 )
{
    long i1 = * (long *) p1;
    long i2 = * (long *) p2;
    int cmp = srList[i1].compare_line(srList[i2]);
    if( cmp == 0 ) cmp = srIndex2[i1] - srIndex2[i2];
    return cmp;
}

/* Sort based on file and line */

static int cmp_srdef_fileloc( const void *p1, const void *p2 )
{
    long i1 = * (long *) p1;
    long i2 = * (long *) p2;
    int cmp;

    get_connection_data_by_id( srList[i1].from, srList[i1].to_id, srList[i1].obs_id, connection );
    get_connection_data_by_id( srList[i2].from, srList[i2].to_id, srList[i2].obs_id, connection2 );

    cmp = survey_data_file_name( connection->file ).compare(survey_data_file_name( connection2->file ));
    if( cmp == 0 ) { cmp = connection->line - connection2->line; }
    if( cmp == 0 ) { cmp = srIndex2[i1] - srIndex2[i2]; }

    return cmp;
}

static void create_sres_index( void )
{
    int from;
    if( ! srList.empty() ) return;
    if( !ndata ) return;
    if( !connlst ) return;
    srList.resize( ndata );
    srIndex = (long *) check_malloc( ndata * sizeof(long) );
    srIndex2 = (long *) check_malloc( ndata * sizeof(long) );
    srListCount = 0;
    srIndexCount = 0;
    for( from = 1; from <= number_of_stations(net); from++ )
    {
        fconn_ptr *fp;
        int to_id;
        fp = &connlst[from];
        for( to_id = 0; to_id < fp->nconn; to_id++ )
        {
            tconn_ptr *tp;
            int obs_id;
            int first_conn;
            tp = &fp->to[to_id];
            for( first_conn = 1, obs_id = 0;
                    get_connection_data( from, tp, connection, first_conn);
                    first_conn = 0, obs_id++ )
            {
                SresDef *sr;
                // assert( srListCount < ndata );
                if( srListCount >= ndata ) break;
                if( connection->flags & CONN_OBS_REVERSE ) continue;
                sr = &srList[srListCount];
                srListCount++;
                sr->from = from;
                sr->to_id = to_id;
                sr->obs_id = obs_id;
                sr->unused = connection->flags & (CONN_UNUSED | CONN_REJECTED);
            }
        }
    }
}

static void SetupSresIndex( void )
{
    long idata;
    int oldIndexCount = srIndexCount;
    int (*cmp_func)( const void *p1, const void *p2 );

    if( indexValid ) return;
    if( srList.empty() ) create_sres_index();
    if( srList.empty() ) return;
    srIndexCount = 0;

    /* Save the old index order to make the sort preserve order */
    for( idata = 0; idata < srListCount; idata++ )
    {
        srIndex2[idata] = srListCount;
    }
    for( idata = 0; idata < oldIndexCount; idata++ )
    {
        srIndex2[srIndex[idata]] = idata;
    }

    /* Setup the index with the values to compare.  At this point
       filter the list to get only the required observations */

    for( idata = 0; idata < srListCount; idata++ )
    {
        SresDef *sr = &srList[idata];
        int valid = 1;
        switch( srListMode )
        {
        case SRL_REJECTED: if( !(sr->unused & CONN_REJECTED)) valid = 0; break;
        case SRL_UNUSED:   if( !sr->unused ) valid = 0; break;
        case SRL_USED:     if( sr->unused ) valid = 0; break;
        }
        if( valid )
        {
            srIndex[srIndexCount++] = idata;
            sr->set_sort_key( srListOrder );
        }
    }

    /* Choose the comparison function */

    switch (srListOrder)
    {
    case SRF_FROM:    cmp_func = cmp_srdef_from; break;
    case SRF_TO:      cmp_func = cmp_srdef_line; break;
    case SRF_TYPE:    cmp_func = cmp_srdef_generic; break;
    case SRF_STATUS:  cmp_func = cmp_srdef_generic; break;
    case SRF_SRES:    cmp_func = cmp_srdef_reversefloat; break;
    case SRF_RFAC:    cmp_func = cmp_srdef_generic; break;
    case SRF_DATE:    cmp_func = cmp_srdef_generic; break;
    case SRF_OBSID:   cmp_func = cmp_srdef_generic; break;
    case SRF_FILE:
    case SRF_LINENO:  cmp_func = cmp_srdef_fileloc; break;
    case SRF_LENGTH:  cmp_func = cmp_srdef_generic; break;
    default:          cmp_func = cmp_srdef_generic; break;
    }

    qsort( srIndex, srIndexCount, sizeof(long), cmp_func );
    indexValid = 1;
    set_display_field_widths();
}

void SresDef::set_sort_key( int order )
{
    switch (order)
    {
    case SRF_FROM:
    case SRF_TO:      cmpval = std::string( stnptr(from)->Code );
        break;
    case SRF_TYPE:    get_connection_data_by_id( from, to_id, obs_id, connection );
        cmpval = std::string( datatype[connection->type].code );
        break;
    case SRF_STATUS:  get_connection_data_by_id( from, to_id, obs_id, connection );
        cmpval =   (connection->flags & CONN_REJECTED ) ? "reject" :
                   (connection->flags & CONN_UNUSED) ? "unused" : "";
        break;
    case SRF_SRES:    get_connection_data_by_id( from, to_id, obs_id, connection );
        cmpval = (double) connection->sres;
        break;
    case SRF_RFAC:    get_connection_data_by_id( from, to_id, obs_id, connection );
        cmpval = (double) connection->rfac;
        break;
    case SRF_OBSID:   get_connection_data_by_id( from, to_id, obs_id, connection );
        cmpval = (long) connection->id;
        break;
    case SRF_FILE:
    case SRF_LINENO:
        break;
    case SRF_DATE:
        get_connection_data_by_id( from, to_id, obs_id, connection );
        cmpval = connection->date;
        break;
    case SRF_LENGTH:
        {
            double value = 0.0;
            int to = connlst[from].to[to_id].to;
            if( to )
            {
                station *sfrom = stnptr(from);
                station *sto = stnptr(to);
                value = calc_distance( sfrom, 0.0, sto, 0.0, nullptr, nullptr );
                value *= ellipsoidal_distance_correction( sfrom, sto );
            }
            cmpval = value;
        }
        break;
    default:
        if( order > 0 )
        {
            get_connection_data_by_id( from, to_id, obs_id, connection );
            cmpval = obs_classes.value_name( order, connection->cclass[order-1] );
        }
        else
        {
            cmpval = "";
        }
        break;
    }
    // stncodecmp (used by compare_from/compare_line) already folds case
    // itself, and every other consumer of a string sort key needs a
    // case-insensitive ordering too - canonicalise once here rather than
    // on every comparison during the sort.
    if( std::holds_alternative<std::string>(cmpval) )
    {
        cmpval = boost::algorithm::to_upper_copy(std::get<std::string>(cmpval));
    }
}


static void reverse_index()
{
    long i0, i1;
    for( i0 = 0, i1=srIndexCount-1; i0 < i1; i0++, i1-- )
    {
        long tmp = srIndex[i0];
        srIndex[i0] = srIndex[i1];
        srIndex[i1] = tmp;
    }
}

void set_sres_sort_col( int col )
{
    if( col >= 0 && col < nDisplayFields )
    {
        set_sres_sort_option( displayFields[col] );
    }
}

void set_sres_sort_option( int option )
{
    if( option && option != srListOrder )
    {
        indexValid = 0;
        srListOrder = option;
    }
    else if( indexValid && option == srListOrder )
    {
        reverse_index();
    }
}

int get_sres_sort_option( void )
{
    return srListOrder;
}

int get_sres_display_option()
{
    return srListMode;
}

void set_sres_display_option( int mode )
{
    if( mode != srListMode ) indexValid = 0;
    srListMode = mode;
}

long sres_index_count( void )
{
    if( !indexValid ) SetupSresIndex();
    return srIndexCount;
}

std::string sres_list_header()
{
    std::string header;
    for( int i = 0; i < nDisplayFields; i++ )
    {
        std::string data;
        int number = 0;
        switch( displayFields[i] )
        {
        case SRF_FROM:	data = "From"; break;
        case SRF_TO:		data = "To"; break;
        case SRF_TYPE:    data = "Type"; break;
        case SRF_STATUS:  data =   "Status"; break;
        case SRF_SRES:    data = "Std.res"; number = 1; break;
        case SRF_RFAC:    data = "Redundancy"; number = 1; break;
        case SRF_DATE:    data = "Date/time"; number=1; break;
        case SRF_FILE:    data = "File"; break;
        case SRF_LINENO:  data = "Line no"; number = 1; break;
        case SRF_OBSID:   data = "Id"; number = 1; break;
        case SRF_LENGTH:	data = "Length"; number = 1; break;
        default:          if( displayFields[i] > 0 )
            {
                data = obs_classes.name( displayFields[i] );
            }
            else
            {
                data = "";
            }
            break;
        }
        const int datalen = std::max( numeric_cast<int>( data.size() ), displayFieldWidths[i] );
        if( i ) header += '\t';
        if( number ) header += ' ';
        header += data;
        header.append( numeric_cast<size_t>( datalen ) - data.size(), ' ' );
    }
    return header;
}

/// Returns the date as text, empty if it is undefined
static std::string pdate_as_string( const double date )
{
    return date == UNDEFINED_DATE ? std::string() : date_as_string(date,DateStringFormat::timeIfNotMidnight);
}

std::string sres_item_description( long id )
{
    SresDef *sr;
    station *sfrom;
    station *sto;
    tconn_ptr *tp;
    double value;
    float sres;
    if( !indexValid ) SetupSresIndex();
    if( !indexValid ) return std::string();
    if( id < 0 || id >= srIndexCount ) return std::string();
    id = srIndex[id];
    sr = &srList[id];
    sfrom = stnptr( sr->from );
    tp = &connlst[sr->from].to[sr->to_id];
    sto = tp->to ? stnptr( tp->to ) : nullptr;
    get_connection_data_by_id( sr->from, sr->to_id, sr->obs_id, connection );
    sres = connection->sres;
    if( aposteriori_errors && seu > 0.0 ) sres /= seu;
    std::string description;
    for( int i = 0; i < nDisplayFields; i++ )
    {
        std::string data;
        char number[32];
        switch( displayFields[i] )
        {
        case SRF_FROM:    data = sfrom->Code; break;
        case SRF_TO:      data = sto ? sto->Code.c_str() : ""; break;
        case SRF_TYPE:    data = datatype[connection->type].code; break;
        case SRF_STATUS:  data =   (connection->flags & CONN_REJECTED ) ? "reject" :
                                       (connection->flags & CONN_UNUSED) ? "unused" : "";
            break;
        case SRF_SRES:    if( sres < 0.0 ) { data = "-   "; }
            else { sprintf(number, "%6.2f", sres ); data = number; } break;
        case SRF_RFAC:    sprintf(number, "%6.3f", connection->rfac ); data = number; break;
        case SRF_FILE:    data = survey_data_file_name( connection->file ); break;
        case SRF_LINENO:  sprintf(number, "%d", (int) (connection->line));
            data = number;
            break;
        case SRF_OBSID:
            sprintf(number, "%d", (int) (connection->id));
            data = number;
            break;
        case SRF_DATE:
            data=pdate_as_string( connection->date );
            break;
        case SRF_LENGTH:  if( sto )
            {
                value = calc_distance( sfrom, 0.0, sto, 0.0, nullptr, nullptr );
                value *= ellipsoidal_distance_correction( sfrom, sto );
                sprintf(number,"%.2lf",value);
            }
            else
            {
                strcpy(number,"-");
            }
            data = number;
            break;
        default:          if( displayFields[i] > 0 )
            {
                data = obs_classes.value_name( displayFields[i],
                                         connection->cclass[displayFields[i]-1] );
            }
            else
            {
                data  = "";
            }
            break;
        }
        if( i > 0 ) description += '\t';
        description += data;
    }
    return description;
}

void sres_item_info( long id, PutTextInfo *jmp )
{
    SresDef *sr;
    tconn_ptr *tp;
    jmp->type = ptfNone;
    if( !indexValid ) SetupSresIndex();
    if( !indexValid ) return;
    if( id < 0 || id >= srIndexCount ) return;
    id = srIndex[id];
    sr = &srList[id];
    tp = &connlst[sr->from].to[sr->to_id];
    jmp->type = ptfObs;
    jmp->from = sr->from;
    jmp->to = tp->to;
    jmp->obs_id = sr->obs_id;
}


/******************************************************************/

void set_eastmost_conn( int from )
{
    fconn_ptr *fp;
    tconn_ptr *t;
    int i;
    double e, n, emost;
    if( !connlst ) return;
    fp = &connlst[from];
    get_station_coordinates( from, &emost, &n );
    fp->eastmost_conn = from;
    for( t = fp->to, i = fp->nconn; i--; t++ )
    {
        if( t->to )
        {
            get_station_coordinates( t->to, &e, &n );
            if( e < emost )
            {
                emost = e;
                fp->eastmost_conn = t->to;
            }
        }
    }
}

static double calc_nearest_point( double x1, double y1, double x2, double y2, double x, double y )
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    double tx = x - x1;
    double ty = y - y1;
    double ds2, ts2, dts;
    ds2 = dx * dx + dy * dy;
    ts2 = tx * tx + ty * ty;
    if( ds2 <= 0 )
    {
        return ts2;
    }
    dts = tx * dx + ty * dy;
    if( dts < 0.0 ) return ts2;
    if( dts < ds2 )
    {
        ts2 -= dts*dts/ds2;
        return ts2;
    }
    dx = x - x2;
    dy = y - y2;
    return dx*dx + dy*dy;
}


static long xyindex_used = -1;

int nearest_connection( double e, double n, double tol, int *from, int *to )
{
    double end_e = e + tol;
    int ixfrom;
    int ixlast;
    int i;
    long current_xyindex;
    *from = *to = 0;
    if( !connlst ) return 0;
    current_xyindex = get_xyindex_version();
    if( current_xyindex != xyindex_used )
    {
        for( i = 1; i <= number_of_stations(net) ; i++ )
        {
            connlst[i].eastmost_conn = -1;
        }
        xyindex_used = current_xyindex;
    }
    ixfrom = first_station_past_x( e - tol );
    ixlast = used_station_count();
    tol *= tol;
    for ( ; ixfrom < ixlast; ixfrom++ )
    {
        int f, it;
        fconn_ptr *fp;
        tconn_ptr *tp;
        double e1, n1;
        double e2, n2;
        char from_visible;
        f = sorted_x_station_number( ixfrom );
        if( ! obs_station_showable(f) ) continue;
        from_visible = station_in_view( f );
        fp = &connlst[f];
        get_station_coordinates( f, &e1, &n1 );
        if( fp->eastmost_conn < 0 ) set_eastmost_conn( f );
        get_station_coordinates( fp->eastmost_conn, &e2, &n2 );
        if( e2 > end_e ) continue;  /* Can't get close enough */
        for( it = fp->nconn, tp = fp->to; it--; tp++ )
        {
            double nearest;
            if( !tp->to ) continue;
            if( ! obs_station_showable( tp->to ) ) continue;
            if( ! from_visible && ! station_in_view( tp->to ) ) continue;
            if( ! tp->conn->visible ) continue;
            get_station_coordinates(  tp->to, &e2, &n2 );
            if( e2 > e1 ) continue; /* We'll get it in the reverse direction */
            nearest = calc_nearest_point( e1, n1, e2, n2, e, n );
            if( nearest < tol )
            {
                tol = nearest;
                *from = f;
                *to = tp->to;
            }
        }
    }
    if( *from > *to ) { ixfrom = *from; *from = *to; *to = ixfrom; }
    return *from ? 1 : 0;
}

/**********************************************************************/

int get_connection_count( int istn )
{
    return connlst ? connlst[istn].nconn : 0;
}

int connection_observation_count( int from, int index )
{
    tconn_ptr *tp;
    fconn_ptr *fp;
    conn_ptr *p;
    if( !connlst ) return 0;
    fp = &connlst[from];
    if( index < 0 || index >= fp->nconn ) return 0;
    tp = &fp->to[index];
    p = tp->conn;
    return p ? p->nconn : 0;
}

int get_connected_station( int from, int index, char *visible )
{
    tconn_ptr *tp;
    fconn_ptr *fp;
    int to;

    if( !connlst ) return -1;
    fp = &connlst[from];
    if( index < 0 || index >= fp->nconn ) return -1;
    tp = &fp->to[index];
    to = tp->to;
    if( visible )
    {
        *visible = 0;
        if( to && obs_station_showable(from) && obs_station_showable(to) &&
                (station_in_view(from) || station_in_view(to)) &&
                tp->conn->visible ) *visible = 1;
    }
    return to;
}

/**********************************************************************/

struct ConnData
{
    int nto;
    double dist;
    double azimuth;
};

static const DmsFormat angleFormat( 3, 1 );
static const DmsFormat signedAngleFormat( 3, 1, 1, " ", " ", "", " ", "-" );
static const DmsFormat latitudeObservationFormat( 3, 1, 1, " ", " ", "", "N", "S" );
static const DmsFormat longitudeObservationFormat( 3, 1, 1, " ", " ", "", "E", "W" );

void list_connections( void *dest, PutTextFunc f, int from )
{
    fconn_ptr *fp;
    tconn_ptr *tp;
    station *fs, *ts;
    int ic, id;
    int nconn;
    int first_conn;
    int pntdata_id;
    PutTextInfo jump;
    if( !connlst || max_conn < 1 ) return;

    jump.type = ptfNone;

    (*f)( dest, &jump, pad_right( "To", 10 ) + " " + pad_left( "Length", 8 ) + " " + pad_left( "Azimuth", 7 ) + "  Observations" );

    fp = &connlst[from];
    std::vector<double> connAz( fp->nconn );
    std::vector<int> connId( fp->nconn );
    nconn=0;
    fs = stnptr( from );
    pntdata_id = -1;
    for( id = 0; id < fp->nconn; id++ )
    {
        tp = &fp->to[id];
        if( !tp->to ) pntdata_id = id;
        if( tp->to )
        {
            double az;
            ts = stnptr(tp->to);
            az = calc_azimuth( fs, 0.0, ts, 0.0, 0, NULL, NULL );
            az *= RTOD;
            while( az > 360.0 ) az -= 360.0;
            while( az < 0.0 ) az += 360.0;
            connAz[id] = az;
            for( ic = nconn; ic-- > 0;  )
            {
                if( az > connAz[connId[ic]] ) break;
                connId[ic+1] = connId[ic];
            }
            connId[ic+1] = id;
            nconn++;
        }
    }

    jump.type = ptfLine;
    jump.from = from;
    for( ic = 0; ic < nconn; ic++ )
    {
        int i;
        int to;
        double d;
        int count[NOBSTYPE][2];
        id = connId[ic];
        tp = &fp->to[id];
        to = tp->to;
        ts = stnptr( to );
        jump.to = to;
        d = calc_distance( fs, 0.0, ts, 0.0, nullptr, nullptr );
        std::string line = pad_right( ts->Code, 10 ) + " " + format_fixed( d, 1, 7 ) + "m N" + format_fixed( connAz[id], 1, 5, '0' ) + "E  ";
        for( i = 0; i < NOBSTYPE; i++ ) count[i][0] = count[i][1] = 0;
        for( first_conn = 1;
                get_connection_data( from, tp, connection, first_conn);
                first_conn = 0 )
        {
            int obstype = connection->type;
            int reverse = connection->flags & CONN_OBS_REVERSE ? 1 : 0;
            count[obstype][reverse]++;
        }
        if( first_conn ) continue;  /* No connections - covariance only */
        for( i = 0; i < NOBSTYPE; i++ )
        {
            if( datatype[i].isdirectional )
            {
                if( count[i][0] )
                {
                    line += " >" + std::string( datatype[i].code ) + "(" + std::to_string( count[i][0] ) + ")";
                }
                if( count[i][1] )
                {
                    line += " <" + std::string( datatype[i].code ) + "(" + std::to_string( count[i][1] ) + ")";
                }
            }
            else
            {
                int total = count[i][0] + count[i][1];
                if( total )
                {
                    line += " " + std::string( datatype[i].code ) + "(" + std::to_string( total ) + ")";
                }
            }
        }
        (*f)( dest, &jump, line );
    }

    if( pntdata_id >= 0 )
    {
        jump.type = ptfNone;
        (*f)( dest, &jump, "");
        tp = &fp->to[pntdata_id];
        jump.type = ptfObs;
        jump.from = from;
        jump.to = 0;

        for( first_conn = 1, jump.obs_id = 0;
                get_connection_data( from, tp, connection, first_conn);
                first_conn = 0, jump.obs_id++ )
        {

            const int obstype = connection->type;

            std::string line = pad_right( std::string( datatype[obstype].name ), 22 );

            if( binary_data )
            {
                float sres;
                sres = connection->sres;
                if( aposteriori_errors ) sres /= seu;
                line += "  S.R. " + format_fixed( sres, 3, 7 );
                if( connection->flags & CONN_REJECTED )
                {
                    line += " " + pad_right( "Rejected", 11 );
                }
                else if( connection->flags & CONN_UNUSED )
                {
                    line += " " + pad_right( "Not used", 11 );
                }
                else
                {
                    line += " Rdncy " + format_fixed( connection->rfac, 3, 5 );
                }
            }
            line += "   Line " + pad_left( std::to_string( connection->line ), 2 ) + ": "
                    + survey_data_file_name( connection->file ).substr( 0, MAX_FILENAME_LEN );
            (*f)( dest, &jump, line );
        }
    }
}

static void list_line_statistics( void *dest, PutTextFunc f, int from, int to )
{
    PutTextInfo jmp;
    station *sfrom, *sto;
    double dist, edist;
    double az;
    double hd;

    sfrom = stnptr( from );
    sto = stnptr( to );

    jmp.type = ptfStation;
    jmp.from = from;

    (*f)( dest, &jmp, "From " + std::string( sfrom->Code ) + ": " + sfrom->Name.substr( 0, 50 ) );

    jmp.from = to;
    (*f)( dest, &jmp, "To   " + std::string( sto->Code ) + ": " + sto->Name.substr( 0, 50 ) );

    jmp.type = ptfNone;
    (*f)( dest, &jmp, "" );

    /* Print basic information - distance, azimuth, height diff */

    dist = calc_distance( sfrom, 0.0, sto, 0.0, NULL, NULL );
    az = calc_azimuth( sfrom, 0.0, sto, 0.0, 0, NULL, NULL )*RTOD;
    while( az > 360.0 ) az -= 360.0;
    while( az < 0.0 ) az += 360.0;
    hd = calc_hgt_diff( sfrom, 0.0, sto, 0.0, NULL, NULL );
    edist = calc_ellipsoidal_distance( sfrom, sto, NULL, NULL );

    jmp.type = ptfNone;
    (*f)( dest, &jmp, "Slope dist " + format_fixed( dist, 3 ) + "  Ell dist " + format_fixed( edist, 3 )
                      + "   Az " + dms_string( az, angleFormat ) + "  Hgt diff " + format_fixed( hd, 3 ) );
    if( !geodetic_coordsys() )
    {
        double x1, y1;
        double x2, y2;
        get_projection_coordinates( from, &x1, &y1 );
        get_projection_coordinates( to, &x2, &y2 );
        if( x1 != x2 && y1 != y2 )
        {
            dist = _hypot( x2 - x1, y2 - y1 );
            az = atan2( x2-x1, y2-y1 ) * RTOD;
            while( az > 360.0 ) az -= 360.0;
            while( az < 0.0 ) az += 360.0;
            (*f)( dest, &jmp, "Projection distance " + format_fixed( dist, 3 ) + "  azimuth " + dms_string( az, angleFormat ) );
        }
    }
    (*f)( dest, &jmp, "" );
}


void list_observations( void *dest, PutTextFunc f, int from, int to )
{
    fconn_ptr *fp;
    tconn_ptr *tp;
    int i;
    PutTextInfo jump;
    conn_cvr cvr;
    int first_conn;

    list_line_statistics( dest, f, from, to );

    if( !connlst ) return;
    fp = &connlst[from];
    tp = 0;
    for( i = 0; i < fp->nconn; i++ )
    {
        if( fp->to[i].to  == to ) { tp = &fp->to[i]; break;}
    }
    if( !tp ) return;

    /* Print relative covariance information */

    jump.type = ptfNone;

    get_relative_covariance( tp->conn, &cvr );

    if( binary_data )
    {
        std::string prefix = aposteriori_errors ? "Aposteriori " : "Apriori ";
        if( use_confidence_limit )
        {
            prefix += format_fixed( confidence_limit, 2 ) + "% conf. lim. ";
        }
        else if( confidence_limit != 1.0 )
        {
            prefix += format_fixed( confidence_limit, 1 ) + " times ";
        }

        if( cvr.emax > 0.0 && dimension != 1 )
        {
            double b1, b2, emax, emin;
            b1 = cvr.az * RTOD;
            while( b1 < 0.0 ) b1 += 180.0;
            while( b1 > 180.0 ) b1 -= 180.0;
            b2 = b1 + 90.0;
            if( b2 > 180.0 ) b2 -= 180.0;

            emax = cvr.emax * errell_factor * 1000.0;
            emin = cvr.emin * errell_factor * 1000.0;

            (*f)( dest, &jump, prefix + "error ellipse " + format_fixed( emax, 1 ) + "mm at N" + format_fixed( b1, 0 )
                               + "E, " + format_fixed( emin, 1 ) + "mm at N" + format_fixed( b2, 0 ) + "E" );
        }

        if( cvr.sehgt && dimension != 2 )
        {
            double hgterr;
            hgterr = cvr.sehgt * hgterr_factor * 1000.0;
            (*f)( dest, &jump, prefix + "height error " + format_fixed( hgterr, 1 ) + "mm" );
        }
    }

    /* Plot a list of connections - this could be expanded to show
       a bit more about the observation...  */



    jump.type = ptfObs;
    jump.from = from;
    jump.to = to;
    for( first_conn = 1, jump.obs_id = 0;
            get_connection_data( from, tp, connection, first_conn);
            first_conn = 0, jump.obs_id++ )
    {

        const int obstype = connection->type;

        const bool reverse = datatype[obstype].isdirectional &&
                             connection->flags & CONN_OBS_REVERSE;

        std::string line = std::string( datatype[obstype].name ) + " " + pad_left( reverse ? "(rvs)" : "", 5 );
        line[0] = static_cast<char>( TOUPPER( line[0] ) );
        line = pad_right( line, 26 );

        if( binary_data )
        {
            float sres;
            sres = connection->sres;
            if( aposteriori_errors ) sres /= seu;
            line += "  S.R. " + format_fixed( sres, 3, 7 );
            if( connection->flags & CONN_REJECTED )
            {
                line += " " + pad_right( "Rejected", 11 );
            }
            else if( connection->flags & CONN_UNUSED )
            {
                line += " " + pad_right( "Not used", 11 );
            }
            else
            {
                line += " Rdncy " + format_fixed( connection->rfac, 3, 5 );
            }
        }
        line += pad_left( pdate_as_string( connection->date ), 21 );
        line += "   Line " + pad_left( std::to_string( connection->line ), 2 ) + ": "
                + survey_data_file_name( connection->file ).substr( 0, MAX_FILENAME_LEN );
        (*f)( dest, &jump, line );
    }
}

static double degree_angle( double rad )
{
    double angle;
    angle = rad * RTOD;
    while( angle > 360.0 ) angle -= 360.0;
    while( angle < 0.0 ) angle += 360.0;
    return angle;
}

void list_obsdata( void *dest, PutTextFunc f, survdata *sd, int64_t binloc, int index )
{
    PutTextInfo jmp;
    obsdata *o;
    station *sfrom, *sto;
    double semult, srmult;
    int type;


    o = & sd->obs.odata[index];
    semult = aposteriori_errors ? seu : 1.0;
    srmult = (semult > 0.0) ? 1.0/semult : 0.0;

    sfrom = stnptr( sd->from );
    sto = stnptr( o->tgt.to );

    jmp.type = ptfStation;
    jmp.from = sd->from;

    (*f)( dest, &jmp, "From:  " + pad_right( sfrom->Code, 10 ) + "  " + sfrom->Name.substr( 0, 50 ) );

    jmp.from = o->tgt.to;
    (*f)( dest, &jmp, "To:    " + pad_right( sto->Code, 10 ) + "  " + sto->Name.substr( 0, 50 ) );
    jmp.type = ptfNone;
    (*f)( dest, &jmp, "" );
    if( have_obs_ids )
    {
        (*f)( dest, &jmp, "Id: " + std::to_string( o->tgt.id ) );
    }

    if( sd->date != UNDEFINED_DATE )
    {
        (*f)( dest, &jmp, "Date/time:  " + pdate_as_string( sd->date ) );
    }
    (*f)( dest, &jmp, "Source: Line " + std::to_string( o->tgt.lineno ) + ",  " + survey_data_file_name( sd->file ) );
    if( o->tgt.noteloc ) display_note_text( dest, f, o->tgt.noteloc );

    if( o->tgt.nclass )
    {
        int n;
        classdata *c;
        for( n = o->tgt.nclass, c = sd->clsf + o->tgt.iclass; n--; c++ )
        {
            (*f)(dest, &jmp, obs_classes.name( c->class_id ) + ": " + obs_classes.value_name( c->class_id, c->name_id ) );
        }
    }
    (*f)(dest, &jmp, "");

    type = o->tgt.type;
    std::string typeText = "Observation type: " + std::string( datatype[type].name );
    if(  o->tgt.unused & REJECT_OBS_BIT ) typeText += "  (rejected)";
    else if(  o->tgt.unused ) typeText += "  (not used)";
    (*f)( dest, &jmp, typeText );
    (*f)( dest, &jmp, "H.I.  " + format_fixed( sd->fromhgt, 3 ) + "m       H.T.  " + format_fixed( o->tgt.tohgt, 3 ) + "m" );
    if( datatype[type].isangle )
    {
        (*f)( dest, &jmp, "Observed value:  " + dms_string( degree_angle( o->value ), signedAngleFormat )
                          + "  +/-  " + format_fixed( o->error*semult*RTOS, 1, 6 ) );
    }
    else
    {
        (*f)( dest, &jmp, "Observed value:  " + format_fixed( o->value, 4, 12 ) + "  +/-  " + format_fixed( o->error*semult, 4, 6 ) );
    }

    if( binary_data )
    {
        if( datatype[type].isangle )
        {
            double obslength, altres;
            (*f)( dest, &jmp, "Calculated:      " + dms_string( degree_angle( o->calc ), signedAngleFormat )
                              + "  +/-  " + format_fixed( o->calcerr*semult*RTOS, 1, 6 ) );
            std::string residualText = "Residual:        " + format_fixed( o->residual*RTOS, 1, 12 )
                                       + "  +/-  " + format_fixed( o->reserr*semult*RTOS, 1, 6 );

            switch( type )
            {
            case AZ:
            case ZD:
                obslength = calc_distance( stnptr(sd->from), 0.0, stnptr(o->tgt.to), 0.0,nullptr,nullptr);
                altres = o->residual*obslength;
                if( type == ZD ) altres *= sin( o->value );
                residualText += "     (" + format_fixed( altres, 3 ) + "m offset)";
                break;
            case HA:
            case PB:
                obslength = calc_ellipsoidal_distance( stnptr(sd->from), stnptr(o->tgt.to),nullptr,nullptr);
                altres = o->residual*obslength;
                if( type == ZD ) altres *= sin( o->value );
                residualText += "     (" + format_fixed( altres, 3 ) + "m offset)";
                break;
            }
            (*f)( dest, &jmp, residualText );
        }
        else
        {
            double obslength;
            (*f)( dest, &jmp, "Calculated:      " + format_fixed( o->calc, 4, 12 ) + "  +/-  " + format_fixed( o->calcerr*semult, 4, 6 ) );
            std::string residualText = "Residual:        " + format_fixed( o->residual, 4, 12 )
                                       + "  +/-  " + format_fixed( o->reserr*semult, 4, 6 );

            switch( type )
            {
            case SD:
            case ED:
            case MD:
            case HD:
            case DR:
                if( o->value > 0.0 )
                {
                    residualText += "    (" + format_fixed( 1.0e6*o->residual/o->value, 2 ) + "ppm)";
                }
                break;
            case LV:
                obslength = calc_distance( stnptr(sd->from), 0.0, stnptr(o->tgt.to), 0.0,nullptr,nullptr);
                if( obslength > 0 )
                {
                    residualText += "   (" + format_fixed( 1.0e6*o->residual/obslength, 2 ) + "ppm of distance between stations)";
                }
                break;
            }
            (*f)( dest, &jmp, residualText );
        }
        (*f)( dest, &jmp, "Standardised Residual: " + pad_right( format_fixed( o->sres*srmult, 3 ), 10 ) );
    }

    std::string parameterText;
    if( type == ZD && o->refcoef)
    {
        parameterText = std::string( param_name( o->refcoef ) ) + " (" + format_fixed( param_value( o->refcoef ), 3 ) + ")";
    }
    else if( o->prm_id && (type == ED || type == MD || type == HD || type == SD || type == DR ))
    {
        parameterText = "   " + std::string( param_name( o->prm_id ) ) + "  (" + format_fixed( param_value( o->prm_id ), 1 ) + "ppm)";
    }
    else if( o->prm_id && (type == AZ || type == PB) )
    {
        parameterText = "   " + std::string( param_name( o->prm_id ) ) + "  (" + format_fixed( param_value( o->prm_id ), 1 ) + "sec)";
    }
    if( !parameterText.empty() ) (*f)( dest, &jmp, parameterText );

    if( datatype[type].joinsgroup )
    {
        int iobs;
        obsdata *o;
        (*f)(dest, &jmp, "" );
        (*f)(dest, &jmp, "Related observations from station " + std::string( stnptr(sd->from)->Code ) );
        (*f)(dest, &jmp, std::string( "     Target         Observed" ) + ( binary_data ? "   Residual    S.R." : "" ) );
        for( iobs=0, o=sd->obs.odata; iobs < sd->nobs; iobs++, o++ )
        {
            if( o->tgt.type != type ) continue;
            if( iobs == index )
            {
                jmp.type = ptfNone;
            }
            else
            {
                int obs_id;
                jmp.type = ptfObs;
                jmp.from = sd->from;
                jmp.to = o->tgt.to;
                obs_id = get_connection_to_id( jmp.from, jmp.to );
                if( obs_id >= 0 )
                    obs_id = get_connection_obs_id( jmp.from, obs_id, binloc, iobs );
                if( obs_id >= 0 )
                {
                    jmp.obs_id = obs_id;
                }
                else
                {
                    jmp.type = ptfLine;
                }
            }
            std::string line = "     " + pad_right( stnptr(o->tgt.to)->Code, 10 ) + "  ";
            if( datatype[type].isangle )
            {
                line += dms_string( o->value * RTOD, signedAngleFormat );
                if( binary_data )
                {
                    line += "  " + format_fixed( o->residual*RTOS, 1, 7 ) + "  " + format_fixed( o->sres*srmult, 2, 7 )
                            + ( o->tgt.unused ? "  (not used)" : "" );
                }
            }
            else
            {
                line += format_fixed( o->value, 4, 12 );
                if( binary_data )
                {
                    line += "  " + format_fixed( o->residual, 4, 7 ) + "  " + format_fixed( o->sres*srmult, 2, 7 )
                            + ( o->tgt.unused ? "  (not used)" : "" );
                }
            }
            (*f)(dest,&jmp,line);
        }
    }
}

void list_vecdata( void *dest, PutTextFunc f, survdata *sd, unsigned char flags,
                   int64_t bloc, int index )
{
    PutTextInfo jmp;
    vecdata *v;
    trgtdata *tgt;
    int ofrom, oto, from, to;
    station *sfrom, *sto;
    double semult, srmult;
    int type;
    char unused;
    int ndp;
    int ispoint;
    double oxyz[3], eoxyz[3], cxyz[3], ecxyz[3], renu[3], erenu[6],
           sres[3], rfac[3], small;
    double obslen, calclen, reslen, vsres;
    int axis, rank;
    small = 0.0;

    if( index >= sd->nobs )
    {
        ofrom = index % sd->nobs;
        oto = index / sd->nobs;
        tgt = get_trgtdata( sd, ofrom );
        unused = tgt->unused | (get_trgtdata(sd,oto))->unused;
    }
    else
    {
        ofrom = VD_REF_STN;
        oto = index;
        tgt = get_trgtdata( sd, index );
        unused = tgt->unused;
    }
    if( flags & CONN_OBS_REVERSE )
    {
        int tmp;
        tmp = ofrom; ofrom = oto; oto = tmp;
    }
    v = sd->obs.vdata;
    ispoint = datatype[v->tgt.type].ispoint;

    from = (ofrom == VD_REF_STN) ?  sd->from : v[ofrom].tgt.to;
    to   = (oto == VD_REF_STN)   ?  sd->from : v[oto].tgt.to;

    if( from == 0 ) { from = to; to = 0;}

    semult = aposteriori_errors ? seu : 1.0;
    srmult = (semult > 0.0) ? 1.0/semult : 0.0;

    sfrom = stnptr( from );

    jmp.type = ptfStation;
    jmp.from = from;

    (*f)( dest, &jmp, "From:  " + pad_right( sfrom->Code, 10 ) + "  " + sfrom->Name.substr( 0, 50 ) );

    if( to )
    {
        sto = stnptr( to );
        jmp.from = to;
        (*f)( dest, &jmp, "To:    " + pad_right( sto->Code, 10 ) + "  " + sto->Name.substr( 0, 50 ) );
    }

    jmp.type = ptfNone;
    (*f)( dest, &jmp, "" );
    if( have_obs_ids )
    {
        std::string idText = "Id:";
        if( ofrom != VD_REF_STN )
        {
            idText += " " + std::to_string( v[ofrom].tgt.id );
        }
        if( oto != VD_REF_STN )
        {
            idText += " " + std::to_string( v[oto].tgt.id );
        }
        (*f)( dest, &jmp, idText );
    }

    if( sd->date != UNDEFINED_DATE )
    {
        (*f)( dest, &jmp, "Date/time:  " + pdate_as_string( sd->date ) );
    }
    (*f)( dest, &jmp, "Source: Line " + std::to_string( tgt->lineno ) + ",  " + survey_data_file_name( sd->file ) );
    if( tgt->noteloc ) display_note_text( dest, f, tgt->noteloc );
    (*f)( dest, &jmp, "" );
    type = tgt->type;
    std::string typeText = "Observation type: " + std::string( datatype[type].name );
    if( index >= sd->nobs ) typeText += "  (calculated)";
    if( unused & REJECT_OBS_BIT ) typeText += "  (rejected)";
    else if( unused ) typeText += "  (not used)";
    (*f)( dest, &jmp, typeText );
    if( sd->reffrm ) (*f)( dest, &jmp, "Reference frame: " + rftrans_from_id(sd->reffrm)->name );
    std::string heightText;
    if( datatype[type].ispoint )
    {
        calc_vecdata_point( sd, oto, VD_OBSVEC | VD_STDERR, oxyz, eoxyz );
        if( binary_data )
        {
            calc_vecdata_point( sd, oto, VD_CALCVEC | VD_STDERR, cxyz, ecxyz );
            calc_vecdata_point( sd, oto, VD_RESVEC | VD_TOPOCENTRIC | VD_STDERR,
                                renu, erenu );
            calc_vecdata_point( sd, oto, VD_OBSVEC | VD_TOPOCENTRIC | VD_STDERR,
                                NULL, rfac );
            small = (rfac[0]+rfac[1]+rfac[2])*1.0e-3;
        }
        heightText = "H.I.  " + format_fixed( v[oto].tgt.tohgt, 3 ) + "m";
    }
    else
    {
        calc_vecdata_vector( sd, ofrom, oto,  VD_OBSVEC | VD_STDERR, oxyz, eoxyz );
        if( binary_data )
        {
            calc_vecdata_vector( sd, ofrom, oto,  VD_CALCVEC | VD_STDERR, cxyz, ecxyz );
            calc_vecdata_vector( sd, ofrom, oto,  VD_RESVEC | VD_TOPOCENTRIC | VD_STDERR,
                                 renu, erenu );
            calc_vecdata_vector( sd, ofrom, oto, VD_OBSVEC | VD_TOPOCENTRIC | VD_STDERR,
                                 NULL, rfac );
            small = (rfac[0]+rfac[1]+rfac[2])*1.0e-3;
        }
        heightText = "H.I.  " + format_fixed( (ofrom == VD_REF_STN) ? sd->fromhgt : v[ofrom].tgt.tohgt, 3 )
                     + "m       H.T.  " + format_fixed( (oto == VD_REF_STN) ? sd->fromhgt : v[oto].tgt.tohgt, 3 ) + "m";
    }
    (*f)(dest, &jmp, heightText );

    (*f)( dest, &jmp, std::string( "     Observed (XYZ)" )
                      + ( binary_data ? "     Calculated (XYZ)   Residual (ENU)    S.R  Redundancy" : "" ) );
    obslen = sqrt( oxyz[0]*oxyz[0] + oxyz[1]*oxyz[1] + oxyz[2]*oxyz[2] );
    ndp = (obslen < 999999.0) ? 4 : 3;
    for( axis = 0; axis < 3; axis++ )
    {
        std::string axisText = format_fixed( oxyz[axis], ndp, 12 ) + " " + format_fixed( eoxyz[axis]*semult, ndp, 7 );
        if( binary_data )
        {
            sres[axis] = erenu[axis] > small ? fabs(renu[axis]/erenu[axis]) : -1.0;
            if( !unused && rfac[axis] > small )
            {
                rfac[axis] = erenu[axis]/rfac[axis];
            }
            else
            {
                rfac[axis] = 1.0;
            }
            axisText += " " + format_fixed( cxyz[axis], ndp, 12 ) + " " + format_fixed( ecxyz[axis]*semult, ndp, 7 )
                        + " " + format_fixed( renu[axis], ndp, 8 ) + " " + format_fixed( erenu[axis]*semult, ndp, 7 )
                        + " " + format_fixed( sres[axis]*srmult, 2, 7 ) + " " + format_fixed( rfac[axis], 2, 7 );
        }
        (*f)(dest,&jmp,axisText);
    }
    std::string lengthText = format_fixed( obslen, ndp, 12 );
    if( binary_data )
    {
        calclen = sqrt(cxyz[0]*cxyz[0]+cxyz[1]*cxyz[1]+cxyz[2]*cxyz[2]);
        reslen = sqrt(renu[0]*renu[0]+renu[1]*renu[1]+renu[2]*renu[2]);
        calc_vecdata_vector( sd, ofrom, oto, VD_RESVEC, renu, erenu );
        vsres = vector_standardised_residual( renu, erenu, &rank );
        lengthText += "         " + format_fixed( calclen, ndp, 12 ) + "         " + format_fixed( reslen, ndp, 8 )
                      + "         " + format_fixed( vsres*srmult, 2, 7 );
    }
    (*f)( dest, &jmp, lengthText );

    if( tgt->nclass )
    {
        int n;
        classdata *c;
        (*f)(dest, &jmp, "Classifications");
        for( n = tgt->nclass, c = sd->clsf + tgt->iclass; n--; c++ )
        {
            (*f)(dest, &jmp, "     " + pad_right( obs_classes.name( c->class_id ), 15 ) + "  " + obs_classes.value_name( c->class_id, c->name_id ) );
        }
    }

    if( ispoint ) from = 0;
    if( sd->nobs > 1 )
    {
        int iobs;
        (*f)( dest, &jmp, "" );
        (*f)( dest, &jmp, std::string( "Other " ) + ( ispoint ? "points" : "vectors" ) + " in set referenced to station " + std::string( sfrom->Code ) );
        (*f)( dest, &jmp, std::string( "     To             Distance" )
                          + ( binary_data ? "          Residual (E,N,U)        Std.Res" : "" ) );
        for( iobs = ispoint ? 0 : -1; iobs < sd->nobs; iobs++ )
        {
            int otgt;
            int index;
            int to;
            int obs_id;
            otgt = iobs < 0 ? VD_REF_STN : iobs;
            if( otgt == ofrom || (ispoint && otgt == oto) ) continue;
            if( ispoint )
            {
                index = otgt;
                to = 0;
            }
            else if( ofrom == VD_REF_STN )
            {
                index = otgt;
            }
            else if( otgt == VD_REF_STN )
            {
                index = ofrom;
            }
            else if( ofrom < otgt )
            {
                index = ofrom + sd->nobs * otgt;
            }
            else
            {
                index = otgt + sd->nobs * ofrom;
            }
            to = (otgt == VD_REF_STN) ? sd->from : v[otgt].tgt.to;
            obs_id = get_connection_to_id( from, to );
            if( obs_id >= 0 ) obs_id = get_connection_obs_id( from ? from : to, obs_id, bloc, index );
            jmp.type = ptfObs;
            jmp.from = to;
            jmp.to = from;
            jmp.obs_id = obs_id;
            if( obs_id < 0 ) jmp.type = ptfNone;
            if( ispoint )
            {
                calc_vecdata_point( sd, otgt, VD_OBSVEC, oxyz, nullptr );
                obslen = sqrt( oxyz[0]*oxyz[0] + oxyz[1]*oxyz[1] + oxyz[2]*oxyz[2] );
                std::string line = "     " + pad_right( stnptr(to)->Code, 10 ) + "   " + format_fixed( obslen, 4, 12 );
                if( binary_data )
                {
                    calc_vecdata_point( sd, otgt, VD_RESVEC, renu, erenu );
                    vsres = vector_standardised_residual( renu, erenu, &rank );
                    calc_vecdata_point( sd,otgt, VD_RESVEC | VD_TOPOCENTRIC,
                                        renu, nullptr );
                    line += "   " + format_fixed( renu[0], 4, 8 ) + " " + format_fixed( renu[1], 4, 8 ) + " " + format_fixed( renu[2], 4, 8 )
                            + "   " + format_fixed( vsres*srmult, 2, 7 );
                }
                (*f)(dest,&jmp,line);
            }
            else
            {
                calc_vecdata_vector( sd, ofrom, otgt, VD_OBSVEC, oxyz, nullptr );
                obslen = sqrt( oxyz[0]*oxyz[0] + oxyz[1]*oxyz[1] + oxyz[2]*oxyz[2] );
                std::string line = "     " + pad_right( stnptr(to)->Code, 10 ) + "   " + format_fixed( obslen, 4, 12 );
                if( binary_data )
                {
                    calc_vecdata_vector( sd, ofrom, otgt, VD_RESVEC, renu, erenu );
                    vsres = vector_standardised_residual( renu, erenu, &rank );
                    calc_vecdata_vector( sd, ofrom, otgt, VD_RESVEC | VD_TOPOCENTRIC,
                                         renu, nullptr );
                    line += "   " + format_fixed( renu[0], 4, 8 ) + " " + format_fixed( renu[1], 4, 8 ) + " " + format_fixed( renu[2], 4, 8 )
                            + "   " + format_fixed( vsres*srmult, 2, 7 );
                }
                (*f)(dest,&jmp,line);
            }
        }
    }
}


void list_pntdata( void *dest, PutTextFunc f, survdata *sd, int index )
{
    PutTextInfo jmp;
    pntdata *p;
    station *sfrom;
    double semult, srmult;
    int type;

    p = & sd->obs.pdata[index];
    semult = aposteriori_errors ? seu : 1.0;
    srmult = (semult > 0.0) ? 1.0/semult : 0.0;

    sfrom = stnptr( sd->from );

    jmp.type = ptfStation;
    jmp.from = sd->from;

    (*f)( dest, &jmp, "Station:  " + pad_right( sfrom->Code, 10 ) + "  " + sfrom->Name.substr( 0, 50 ) );
    jmp.type = ptfNone;
    (*f)( dest, &jmp, "" );
    if( have_obs_ids )
    {
        (*f)( dest, &jmp, "Id: " + std::to_string( p->tgt.id ) );
    }
    if( sd->date != UNDEFINED_DATE )
    {
        (*f)( dest, &jmp, "Date/time:  " + pdate_as_string( sd->date ) );
    }
    (*f)( dest, &jmp, "Source: Line " + std::to_string( p->tgt.lineno ) + ",  " + survey_data_file_name( sd->file ) );
    if( p->tgt.noteloc ) display_note_text( dest, f, p->tgt.noteloc );
    (*f)( dest, &jmp, "" );
    type = p->tgt.type;
    std::string typeText = "Observation type: " + std::string( datatype[type].name );
    if(  p->tgt.unused & REJECT_OBS_BIT ) typeText += "  (rejected)";
    else if(  p->tgt.unused ) typeText += "  (not used)";
    (*f)( dest, &jmp, typeText );
    /* Only used for angle observations, which includes latitude and longitude */
    const DmsFormat &format = type == LT ? latitudeObservationFormat
                              : type == LN ? longitudeObservationFormat : signedAngleFormat;
    if( datatype[type].isangle )
    {
        (*f)( dest, &jmp, "Observed value:  " + dms_string( p->value * RTOD, format )
                          + "  +/-  " + format_fixed( p->error*semult*RTOS, 1, 6 ) );
    }
    else
    {
        (*f)( dest, &jmp, "Observed value:  " + format_fixed( p->value, 4, 12 ) + "  +/-  " + format_fixed( p->error*semult, 4, 6 ) );
    }

    if( binary_data )
    {
        if( datatype[type].isangle )
        {
            (*f)( dest, &jmp, "Calculated:      " + dms_string( p->calc * RTOD, format )
                              + "  +/-  " + format_fixed( p->calcerr*semult*RTOS, 1, 6 ) );
            (*f)( dest, &jmp, "Residual:        " + format_fixed( p->residual*RTOS, 1, 12 )
                              + "  +/-  " + format_fixed( p->reserr*semult*RTOS, 1, 6 ) );
        }
        else
        {
            (*f)( dest, &jmp, "Calculated:      " + format_fixed( p->calc, 4, 12 ) + "  +/-  " + format_fixed( p->calcerr*semult, 4, 6 ) );
            (*f)( dest, &jmp, "Residual:        " + format_fixed( p->residual, 4, 12 ) + "  +/-  " + format_fixed( p->reserr*semult, 4, 6 ) );
        }
        (*f)( dest, &jmp, "Standardised Residual: " + pad_right( format_fixed( p->sres*srmult, 3 ), 10 ) );
    }


    if( p->tgt.nclass )
    {
        int n;
        classdata *c;
        (*f)(dest, &jmp, "Classifications");
        for( n = p->tgt.nclass, c = sd->clsf + p->tgt.iclass; n--; c++ )
        {
            (*f)(dest, &jmp, "     " + pad_right( obs_classes.name( c->class_id ), 15 ) + "  " + obs_classes.value_name( c->class_id, c->name_id ) );
        }
    }
}

void list_single_observation( void *dest, PutTextFunc f, int from, int to, int obs_id )
{
    fconn_ptr *fp;
    tconn_ptr *tp;
    int to_id;
    survdata *sd;

    if( !connlst ) return;
    fp = &connlst[from];
    tp = 0;
    for( to_id = 0; to_id < fp->nconn; to_id++ )
    {
        if( fp->to[to_id].to  == to ) { tp = &fp->to[to_id]; break;}
    }
    if( !tp ) return;

    get_connection_data_by_id( from, to_id, obs_id, connection );

    sd = get_survdata_from_binary( connection->bloc );
    if( !sd ) return;

    switch ( sd->format )
    {
    case SD_OBSDATA: list_obsdata( dest, f, sd, connection->bloc, connection->idata ); break;
    case SD_VECDATA: list_vecdata( dest, f, sd, connection->flags, connection->bloc, connection->idata ); break;
    case SD_PNTDATA: list_pntdata( dest, f, sd, connection->idata ); break;
    }
}


void free_connection_resources()
{
    srList.clear();
    if( srIndex ) check_free( srIndex );
    srIndex = NULL;
    if( srIndex2 ) check_free( srIndex2 );
    srIndex2 = NULL;
    if( pens ) check_free( pens );
    pens = NULL;
    max_pens = 0;
    free_range_pens();

    free_connection_list();

    if( conn_file )
    {
        fclose( conn_file );
        conn_file = NULL;
    }

}
