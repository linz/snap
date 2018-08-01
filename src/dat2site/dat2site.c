#include "snapconfig.h"
/* dat2site.c

   Calculates approximate station coordinates from the observational
   data and some fixed coordinates.

   As written this is somewhat simplistic and is very prone to
   being influenced by gross errors! Also it totally ignores instrument
   and target heights.

*/


/*
   $Log: dat2site.c,v $
   Revision 1.5  1999/05/18 14:38:56  ccrook
   Added handling of projection bearings (ignoring convergence)

   Revision 1.4  1998/06/18 21:54:10  ccrook
   Modified calculations of coordinates from GPS vectors to use median rather than mean
   calculated coordinate - more robust.  Also for the moment ignored information coming from
   GPS vectors to marks without fixed stations.  In practice now nearly all adjustments are
   3d adjustments of GPS data only, so this shouldn't be an issue.

   Revision 1.2  1996/06/25 19:22:41  CHRIS
   Added -c option to confirm each calculation that is to be done.

   Revision 1.1  1996/01/03 22:45:24  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#define MAIN
#define GETVERSION_SET_PROGRAM_DATE
#include "util/datafile.h"
#include "util/linklist.h"
#include "util/fileutil.h"
#include "util/geodetic.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/readcfg.h"
#include "util/dms.h"
#include "util/pi.h"
#include "util/progress.h"
#include "coordsys/coordsys.h"
#include "network/network.h"
#include "snapdata/datatype.h"
#include "snapdata/snapdata.h"
#include "snapdata/loaddata.h"
#include "snap/filenames.h"
#include "snap/stnadj.h"
#include "snap/cfgprocs.h"
#include "snap/survfile.h"
#include "snap/survfilr.h"
#include "snap/snapglob.h"
#include "util/getversion.h"


#define COMMENT_CHAR '!'

typedef struct
{
    double xyz[3];
} GX_obs;

typedef struct
{
    double xyz[3];
} GB_obs;

typedef struct
{
    double az;
    int ro_id;  /* ro_id = 0 implies true azimuth */
} AZ_obs;

typedef struct
{
    double ds;
    int type;
} DS_obs;

#define DS_ARC 0
#define DS_HOR 1
#define DS_SLP 2

typedef struct
{
    double zd;
} ZD_obs;

typedef struct
{
    double hd;
} LV_obs;


struct stn_s;

typedef struct conn_s
{
    struct conn_s *next;
    struct stn_s *to;
    unsigned short flag;
    GB_obs gb;
    AZ_obs az;
    AZ_obs revaz;
    DS_obs ds;
    ZD_obs zd;
    LV_obs lv;
} conn;

#define CN_GB 0x01
#define CN_GX 0x02
#define CN_AZ 0x04
#define CN_DS 0x08
#define CN_ZD 0x10
#define CN_LV 0x20
#define CN_REVAZ 0x40
#define CN_REVZD 0x80
#define CN_INUSE 0x100   /* Temporary flag used in calculation routines */


#define ST_FIXH  0x01
#define ST_FIXV  0x02
#define ST_FIXHV 0x03
#define ST_FIXMASK 0x03
#define ST_HIDEFIX 0x80

typedef struct
{
    int link_ro;
    double corr;
} ro_def;


/* Tolerances accepted before error reported -
   note that instrument heights are not allowed for in these calculations,
   so tolerance must allow for this... */

#define GB_TOL 2.0
#define GX_TOL 5.0
#define AZ_TOL (1.0*RTOD)
#define DS_TOL 2.0
#define ZD_TOL (2.0*RTOD)
#define LV_TOL 1.0        /* May include calculated LV from ZD */

#define RADIUS_OF_EARTH 6378000.0

/* Types of fixes that are done in order of priority.  Stations are fixed
   in order depending upon the priority of the best fix available
   for each station.  Some types of fixes have a quality measure
   associated with them. Selection of staiton is based upon product
   of type and quality.  Quality runs from 0 to 1 */


/* Note: the fix_value and fix_effect array should be matched with this enum */

enum
{
    FIX_GPS_PT = 0,
    FIX_GPS_HV,
    FIX_LV_V,
    FIX_GPS_H,
    FIX_RAZDS_H,
    FIX_2RAZ_H,
    FIX_2DS_H,
    FIX_2AZDS_H,
    FIX_3AZ_H,
    NO_FIX_TYPES
};

/* Will attempt to fix by traverse if fix type to be used is greater than
   FIX_BY_TRAVERSE */

#define FIX_BY_TRAVERSE FIX_RAZDS_H

static double fix_value[NO_FIX_TYPES] =
{
    30000,
    9999,
    10000,
    9998,
    1000,
    999,
    998,
    500,
    50
};

#define MIN_FIX_QUALITY 5.0   /* Fixes of lower quality will not be attempted */

static char fix_effect[NO_FIX_TYPES] =
{
    ST_FIXHV,
    ST_FIXHV,
    ST_FIXV,
    ST_FIXH,
    ST_FIXH,
    ST_FIXH,
    ST_FIXH,
    ST_FIXH,
    ST_FIXH
};


/* Reduction in fix quality if only slope distances are available */

#define SLP_DIST_FACTOR 0.2

/* Definition of information about a station */

typedef struct stn_s
{
    station *st;
    char *code;
    int id;
    char fixed;
    int traverse;
    double travwgt;
    double travxy[2];
    double xy[2];  /* Plane projection coords for calcs */
    /* Point observation data */
    unsigned int flag;
    GX_obs gx;
    /* Info relating to possible fixes of station */
    int fix_order;
    int besttype;
    double bestfix;
    char can_fix[NO_FIX_TYPES];
    double fix_quality[NO_FIX_TYPES];
    conn *connlist;  /* Head of linked list of connections */
} stn;

static void *stnlist;
static stn **stations;
static ro_def *ro_list;
static double max_ro_diff;

static int next_stn = 1;
static int nostns;

static long nvecdata = 0;

static int max_ro_id = 0;

static int listonly = 0;
static int userejected = 0;

static FILE *errlog;
static int errcount;

#define CONFIRM_NONE  0
#define CONFIRM_FIXES 1
#define CONFIRM_QUIT  2
static int confirm_status = CONFIRM_NONE;

static int logfix = 1;
#define LOGFIX logfix

static void printlog( const char *fmt,...);
#define PRINTLOG(x,y) if(x) printlog y

#define FOR_ALL_STATIONS(st) \
   reset_list_pointer( stnlist ); \
   while( NULL != (st = (stn *) next_list_item( stnlist )))

#define FOR_ALL_CONNECTIONS(st,cn) \
   for( cn = st->connlist; cn; cn = cn->next )

/*====================================================================*/
/* Read in the data                                               */

static void close_station_list( void )
{
    stn *st;
    if( next_stn <= 0 ) return;
    stations = (stn **) check_malloc( sizeof(stn) * next_stn );
    if( !stations )
    {
        printf("Not enough memory for program\n");
        exit(0);
    }
    FOR_ALL_STATIONS(st)
    {
        stations[st->id] = st;
    }
    nostns = next_stn-1;
    next_stn = -1;
}

static stn *new_stn( const char *code )
{
    stn *st = (stn * ) add_to_list( stnlist, NEW_ITEM );
    st->code = copy_string( code );
    st->st = NULL;
    st->id = next_stn++;
    st->fixed = 0;
    st->connlist = NULL;
    st->bestfix = 0.0;
    st->besttype = 0;
    for( int i = 0; i < NO_FIX_TYPES; i++ )
    {
        st->can_fix[i] = 0;
        st->fix_quality[i] = 0.0;
    }

    st->flag = 0;
    return st;
}

static stn *find_stn( const char *code )
{
    stn *st;
    FOR_ALL_STATIONS(st)
    {
        if( _stricmp( st->code, code ) == 0 ) return st;
    }
    return 0;
}

static station *find_network_station( const char *code )
{
    return station_ptr(net,find_station(net,code));
}

static void link_station( station *s, stn *st )
{
    s->hook=st;
    st->st=s;
}

static stn *get_station( const char *code )
{
    stn *st=0;
    if( net )
    {
        station *s=find_network_station(code);
        if( s ) st=(stn *)(s->hook);
    }
    if( ! st )
    {
        st=find_stn(code);
    }
    if( st  ) return st;
    if( next_stn < 0 ) return 0;
    return new_stn( code );
}

void add_network_stations()
{
    if ( ! net ) return;
    int nstns=number_of_stations(net);
    for( int i=0; i++ < nstns; )
    {
        station *s=stnptr(i);
        if( ! s->hook )
        {
            stn *st=find_stn(s->Code);
            if( st ) link_station(s, st);
        }
    }
    for( int i=0; i++ < nstns; )
    {
        station *s=stnptr(i);
        stn *st=(stn *)(s->hook);
        if( ! st )
        {
            st=new_stn(s->Code);
            link_station(s,st);
        }
        st->fixed = ST_FIXHV;
    }
}

static int get_station_id( const char *code )
{
    return get_station(code)->id;
}


static stn *station_from_id( int id )
{
    stn *st=0;
    station *s=stnptr(id);
    if( s ) st=(stn *)(s->hook);
    if( st && st->id == id ) return st;
    FOR_ALL_STATIONS(st)
    {
        if( st->id == id ) return st;
    }
    return 0;
}


static double ds_calc_arc_distance( stn *from, stn *to )
{
    if( ! from->st || ! to->st ) return 0.0;
    return calc_distance( from->st, -from->st->OHgt, to->st, -to->st->OHgt,
                          NULL, NULL );
}

static double ds_calc_azimuth( stn *from, stn *to )
{
    if( ! from->st || ! to->st ) return 0.0;
    return calc_azimuth( from->st, 0.0, to->st, 0.0, 0, NULL, NULL );
}

static double ds_calc_height_diff( stn *from, stn *to )
{
    if( ! from->st || ! to->st ) return 0.0;
    return to->st->OHgt - from->st->OHgt;
}


static char *dstation_code( int id )
{
    stn *st;
    st = station_from_id( id );
    return st ? st->code : NULL;
}


static conn * get_connection( stn *from, stn *to )
{
    conn *cn;
    FOR_ALL_CONNECTIONS(from,cn)
    {
        if( cn->to == to ) return cn;
    }
    cn = (conn *) check_malloc( sizeof(conn) );
    cn->next = from->connlist;
    from->connlist = cn;
    cn->to = to;
    cn->flag = 0;
    return cn;
}

static void fixup_connection_angles( stn *from, int ro_id, double change, int new_id )
{
    conn *cn, *rcn;
    FOR_ALL_CONNECTIONS(from,cn)
    {
        if( (cn->flag & CN_AZ) && (cn->az.ro_id == ro_id ) )
        {
            cn->az.az += change;
            cn->az.ro_id = new_id;
            rcn = get_connection( cn->to, from );
            rcn->revaz.az += change;
            rcn->revaz.ro_id = new_id;
        }
    }
}

/* Adding vector data - not very scientific - average current obs with
   previous average - always favours last obs */


static void add_vecdata( stn * from, stn * to, double value[3], int type )
{
    if( type == GB )
    {
        conn *cn, *rcn;
        if( ! from ) return;
        cn = get_connection( from, to );
        rcn = get_connection( to, from );
        if( cn->flag & CN_GB )
        {
            double diff;
            diff = fabs(cn->gb.xyz[0] - value[0]) +
                   fabs(cn->gb.xyz[1] - value[1]) +
                   fabs(cn->gb.xyz[2] - value[2]);
            if( diff > GB_TOL )
            {
                fprintf( errlog,
                         "\nFrom %s to %s\n: GPS baselines differ by %.2f m\n",
                         from->code, to->code, diff );
                errcount++;
            }
            cn->gb.xyz[0] = (cn->gb.xyz[0]+value[0])/2;
            cn->gb.xyz[1] = (cn->gb.xyz[1]+value[1])/2;
            cn->gb.xyz[2] = (cn->gb.xyz[2]+value[2])/2;
        }
        else
        {
            cn->flag |= CN_GB;
            cn->gb.xyz[0] = value[0];
            cn->gb.xyz[1] = value[1];
            cn->gb.xyz[2] = value[2];
        }
        rcn->gb.xyz[0] = -cn->gb.xyz[0];
        rcn->gb.xyz[1] = -cn->gb.xyz[1];
        rcn->gb.xyz[2] = -cn->gb.xyz[2];
        rcn->flag |= CN_GB;
    }
    else if( type == GX )
    {
        if( to->flag & CN_GX )
        {
            double diff;
            diff = fabs(value[0] - to->gx.xyz[0]) +
                   fabs(value[1] - to->gx.xyz[1]) +
                   fabs(value[2] - to->gx.xyz[2]);
            if( diff > GX_TOL )
            {
                fprintf( errlog,
                         "\nFrom %s\n: Point coordinate obs differ by %.2f m\n",
                         to->code, diff );
                errcount++;
            }
            to->gx.xyz[0] = (to->gx.xyz[0] + value[0])/2;
            to->gx.xyz[1] = (to->gx.xyz[1] + value[1])/2;
            to->gx.xyz[2] = (to->gx.xyz[2] + value[2])/2;

        }
        else
        {
            to->gx.xyz[0]= value[0];
            to->gx.xyz[1]= value[1];
            to->gx.xyz[2]= value[2];
            to->flag |= CN_GX;
        }
    }
}


/* Favour ellipsoidal over horizontal over slope distances */

static void add_dsdata ( stn *from, stn *to, double dist, char type )
{
    conn *cn, *rcn;
    cn = get_connection( from, to );
    rcn = get_connection( to, from );
    if( cn->flag & CN_DS )
    {
        if( type < cn->ds.type )
        {
            double diff;
            diff = fabs(cn->ds.ds - dist);
            if( diff > DS_TOL )
            {
                fprintf(errlog, "From %s to %s: Distances differ by %.1f m\n" ,
                        from->code, to->code, diff );
            }
            cn->ds.ds = dist;
            cn->ds.type = type;
        }
        else if( type == cn->ds.type )
        {
            cn->ds.ds = (dist + cn->ds.ds)/2.0;
        }
    }
    else
    {
        cn->ds.ds = dist;
        cn->ds.type = type;
        cn->flag |= CN_DS;
    }
    rcn->ds.ds = cn->ds.ds;
    rcn->ds.type = cn->ds.type;
    rcn->flag |= CN_DS;
}


/* Don't add reverse zenith distances */

static void add_zddata( stn *from, stn *to, double zd )
{
    conn *cn;
    cn = get_connection( from, to );
    if( cn->flag & CN_ZD )
    {
        double diff;
        diff = fabs(cn->zd.zd-zd);
        if( diff > ZD_TOL )
        {
            fprintf(errlog,"From %s to %s: Zenith distances differ by %.3f degrees\n",
                    from->code, to->code, diff);
            errcount++;
        }
        cn->zd.zd = (cn->zd.zd+zd)/2.0;
    }
    else
    {
        cn->zd.zd = zd;
        cn->flag |= CN_ZD;
    }
    cn = get_connection( to, from );
    cn->flag |= CN_REVZD;
}

/* Add levelling data */

static void add_lvdata( stn *from, stn *to, double hd )
{
    conn *cn, *rcn;
    cn = get_connection( from, to );
    rcn = get_connection( to, from );
    if( cn->flag & CN_LV )
    {
        double diff;
        diff = fabs( cn->lv.hd - hd);
        if( diff > LV_TOL )
        {
            fprintf( errlog, "From %s to %s: Height differences differ by %.1fm\n",
                     from->code, to->code, diff);
            errcount++;
        }
        cn->lv.hd = (cn->lv.hd + hd)/2.0;
    }
    else
    {
        cn->flag |= CN_LV;
        cn->lv.hd = hd;
    }
    rcn->lv.hd = - cn->lv.hd;
    rcn->flag |= CN_LV;
}

/* Add horizontal angle/azimuth data - When overlaps another ro adjusts to
   all to match the lower ro and may return a correction to be applied
   for remaining angles at current ro.  Observed azimuths have ro_id of
   0, and so automatically override everything else */

static void add_azdata( stn *from, stn *to, double az, int *ro_id, double *ro_corr )
{
    conn *cn, *rcn;
    cn = get_connection( from, to );
    if( cn->flag & CN_AZ )
    {
        if( cn->az.ro_id == (*ro_id) )
        {
            double diff = cn->az.az - az;
            while( diff > PI ) diff -= TWOPI;
            while( diff < -PI ) diff += TWOPI;
            diff = fabs(diff);
            if( diff > AZ_TOL )
            {
                fprintf(errlog,"From %s to %s: Angle/azimuth discrepency of %.2f deg\n",
                        from->code, to->code, diff*RTOD );
                errcount++;
            }
            cn->az.az = (cn->az.az+az)/2.0;
        }
        else if( cn->az.ro_id < (*ro_id) )
        {
            *ro_corr = cn->az.az - az;
            fixup_connection_angles( from, (*ro_id), (*ro_corr), cn->az.ro_id);
            *ro_id = cn->az.ro_id;
        }
        else
        {
            fixup_connection_angles( from, cn->az.ro_id, az - cn->az.az, (*ro_id) );
        }
    }
    else
    {
        cn->az.az = az;
        cn->az.ro_id = *ro_id;
        cn->flag |= CN_AZ;
    }

    rcn = get_connection( to, from );
    rcn->flag |= CN_REVAZ;
    rcn->revaz.az = az+PI;
    rcn->revaz.ro_id = *ro_id;
}


static void load_data( survdata *sd )
{
    int i;
    stn *from;
    stn *to;
    if( listonly ) return;
    from = sd->from ? station_from_id( sd->from ) : 0;
    if( sd->format == SD_OBSDATA )
    {
        obsdata *o;
        double hacorr = 0.0;
        int newro = -1;
        int got_ha = 0;

        /* Need to ensure that all azimuth data are handled before
           all horizontal angles */

        if( ! from ) return;
        for( i = 0, o = sd->obs.odata; i < sd->nobs; i++, o++ )
        {
            trgtdata *tgt = &(o->tgt);
            if( tgt->unused && ! userejected ) continue;
            to = station_from_id( tgt->to );
            if( !to ) continue;
            if( tgt->type == HA ) { got_ha = 1; continue; }

            switch( tgt->type )
            {
            case SD:
            case DR: add_dsdata( from, to, o->value, DS_SLP ); break;

            case HD: add_dsdata( from, to, o->value, DS_HOR ); break;

            case MD:
            case ED: add_dsdata( from, to, o->value, DS_ARC ); break;

            case AZ: newro = 0;
                add_azdata( from, to, o->value, &newro, &hacorr );
                break;

                /* Treat projection bearings as the same as azimuths */
            case PB: newro = 0;
                add_azdata( from, to, o->value, &newro, &hacorr );
                break;

            case ZD: add_zddata( from, to, o->value ); break;

            case LV: add_lvdata( from, to, o->value ); break;
            }
        }
        if( got_ha )
        {
            newro = max_ro_id + 1;
            hacorr = 0.0;
            for( i = 0, o = sd->obs.odata; i < sd->nobs; i++, o++ )
            {
                trgtdata *tgt = &(o->tgt);
                if( tgt->type == HA )
                {
                    to = station_from_id( tgt->to );
                    if( !to ) continue;
                    add_azdata( from, to, o->value, &newro, &hacorr );
                }
            }
            if( newro > max_ro_id ) max_ro_id = newro;
        }
    }
    else if( sd->format == SD_VECDATA )
    {
        vecdata *v;
        for( i = 0, v = sd->obs.vdata; i < sd->nobs; i++, v++ )
        {
            trgtdata *tgt = &(v->tgt);
            if( tgt->unused && ! userejected ) continue;
            to = station_from_id( tgt->to );
            if( !to ) continue;
            add_vecdata( from, to, v->vector, tgt->type );
            nvecdata++;
        }
    }
}


// #pragma warning (disable : 4100)

static long get_id( int type, int group_id, const char *code )
{
    if( type == ID_STATION )
    {
        return (long) get_station_id( code );
    }
    else if ( type == ID_PROJCTN )
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static const  char * get_name( int type, int group_id, long id )
{
    if( type == ID_STATION )
    {
        return dstation_code( (int) id );
    }
    else
    {
        return NULL;
    }
}

static double get_value( int type, long id1, long id2 )
{
    switch( type )
    {
    case CALC_DISTANCE:
    case CALC_HDIST:
        return 1000.0;
        break;
    }
    return 0.0;
}

static void convert_zd_to_lv( stn *from, stn *to, double arcdist )
{
    conn *cn, *rcn;
    double corr, zd, hd;
    cn = get_connection( from, to );
    if( cn->flag & CN_LV ) return;
    rcn = get_connection( to, from );
    corr = -0.425*arcdist/RADIUS_OF_EARTH;
    if( cn->flag & CN_ZD && rcn->flag & CN_ZD )
    {
        double a1, a2, diff;
        a1 = cn->zd.zd + corr;
        a2 = rcn->zd.zd + corr;
        diff = fabs(a1 + a2 - PI);
        if( diff > ZD_TOL )
        {
            fprintf(errlog,"From %s to %s: Zenith distances differ by %.1f deg",
                    from->code, to->code, diff*RTOD );
            errcount++;
        }
        zd = (a1 + PI - a2)/2.0;
    }
    else if( cn->flag & CN_ZD )
    {
        zd = cn->zd.zd + corr;
    }
    else if( rcn->flag & CN_ZD )
    {
        zd = PI - (rcn->zd.zd + corr);
    }
    else
    {
        return;
    }
    hd = arcdist * cos(zd) / sin(zd);
    add_lvdata( from, to, hd );
}


static void fixup_all_zdds( void )
{
    conn *cn, *rcn;
    stn *st;
    double dist;
    FOR_ALL_STATIONS(st)
    {
        FOR_ALL_CONNECTIONS(st,cn)
        {
            if( cn->flag & CN_LV )
            {
                if( cn->flag & CN_DS && cn->ds.type == DS_SLP )
                {
                    double nds;
                    nds = cn->ds.ds*cn->ds.ds - cn->lv.hd*cn->lv.hd;
                    if( nds > 0.0 )
                    {
                        cn->ds.type = DS_HOR;
                        cn->ds.ds = sqrt(nds);
                        rcn = get_connection( cn->to, st );
                        rcn->ds.type = cn->ds.type;
                        rcn->ds.ds = cn->ds.ds;
                    }
                }
                continue;
            }
            if( ! (cn->flag & CN_DS) ) continue;
            if( ! (cn->flag & CN_ZD) ) continue;
            dist = cn->ds.ds;
            if( cn->ds.type == DS_SLP )
            {
                dist *= sin( cn->zd.zd - 0.425*dist/RADIUS_OF_EARTH);
                cn->ds.ds = dist;
                cn->ds.type = DS_HOR;
                rcn = get_connection( cn->to, st );
                rcn->ds.ds = dist;
                rcn->ds.type = DS_HOR;
            }
            convert_zd_to_lv( st, cn->to, dist );
        }
    }
}

/* First pass at code to resolve horizontal angle ro's by comparing
   obs with reverse.  This is done very crudely - the first comparison
   of two ro's is used to determine their relative values.  This makes
   it very susceptible to errors in the data...

   Currently it only reports the maximum error in resolving ro's */


static void setup_ro_list( void )
{
    int i;
    if( max_ro_id == 0 ) return;
    ro_list = (ro_def *) check_malloc( sizeof(ro_def) * (max_ro_id+1) );
    for( i =  0; i <= max_ro_id; i++ )
    {
        ro_list[i].link_ro = i;
        ro_list[i].corr = 0.0;
    }
    max_ro_diff = 0.0;
}


static int get_link_ro( int ro, double *pcorr )
{
    double corr = 0.0;
    int start_ro = ro;
    char  s1 = 0;
    char  s2 = 0;
    if( !ro ) { if( pcorr ) *pcorr = 0.0; return 0; }
    while( ro_list[ro].link_ro < ro )
    {
        corr += ro_list[ro].corr;
        ro = ro_list[ro].link_ro;
        s2 = s1;
        s1 = 1;
    }

    /* Just to keep it up to date if we need to follow more than one link */
    if( s2 )
    {
        ro_list[start_ro].link_ro = ro;
        ro_list[start_ro].corr = corr;
    }
    if( pcorr ) *pcorr = corr;
    return ro;
}

static int is_fixed_azimuth( AZ_obs *o )
{
    double corr;
    int new_ro_id;
    if( !o->ro_id ) return 1;
    new_ro_id = get_link_ro( o->ro_id, &corr );
    o->ro_id = new_ro_id;
    o->az += corr;
    return o->ro_id ? 0 : 1;
}


/* Add a link between ro1 and ro2, such that if x1 is measured relative
   to ro1 and x2 is measured relative to ro2 then x1+corr = x2 */

static void add_ro_link( int ro1, int ro2, double corr )
{
    int lro1, lro2;
    double lcorr1, lcorr2;
    lro1 = get_link_ro( ro1, &lcorr1 );
    lro2 = get_link_ro( ro2, &lcorr2 );
    if( lro1 == lro2 )
    {
        double diff = lcorr1 - corr - lcorr2;
        while( diff > PI ) diff -= TWOPI;
        while( diff < -PI) diff += TWOPI;
        diff = fabs(diff);
        if( diff < max_ro_diff ) max_ro_diff = diff;
        return;
    }
    if( lro1 > lro2 )
    {
        int temp;
        double ctemp;
        temp = lro2; lro2 = lro1; lro1 = temp;
        ctemp = lcorr2; lcorr2 = lcorr1; lcorr1 = ctemp;
        corr = -corr;
    }
    ro_list[lro2].link_ro = lro1;
    ro_list[lro2].corr = lcorr1 - corr - lcorr2;
}


static void resolve_ha_ros( void )
{
    conn *cn;
    stn *st;
    double diff;
    setup_ro_list();
    max_ro_diff = 0.0;
    FOR_ALL_STATIONS(st)
    {
        FOR_ALL_CONNECTIONS(st,cn)
        {
            if( cn->to->id < st->id ) continue; /* To avoid doing everything twice*/
            if( ! (cn->flag & CN_AZ && cn->flag & CN_REVAZ ) ) continue;
            diff = cn->revaz.az - cn->az.az;
            while( diff > PI ) diff -= TWOPI;
            while( diff < -PI ) diff += TWOPI;
            add_ro_link( cn->az.ro_id, cn->revaz.ro_id, diff );
        }
    }
}


/***************************************************************/
/* Apply info for horizontally fixed station to upgrade data   */
/* used for fixing other stations.  Can do the following...    */
/* Act as an RO fix for horizontal angle data                  */
/* Allow ZD data to be converted to LV data                    */

static void use_horizontal_fix( stn *from )
{
    conn *cn;
    int azroid[5];
    double azcor[5];
    double azdst[5];
    int nro, iro;
    static int maxro = 5;
    nro = 0;
    FOR_ALL_CONNECTIONS(from,cn)
    {
        double arcdist;
        if( !(cn->to->fixed & ST_FIXH) ) continue;
        arcdist = ds_calc_arc_distance( from, cn->to );
        if( !(cn->flag & CN_LV) && (cn->flag & (CN_ZD | CN_REVZD)) )
        {
            convert_zd_to_lv( from, cn->to, arcdist );
        }
        if( cn->flag & CN_AZ )
        {
            for( iro = 0; iro < nro; iro++ )
            {
                if( azroid[iro] == cn->az.ro_id ) break;
            }
            if( iro == nro && iro < maxro)
            {
                azdst[iro] = azcor[iro] = 0.0;
                azroid[iro] = cn->az.ro_id;
                nro++;
            }
            if( iro < maxro && arcdist > azdst[iro] )
            {
                azdst[iro] = arcdist;
                azcor[iro] = ds_calc_azimuth( from, cn->to ) - cn->az.az;
            }
        }
    }
    /* If distance is too int don't want to control azimuths by it */

    for( iro = 0; iro < nro; iro++ )
    {
        if( azdst[iro] > 2.0 ) add_ro_link( azroid[iro], 0, azcor[iro] );
    }
}

/* Apply info for a vertically fixed station.  Only real effect is to allow
   slope distances to be converted to horizontal */

static void use_vertical_fix( stn *from )
{
    conn *cn, *rcn;
    FOR_ALL_CONNECTIONS(from,cn)
    {
        if( !(cn->to->fixed & ST_FIXV) ) continue;
        if( cn->flag & CN_DS && cn->ds.type == DS_SLP )
        {
            double hdiff = ds_calc_height_diff( from, cn->to );
            double ds2 = cn->ds.ds * cn->ds.ds - hdiff * hdiff;
            if( ds2 > 0.0 )
            {
                cn->ds.ds = sqrt( ds2 );
                cn->ds.type = DS_HOR;
                rcn = get_connection( cn->to, from );
                rcn->ds.ds = cn->ds.ds;
                rcn->ds.type = DS_HOR;
            }
        }
    }
}

/*********************************************************/
/* Maintain a list of stations in the order that they    */
/* can be fixed..                                        */

stn ** fix_list = NULL;

static void setup_fix_order( void )
{
    stn *st;
    int i = 0;
    fix_list = (stn **) check_malloc( nostns * sizeof(stn *) );
    FOR_ALL_STATIONS(st)
    {
        fix_list[i] = st;
        st->fix_order = i;
        i++;
    }
}

static void update_fix_order( stn *st )
{
    double bestfix = 0.0;
    int fo, i, besttype = -1;
    char needed = 0;
    if( !(st->fixed & ST_FIXH) ) needed |= ST_FIXH;
    if( !(st->fixed & ST_FIXV) ) needed |= ST_FIXV;

    for( i = 0; i < NO_FIX_TYPES; i++ )
    {
        double fq;
        if( ! (fix_effect[i] & needed) ) continue;
        if( ! st->can_fix[i] ) continue;

        fq = fix_value[i] * st->fix_quality[i];
        if( fq > bestfix ) { bestfix = fq; besttype = i;}
    }
    st->bestfix = bestfix;
    st->besttype = besttype;
    fo = st->fix_order;
    for(;;)
    {
        if( fo > 0 )
        {
            stn *next;
            next = fix_list[fo-1];
            if( next->bestfix < st->bestfix )
            {
                fix_list[fo] = next;
                next->fix_order = fo;
                fo--;
                fix_list[fo] = st;
                st->fix_order = fo;
                continue;
            }
        }
        if( fo < nostns-1 )
        {
            stn *next;
            next = fix_list[fo+1];
            if( next->bestfix >= st->bestfix )
            {
                fix_list[fo] = next;
                next->fix_order = fo;
                fo++;
                fix_list[fo] = st;
                st->fix_order = fo;
                continue;
            }
        }
        break;
    }
}


static void setup_point_fixes()
{
    stn *st;

    FOR_ALL_STATIONS(st)
    {
        int havefix = 0;
        if( st->flag & CN_GX )
        {
            st->can_fix[FIX_GPS_PT] = 1;
            st->fix_quality[FIX_GPS_PT] = 1;
            havefix = 1;
        }
        if( havefix ) update_fix_order(st);
    }
}

/**********************************************************/
/* Update fix quality of related stations when a station  */
/* is fixed...                                            */
/* Note: this code can be abbreviated - for example there */
/* is no need to check any other options as the info will */
/* never be used...                                       */


/* Check fixed azimuths.  If we have an azimuth that is
   fixed one way, but no azimuth the other way, then add
   an opposite fixed azimuth.  If azimuths are available
   both way then if one is fixed so must the other be,
   so we don't need to worry about this... */

static void check_fixed_azimuths( stn *from )
{
    conn *cn;
    FOR_ALL_CONNECTIONS(from,cn)
    {
        if( (cn->flag & CN_AZ) && (cn->flag & CN_REVAZ) ) continue;
        if( !(cn->flag & CN_AZ) && !(cn->flag & CN_REVAZ) ) continue;
        if( cn->flag & CN_AZ )
        {
            if( is_fixed_azimuth( &cn->az ) )
            {
                cn->revaz.az = cn->az.az;
                cn->revaz.ro_id = cn->az.ro_id;
                cn->flag |= CN_REVAZ;
            }
        }
        else
        {
            if( is_fixed_azimuth( &cn->revaz ) )
            {
                cn->az.az = cn->revaz.az;
                cn->az.ro_id = cn->revaz.ro_id;
                cn->flag |= CN_AZ;
            }
        }
    }
}

static void update_fix_info( stn *from, stn *newfix )
{
    conn *newcn, *cn;
    int once;

    /* Ensure that azimuths for which the ro has been fixed are
       now entered as both a forward and reverse azimuth */

    check_fixed_azimuths( from );

    newcn = get_connection( from, newfix );

    /* Enclose in a for loop to allow breaking out with goto */

    for( once=1; once; once=0)
    {

        /* GPS fixes */

        if( newcn->flag & CN_GB && newcn->to->fixed & ST_FIXH )
        {
            int gpsfix = FIX_GPS_H;
            if( newcn->to->fixed & ST_FIXV ) gpsfix = FIX_GPS_HV;
            from->can_fix[gpsfix] = 1;
            from->fix_quality[gpsfix] = 1.0;

            /* If we've got this - don't need anything else */

            if( gpsfix == FIX_GPS_HV || from->fixed & ST_FIXV ) break;
        }

        /* Vertical fixes */

        if( newcn->flag & CN_LV &&
                newcn->to->fixed & ST_FIXV &&
                !(from->fixed & ST_FIXV)  )
        {
            from->can_fix[FIX_LV_V] = 1;
            from->fix_quality[FIX_LV_V] = 1.0;
        }

        /* The rest applies for horizontal fixes only */
        if( from->fixed & ST_FIXH ) break;


        /* New fixes from linked distances */

        if( newcn->to->fixed & ST_FIXH && newcn->flag & CN_DS )
        {
            int nsdst;
            int nhdst;
            double quality;

            if( newcn->flag & CN_REVAZ && is_fixed_azimuth( &newcn->revaz ) )
            {
                quality = (newcn->ds.type == DS_SLP) ? SLP_DIST_FACTOR : 1.0;
                from->can_fix[FIX_RAZDS_H] = 1;
                if( quality > from->fix_quality[FIX_RAZDS_H] )
                    from->fix_quality[FIX_RAZDS_H] = quality;
            }

            /* Connections by distance observations only - require at least
               three distances.  Use sine of included angle as measure of
               quality of fix.  Only need to consider possible angles at
               new station as already have quality for others...

               Don't worry about these if we've got RAZ+DS ifx */

            else
            {

                nsdst = nhdst = 0;
                if( newcn->ds.type == DS_SLP ) nsdst++; else nhdst++;
                quality = 0.0;
                FOR_ALL_CONNECTIONS(from,cn)
                {
                    double dst;
                    double cos;
                    if( cn->to == newfix || !(cn->to->fixed & ST_FIXH)
                            || !(cn->flag & CN_DS) ) continue;
                    if( cn->ds.type== DS_SLP ) nsdst++; else nhdst++;
                    dst = ds_calc_arc_distance( newfix, cn->to );
                    cos = (newcn->ds.ds*newcn->ds.ds + cn->ds.ds * cn->ds.ds - dst*dst)/
                          (2.0*newcn->ds.ds*cn->ds.ds);
                    cos = 1.0-cos*cos;
                    if( cos < 0.0 ) continue;
                    if( newcn->ds.type == DS_SLP || cn->ds.type == DS_SLP )
                        cos *= SLP_DIST_FACTOR;
                    if( cos > quality ) quality = cos;
                }
                if( nsdst+nhdst <= 1 ) from->fix_quality[FIX_2DS_H] = 0.0;
                if( quality > from->fix_quality[FIX_2DS_H] ) from->fix_quality[FIX_2DS_H] = quality;
                if( nsdst + nhdst > 2 ) from->can_fix[FIX_2DS_H] = 1;

                /* Connections like eccentric stations with obs from eccentric with
                   1 distance and several angles from the eccentric station */

                if( newcn->flag & CN_AZ )
                {
                    FOR_ALL_CONNECTIONS(from,cn)
                    {
                        double dst, quality;
                        if( cn->to == newfix
                                || !(cn->to->fixed & ST_FIXH)
                                || !(cn->flag & CN_AZ)
                                || (cn->az.ro_id != newcn->az.ro_id) ) continue;
                        dst = ds_calc_arc_distance( newfix, cn->to );
                        if( dst < newcn->ds.ds ) continue;  /* Don't want ambiguous distance */
                        if( !from->can_fix[FIX_2AZDS_H] )  from->fix_quality[FIX_2AZDS_H] = 0.0;
                        from->can_fix[FIX_2AZDS_H] = 1;
                        quality = newcn->ds.type == DS_SLP ? SLP_DIST_FACTOR : 1.0;
                        if( from->fix_quality[FIX_2AZDS_H] < quality )
                            from->fix_quality[FIX_2AZDS_H] = quality;
                    }
                }

            }
        }

        /* Fixes by azimuths from two horizontally fixed stations ... */

        if( newcn->to->fixed & ST_FIXH &&
                newcn->flag & CN_REVAZ &&
                is_fixed_azimuth( &newcn->revaz ) )
        {
            int nfixaz = 0;
            double quality = 0.0;
            FOR_ALL_CONNECTIONS(from,cn)
            {
                double qdiff;
                if( cn->to == newfix ||
                        ! (cn->flag & CN_REVAZ) ||
                        ! is_fixed_azimuth( &cn->revaz ) ) continue;
                nfixaz++;
                qdiff = fabs(sin(newcn->revaz.az - cn->revaz.az ));
                if( qdiff > quality ) quality = qdiff;
            }
            if( nfixaz == 0 ) from->fix_quality[FIX_2RAZ_H] = 0.0;
            if( nfixaz > 0 )
            {
                from->can_fix[FIX_2RAZ_H] = 1;
                if( quality > from->fix_quality[FIX_2RAZ_H] )
                    from->fix_quality[FIX_2RAZ_H] = quality;
            }
        }


        /* Fixes by resection - use quality measure to ensure preference for
           stations with more than 3 angles connecting.... */

        if( newcn->to->fixed & ST_FIXH &&
                newcn->flag & CN_AZ )
        {
            int naz = 0;
            FOR_ALL_CONNECTIONS(from,cn)
            {
                if( (cn->flag & CN_AZ) &&
                        (cn->to->fixed & ST_FIXH) &&
                        (cn->az.ro_id == newcn->az.ro_id) ) naz++;
            }
            if( naz > 2 )
            {
                double q;
                from->can_fix[FIX_3AZ_H] = 1;
                q = 0.5 + ((naz < 8) ? (8-naz)*0.1 : 0.5);
                if( q > from->fix_quality[FIX_3AZ_H] )
                    from->fix_quality[FIX_3AZ_H] = q;
            }
        }
    }; /* End of do... */

    /* Determine the best fix for the station */

    update_fix_order( from );
}


static void fix_station( stn *st, double lat, double lon, double hgt, int flag )
{
    char fixneeded;
    conn *cn;
    if( !st->st )
    {
        if( !(flag & ST_FIXH) ) lat = lon = 0.0;
        if( !(flag & ST_FIXV) ) hgt = 0.0;
        station *s = new_network_station( net, st->code, st->code, lat, lon, hgt,
                                      0.0, 0.0, 0.0 );
        link_station(s,st);
    }
    else
    {
        double xyz[3];
        double offset = 0.0;
        int i;
        for( i = 0; i < 3; i++ ) xyz[i] = st->st->XYZ[i];
        if( !(flag & ST_FIXH)) { lat = st->st->ELat; lon = st->st->ELon; }
        if( !(flag & ST_FIXV)) { hgt = st->st->OHgt; }
        modify_network_station_coords( net, st->st, lat, lon, hgt );
        for( i = 0; i < 3; i++ )
        {
            double dx = xyz[i] - st->st->XYZ[i];
            offset += dx*dx;
        }
        offset = sqrt(offset);
        if( ! (flag & ST_HIDEFIX) )
        {
            PRINTLOG(LOGFIX,("Station %s shifted %.2lf m\n",st->code,offset));
        }

    }
    st->fixed |= (flag & ST_FIXMASK);

    /* Update data that can take advantage of fixed station coords */
    if( flag & ST_FIXH ) use_horizontal_fix( st );
    if( flag & ST_FIXV ) use_vertical_fix( st );

    /* Now need to use fixed information to see how much better we can
       fix adjoining stations */

    FOR_ALL_CONNECTIONS(st,cn)
    {
        if( (cn->to->fixed & ST_FIXHV) == ST_FIXHV ) continue;
        update_fix_info( cn->to, st );
    }

    /* Also need to account for updating fix info for this station
       if not already fixed */

    fixneeded = 0;
    if( ! (st->fixed & ST_FIXH) ) fixneeded |= ST_FIXH;
    if( ! (st->fixed & ST_FIXV) ) fixneeded |= ST_FIXV;

    if( fixneeded ) FOR_ALL_CONNECTIONS(st,cn)
    {
        if( cn->to->fixed & fixneeded ) update_fix_info( st, cn->to );
    }

    update_fix_order( st );

}


/**********************************************************/
/* Fix the known stations.  Initially flag all as fixed,  */
/* then fix each on in turn.  Flag as fixed before fixing */
/* to avoid a lot of unnecessary work determining how well*/
/* the still to be fixed known stations can be fixed from */
/* the data                                               */

static void fix_known_stations( char **recalclist, int nrecalc )
{
    int i, j, maxstn;
    maxstn = number_of_stations( net );
    for( j = 0; j < nrecalc; j++ )
    {
        station *s=find_network_station(recalclist[j]);
        if( s && s->hook )
        {
            stn *st=(stn *)(s->hook);
            st->fixed=0;
        }
    }
    printf("\nFixing known stations\n");
    init_progress_meter(maxstn);
    for( i = 0; i++ < maxstn; )
    {
        station *s;
        stn *st;
        update_progress_meter(i);
        s = station_ptr( net, i );
        if( ! s->hook ) continue;
        st = (stn *)(s->hook);
        if( !st->fixed ) continue;
        st->fixed = 0;
        fix_station( st, s->ELat, s->ELon, s->OHgt, ST_FIXHV | ST_HIDEFIX );
    }
    end_progress_meter();

}

static int add_known_station( station *s )
{
    stn *st = get_station( s->Code );
    if( !st ) return 0;
    st->st = s;
    fix_station( st, s->ELat, s->ELon, s->OHgt, ST_FIXHV );
    return 1;
}


static int confirm_fix( char *msg )
{
    char response[80];
    char *c;
    if( confirm_status == CONFIRM_NONE ) return 1;
    if( confirm_status == CONFIRM_QUIT ) return 0;
    for(;;)
    {
        printf("%s? Y(es), N(o), Q(uit): ", msg);
        fgets(response,80,stdin);
        for( c = response; *c && *c==' '; c++ ) {}
        switch (*c)
        {
        case 0:
        case '\n':
        case 'y':
        case 'Y': return 1;

        case 'n':
        case 'N': return 0;

        case 'q':
        case 'Q': confirm_status = CONFIRM_QUIT; return 0;
        }
    }
}



/**********************************************************/

static int fix_with_gps( stn *st, double *lat, double *lon, double *hgt, int *fixtype )
{
    static coord_conversion toitrf;
    static coord_conversion tonet;
    static coordsys *itrf;
    static coordsys *netcs;
    static int got_conversion = 0;
    static vector3 (*calc_xyz);
    static int *order;
    static int ncalc_xyz = 0;

    double xyz[3];
    conn *cn;
    int nhor, nvrt, nvec;
    int i, j1, j2;

    {
        char msg[80];
        sprintf(msg,"Fixing %.10s using GPS",st->code);
        if( !confirm_fix(msg) ) return 0;
    }

    PRINTLOG(LOGFIX,("Fixing %s using GPS\n",st->code ));


    if( !got_conversion )
    {
        coordsys *temp;
        netcs = related_coordsys( net->crdsys, CSTP_GEODETIC );
        temp = load_coordsys( "ITRF2008" );
        if( !temp|| define_coord_conversion(&toitrf,netcs,temp) != OK )
        {
            itrf = related_coordsys( net->crdsys, CSTP_CARTESIAN );
        }
        else
        {
            itrf = related_coordsys( temp, CSTP_CARTESIAN );
        }
        if( temp ) delete_coordsys( temp );
        define_coord_conversion( &toitrf, netcs, itrf );
        define_coord_conversion( &tonet, itrf, netcs );
        got_conversion = 1;
    }

    nhor = nvrt = nvec = 0;
    *lat = *lon = *hgt = 0.0;
    FOR_ALL_CONNECTIONS(st,cn)
    {
        if( !(cn->to->fixed & ST_FIXH) || !(cn->flag & CN_GB) ) continue;
        nvec++;
    }
    if( nvec <= 0 ) return 0;
    if( nvec > ncalc_xyz )
    {
        if( ncalc_xyz > 0 ) { check_free( calc_xyz ); check_free( order ); }
        ncalc_xyz = nvec * 2;
        calc_xyz = (vector3 *) check_malloc( ncalc_xyz * sizeof(vector3) );
        order = (int *) check_malloc( ncalc_xyz * sizeof(int) );
    }
    /* Form an array of all connections, nvec in total, first nhor have good
       3d coords, following nvrt may have suspect heights. */
    FOR_ALL_CONNECTIONS(st,cn)
    {
        int index;
        if( !(cn->to->fixed & ST_FIXH) || !(cn->flag & CN_GB) ) continue;
        xyz[CRD_LAT] = cn->to->st->ELat;
        xyz[CRD_LON] = cn->to->st->ELon;
        xyz[CRD_HGT] = cn->to->st->OHgt;
        if( convert_coords( &toitrf, xyz, NULL, xyz, NULL ) != OK ) continue;
        if( cn->to->fixed & ST_FIXV )
        {
            index = nhor++;
        }
        else
        {
            index = nvec - ++nvrt;
        }
        for( i = 0; i<3; i++ ) calc_xyz[index][i] = xyz[i] - cn->gb.xyz[i];
    }

    /* Find the median value using only completely fixed (3d) positions.
       For simplicity and because generally it won't apply, don't bother
       about horizontal only fixes... */

    if( nhor <= 0 ) return 0;
    j1 = (nhor-1)/2;
    j2 = nhor/2;

    for( i = 0; i < 3; i++ )
    {
        int j;
        order[0] = 0;
        for( j = 1; j < nhor; j++ )
        {
            int k;
            double test;
            test = calc_xyz[j][i];
            for( k = j; k > 0; k-- )
            {
                if( calc_xyz[order[k-1]][i] <= test ) break;
                order[k] = order[k-1];
            }
            order[k] = j;
        }
        xyz[i] = (calc_xyz[order[j1]][i] + calc_xyz[order[j2]][i])/2;
    }

    if( convert_coords( &tonet, xyz, NULL, xyz, NULL ) != OK ) return 0;
    *lat += xyz[CRD_LAT];
    *lon += xyz[CRD_LON];
    *hgt += xyz[CRD_HGT];
    return 1;
}


static int fix_with_gps_point( stn *st, double *lat, double *lon, double *hgt, int *fixtype )
{
    static coord_conversion tonet;
    static coordsys *itrf;
    static coordsys *netcs;
    static int got_conversion = 0;

    double llh[3];

    {
        char msg[80];
        sprintf(msg,"Fixing %.10s using GPS point observation",st->code);
        if( !confirm_fix(msg) ) return 0;
    }

    PRINTLOG(LOGFIX,("Fixing %s using GPS point observation\n",st->code ));

    if( !got_conversion )
    {
        coordsys *temp;
        netcs = related_coordsys( net->crdsys, CSTP_GEODETIC );
        temp = load_coordsys( "ITRF2008" );
        if( !temp|| define_coord_conversion( &tonet, temp, netcs ) != OK )
        {
            itrf = related_coordsys( net->crdsys, CSTP_CARTESIAN );
        }
        else
        {
            itrf = related_coordsys( temp, CSTP_CARTESIAN );
        }
        if( temp ) delete_coordsys( temp );
        define_coord_conversion( &tonet, itrf, netcs );
        got_conversion = 1;
    }


    if( convert_coords( &tonet, st->gx.xyz, NULL, llh, NULL ) != OK ) return 0;
    *lat += llh[CRD_LAT];
    *lon += llh[CRD_LON];
    *hgt += llh[CRD_HGT];
    return 1;
}

static int fix_with_hgtdif( stn *st, double *lt, double *ln, double *hgt )
{
    int nvrt;
    conn *cn;

    {
        char msg[80];
        sprintf(msg,"Fixing %.10s vertically using height difference",st->code);
        if( !confirm_fix(msg) ) return 0;
    }

    PRINTLOG(LOGFIX,("Fixing %s vertically using height difference\n",st->code ));

    *lt = *ln = *hgt = 0;
    nvrt = 0;
    FOR_ALL_CONNECTIONS(st,cn)
    {
        if( cn->to->fixed & ST_FIXV && cn->flag & CN_LV )
        {
            *hgt += cn->to->st->OHgt - cn->lv.hd;
            nvrt++;
        }
    }
    if( nvrt ) *hgt /= nvrt;
    return nvrt ? 1 : 0;
}


static double vxyz[3], exyz[3], nxyz[3], cxyz[3];

static void setup_plane_projection(  double xyz[3] )
{
    int i;
    double v;
    v = 0.0;
    for( i = 0; i < 3; i++ )
    {
        cxyz[i] = vxyz[i] = xyz[i];
        v += vxyz[i]*vxyz[i];
    }
    v  = sqrt(v);
    for( i = 0; i < 3; i++ ) vxyz[i] /= v;
    v = _hypot(vxyz[0],vxyz[1]);
    exyz[0] = -vxyz[1]/v;
    exyz[1] = vxyz[0]/v;
    exyz[2] = 0;
    nxyz[0] = -vxyz[0]*vxyz[2]/v;
    nxyz[1] = -vxyz[1]*vxyz[2]/v;
    nxyz[2] = v;
}

static void calc_projection_coords( stn *st )
{
    int i;
    st->xy[0] = st->xy[1] = 0;
    for( i = 0; i < 3; i++ )
    {
        double dx;
        dx = st->st->XYZ[i] - cxyz[i];
        st->xy[0] += dx*exyz[i];
        st->xy[1] += dx*nxyz[i];
    }
}


static int setup_plane_projection_at( stn *st )
{
    conn *cn;
    int nstn, i;
    double xyz[3];
    xyz[0] = xyz[1] = xyz[2] = 0.0;
    nstn = 0;
    FOR_ALL_CONNECTIONS(st,cn)
    {
        stn *st2 = cn->to;
        if( st2->fixed & ST_FIXH )
        {
            for( i = 0; i < 3; i++ ) xyz[i] += st2->st->XYZ[i];
            nstn++;
        }
    }
    if( !nstn ) return 0;
    for( i = 0; i < 3; i++ ) xyz[i] /= nstn;
    setup_plane_projection( xyz );
    FOR_ALL_CONNECTIONS(st,cn)
    {
        stn *st2 = cn->to;
        if( st2->fixed & ST_FIXH )
        {
            calc_projection_coords( st2 );
        }
    }
    return 1;
}


static void plane_to_geodetic( double plxy[2], double *lat, double *lon )
{
    int i;
    double xyz[3];
    for( i = 0; i < 3; i++ )
    {
        xyz[i] = cxyz[i] + plxy[0] * exyz[i] + plxy[1] * nxyz[i];
    }
    xyz_to_llh( net->crdsys->rf->el, xyz, xyz );
    *lat = xyz[CRD_LAT];
    *lon = xyz[CRD_LON];
}

static int fix_with_azds( stn *st, double *lt, double *ln, double *hgt )
{
    double xy[2];
    double nconn;
    conn *cn;

    {
        char msg[80];
        sprintf(msg,"Fixing %.10s using azimuths and distances",st->code);
        if( !confirm_fix(msg) ) return 0;
    }

    PRINTLOG(LOGFIX,("Fixing %s using azimuths and distances\n",st->code ));

    if( !setup_plane_projection_at(st) ) return 0;
    xy[0] = xy[1] = 0.0;
    nconn = 0;
    FOR_ALL_CONNECTIONS(st,cn)
    {
        if( (cn->to->fixed & ST_FIXH) &&
                (cn->flag & CN_DS) &&
                (cn->flag & CN_REVAZ) &&
                (is_fixed_azimuth(&cn->revaz)) )
        {
            double ds;
            ds = cn->ds.ds;
            if( cn->ds.type == DS_SLP )
            {
                ds = cn->ds.ds * SLP_DIST_FACTOR;
                nconn += SLP_DIST_FACTOR;
            }
            else
            {
                ds = cn->ds.ds;
                nconn++;
            }
            xy[0] += cn->to->xy[0] - ds*sin(cn->revaz.az);
            xy[1] += cn->to->xy[1] - ds*cos(cn->revaz.az);
        }
    }
    if( nconn <= 0.0 ) return 0;
    xy[0] /= nconn;
    xy[1] /= nconn;
    plane_to_geodetic( xy, lt, ln );
    *hgt = 0.0;

    return 1;
}

static double distance( double xy1[2], double xy2[2] )
{
    return _hypot( xy2[0]-xy1[0], xy2[1]-xy1[1] );
}

static double bearing( double xy1[2], double xy2[2] )
{
    return atan2( xy2[0]-xy1[0], xy2[1]-xy1[1] );
}


static int fix_with_azaz( stn *st, double *lt, double *ln, double *hgt )
{
    double N[3], b[2], det, xy[2];
    int n;
    conn *cn;

    {
        char msg[80];
        sprintf(msg,"Fixing %.10s using intersecting azimuths",st->code);
        if( !confirm_fix(msg) ) return 0;
    }

    PRINTLOG(LOGFIX,("Fixing %s using intersecting azimuths\n",st->code ));

    if( !setup_plane_projection_at(st) ) return 0;
    n = 0;
    N[0] = N[1] = N[2] = 0.0;
    b[0] = b[1] = 0.0;
    FOR_ALL_CONNECTIONS(st,cn)
    {
        if( (cn->to->fixed & ST_FIXH) &&
                (cn->flag & CN_REVAZ) &&
                is_fixed_azimuth(&cn->revaz) )
        {
            double c, s, v;
            n++;
            c = cos(cn->revaz.az);
            s = sin(cn->revaz.az);
            v = c * cn->to->xy[0] - s * cn->to->xy[1];
            N[0] += c*c;
            N[1] -= c*s;
            N[2] += s*s;
            b[0] += c*v;
            b[1] += -s*v;
        }
    }
    if( n < 2 ) return 0;
    det = N[0]*N[2]-N[1]*N[1];
    if( det < 0.01 ) return 0;
    xy[0] = (N[2]*b[0] - N[1]*b[1])/det;
    xy[1] = (N[0]*b[1] - N[1]*b[0])/det;
    plane_to_geodetic( xy, lt, ln );
    *hgt = 0.0;

    return 1;
}

static int fix_with_dsds( stn *st, double *lt, double *ln, double *hgt )
{
    conn *cn, *cn1, *cnd1, *cnd2;
    double qmax, d1, d2, d12, az, daz, xy1[2], xy2[2];


    {
        char msg[80];
        sprintf(msg,"Fixing %.10s using a distances",st->code);
        if( !confirm_fix(msg) ) return 0;
    }

    PRINTLOG(LOGFIX,("Fixing %s using distances to fixed stations\n",st->code ));


    /* Find the two distances best suited to fixing the position - that is
       with angles closest to 90 degrees between them */

    if( !setup_plane_projection_at(st) ) return 0;

    qmax = 0.0;
    cnd1 = cnd2 = NULL;
    d12 = 0.0;
    FOR_ALL_CONNECTIONS(st,cn)
    {
        cn->flag &= ~CN_INUSE;
        if( ! (cn->to->fixed & ST_FIXH) || ! (cn->flag & CN_DS) ) continue;
        cn->flag |= CN_INUSE;
        FOR_ALL_CONNECTIONS(st,cn1)
        {
            double d, s2;
            if( cn1 == cn ) break;
            if( !(cn1->flag & CN_INUSE)) continue;
            d = distance( cn->to->xy, cn1->to->xy);
            s2 = (cn->ds.ds*cn->ds.ds + cn1->ds.ds*cn1->ds.ds - d*d)/
                 (2.0 * cn->ds.ds * cn1->ds.ds );
            s2 = 1.0 - s2*s2;
            if( s2 < 0.001 ) continue;  /* Angle to acute to use */
            if( cn->ds.type == DS_SLP || cn1->ds.type == DS_SLP ) s2 *= SLP_DIST_FACTOR;
            if( s2 > qmax )
            {
                cnd1 = cn;
                cnd2 = cn1;
                qmax = s2;
                d12 = d;
            }

        }
    }

    if( !cnd1 || !cnd2 ) return 0;

    /* Work out the two possible positions for the new station given the
       distances from cnd1 and cnd2 */

    d1 = cnd1->ds.ds;
    d2 = cnd2->ds.ds;

    daz = (d1*d1+d12*d12-d2*d2)/(2.0*d1*d12);
    if( daz >= 1.0 ) return 0;
    if( daz <= -1.0 ) return 0;
    daz = atan2( sqrt(1-daz*daz), daz );

    az = bearing( cnd1->to->xy, cnd2->to->xy );
    xy1[0] = cnd1->to->xy[0] + d1 * sin(az+daz);
    xy1[1] = cnd1->to->xy[1] + d1 * cos(az+daz);
    xy2[0] = cnd1->to->xy[0] + d1 * sin(az-daz);
    xy2[1] = cnd1->to->xy[1] + d1 * cos(az-daz);

    /* Now look at the other distances to decide which is the best
       fit */

    d1 = d2 = 0;
    FOR_ALL_CONNECTIONS(st,cn)
    {
        double dif;
        if( !(cn->flag & CN_INUSE) ) continue;
        if( cn == cnd1 || cn == cnd2 ) continue;
        dif = distance( xy1, cn->to->xy ) - cn->ds.ds;
        dif *= dif;
        if( cn->ds.type == DS_SLP ) dif *= SLP_DIST_FACTOR;
        d1 += dif;
        dif = distance( xy2, cn->to->xy ) - cn->ds.ds;
        dif *= dif;
        if( cn->ds.type == DS_SLP ) dif *= SLP_DIST_FACTOR;
        d2 += dif;
    }
    if( d1 > 100*d2 )
    {
        plane_to_geodetic( xy2, lt, ln );
    }
    else if( d2 > 100*d1 )
    {
        plane_to_geodetic( xy1, lt, ln );
    }
    else
    {
        return 0;   /* Not convincing which is right */
    }
    *hgt = 0.0;

    return 1;
}



static int fix_with_dshaha( stn *st, double *lt, double *ln, double *hgt )
{
    conn *cn, *cn1, *cnd1, *cnd2;
    double d1, d2, d12, qmax, a12, cs, sn, xy[2];
    int ro;

    {
        char msg[80];
        sprintf(msg,"Fixing %.10s using a distance and horizontal angles",st->code);
        if( !confirm_fix(msg) ) return 0;
    }

    PRINTLOG(LOGFIX,("Fixing %s using a distance and horizontal angles\n",st->code ));


    setup_plane_projection_at( st );
    /* Seek a pair with a distance and the largest distance to the
       other station */

    cnd1 = cnd2 = NULL;
    qmax = 0.0;
    FOR_ALL_CONNECTIONS(st,cn)
    {
        if( !(cn->to->fixed & ST_FIXH) ||
                !(cn->flag & CN_DS) ||
                !(cn->flag & CN_AZ) ) continue;
        d1 = cn->ds.ds;
        ro = cn->az.ro_id;
        FOR_ALL_CONNECTIONS(st,cn1)
        {
            if( cn1 == cn ) continue;
            if( !(cn1->to->fixed & ST_FIXH ) ) continue;
            if( !(cn1->flag & CN_AZ) ) continue;
            if( cn1->az.ro_id != ro ) continue;
            d2 = distance( cn->to->xy, cn1->to->xy );
            if( d2 <= d1 ) continue;
            if( cn->ds.type == DS_SLP ) d2 *= SLP_DIST_FACTOR;
            if( d2 > qmax )
            {
                qmax = d2;
                cnd1 = cn;
                cnd2 = cn1;
            }
        }
    }
    if( !cnd1 || !cnd2 ) return 0;

    /* Work out the missing distance, then the missing angle at station
       cnd1 */

    a12 = cnd2->az.az - cnd1->az.az;
    d12 = distance( cnd1->to->xy, cnd2->to->xy );
    d1 = cnd1->ds.ds;
    d2 = d1*sin(a12);
    d2 = d1*cos(a12) + sqrt( d12*d12 - d2*d2);

    /* Bearing from at cnd1->to from cnd2->to to st */

    sn = d2*sin(a12)/d12;
    cs = (d1*d1 + d12*d12-d2*d2)/(2.0*d1*d12);
    a12 = bearing(cnd1->to->xy,cnd2->to->xy) + atan2( sn, cs );

    xy[0] = cnd1->to->xy[0] + d1 * sin(a12);
    xy[1] = cnd1->to->xy[1] + d1 * cos(a12);
    plane_to_geodetic( xy, lt, ln );
    *hgt = 0.0;

    return 1;
}

typedef struct
{
    double xy[2];
    double r;
} rsc_def;

static int fix_by_resection( stn *st, double *lt, double *ln, double *hgt )
{
    rsc_def *centre;
    int ro, nro, maxro, maxnro;
    double qmax;
    conn *cn, *cn1;
    int ninuse;
    double cs, sn;
    double dx, dy, d1, d2, d12;
    int i1 = 0, i2 = 0, c1, c2;
    double az, daz;
    double xy1[2], xy2[2];

    {
        char msg[80];
        sprintf(msg,"Fixing %.10s using resection",st->code);
        if( !confirm_fix(msg) ) return 0;
    }

    PRINTLOG(LOGFIX,("Fixing %s using resection\n",st->code ));

    setup_plane_projection_at(st);

    /* First find the ro_id with the most observations (usually there
       will only be one ro_id */

    ninuse = 0;
    FOR_ALL_CONNECTIONS(st,cn)
    {
        cn->flag &= ~CN_INUSE;
        if( !(cn->to->fixed & ST_FIXH) ) continue;
        if( !(cn->flag & CN_AZ) ) continue;
        cn->flag |= CN_INUSE;
        ninuse++;
    }
    maxnro = -1;
    maxro = -1;
    while( ninuse )
    {
        ro = -1;
        nro = 0;
        FOR_ALL_CONNECTIONS(st,cn)
        {
            if( !(cn->flag & CN_INUSE) ) continue;
            if( ro < 0 ) ro = cn->az.ro_id;
            if( cn->az.ro_id == ro )
            {
                cn->flag &= ~CN_INUSE;
                nro++;
                ninuse--;
            }
        }
        if( ro < 0 ) break;
        if( nro > maxnro ) {maxro = ro; maxnro = nro;}
    }
    if( maxro <= 0 || maxnro <= 2 ) return 0;

    /* Each included angle defines a centre and a radius of a circle
       on which the point lies.  Allocate space to calculate all centres
       and radii... */

    ro = maxro;
    nro = (maxnro*(maxnro-1))/2;
    centre = (rsc_def *) check_malloc( nro * sizeof(rsc_def) );
    nro = 0;

    FOR_ALL_CONNECTIONS(st,cn)
    {
        if( !(cn->to->fixed & ST_FIXH) ) continue;
        if( !(cn->flag & CN_AZ)) continue;
        if( cn->az.ro_id != ro ) continue;
        cn->flag |= CN_INUSE;
        FOR_ALL_CONNECTIONS(st,cn1)
        {
            if( cn1 == cn ) break;
            if( !(cn->flag & CN_INUSE) ) continue;
            cs = cn1->az.az - cn->az.az;
            sn = sin(cs);
            if( fabs(sn) < 0.0001 ) continue;  /* Centre gets too far away... */
            cs = cos(cs);
            dx = cn1->to->xy[0] - cn->to->xy[0];
            dy = cn1->to->xy[1] - cn->to->xy[1];
            centre[nro].xy[0] = cn->to->xy[0] + (dx + dy * cs / sn )/2;
            centre[nro].xy[1] = cn->to->xy[1] + (dy - dx * cs / sn )/2;
            centre[nro].r = distance( centre[nro].xy, cn->to->xy );
            nro++;
        }
    }

    /* Find the pair of centres which give the best angle at the centre
       for calculating the position (ie the nearest to 90) */

    qmax = 0.01; /* Note: this value limits the intersection angle that
                   will be tolerated for fixing a station */
    c1 = c2 = -1;
    d12 = 0.0;
    for( i1 = 1; i1 < nro; i1++ ) for( i2 = 0; i2 < i1; i2++ )
        {
            double d1, d2, d, s2;
            d1 = centre[i1].r;
            d2 = centre[i2].r;
            d = distance( centre[i1].xy, centre[i2].xy );
            s2 = (d1*d1+d2*d2-d*d)/(2*d1*d2);
            s2 = 1.0 - s2*s2;
            if( s2 > qmax )
            {
                c1 = i1;
                c2 = i2;
                qmax = s2;
                d12 = d;
            }

        }

    if( c1 < 0 || c2 < 0 ) { check_free(centre); return 0;}

    /* Work out the two possible positions for the new station given the
       distances from cnd1 and cnd2 */

    d1 = centre[c1].r;
    d2 = centre[c2].r;

    daz = (d1*d1+d12*d12-d2*d2)/(2.0*d1*d12);
    if( daz >= 1.0 || daz <= -1.0 ) { check_free(centre); return 0;}
    daz = atan2( sqrt(1-daz*daz), daz );

    az = bearing( centre[c1].xy, centre[c2].xy );
    xy1[0] = centre[c1].xy[0] + d1 * sin(az+daz);
    xy1[1] = centre[c1].xy[1] + d1 * cos(az+daz);
    xy2[0] = centre[c1].xy[0] + d1 * sin(az-daz);
    xy2[1] = centre[c1].xy[1] + d1 * cos(az-daz);

    /* Now look at the other distances to decide which is the best
       fit */

    d1 = d2 = 0;
    for( i1 = 0; i1 < nro; i1++ )
    {
        double dif;
        if( i1 == c1 || i2 == c2 ) continue;
        dif = distance( xy1, centre[i1].xy ) - centre[i1].r;
        dif *= dif;
        d1 += dif;
        dif = distance( xy2, centre[i1].xy ) - centre[i1].r;
        dif *= dif;
        d2 += dif;
    }

    check_free( centre );

    if( d1 > 100*d2 )
    {
        plane_to_geodetic( xy2, lt, ln );
    }
    else if( d2 > 100*d1 )
    {
        plane_to_geodetic( xy1, lt, ln );
    }
    else
    {
        return 0;   /* Not convincing which is right */
    }
    *hgt = 0.0;

    return 1;
}

/**********************************************************/
/* Code to fix stations by angle + distance traverse      */

static int fix_by_traverse( void )
{
    stn *st, *start;
    conn *cn;
    int start_ro_id;
    int current_id;
    int fixed_stns;
    int new_count;
    int nro;
    int iro;
    int maxro = 5;
    int ro_id[5];
    double ro_corr[5];
    double ro_wgt[5];
    double azcorr;
    double xyz[3];
    double ca[2], ct[2], ss, sc, rr;
    double maxerr;
    int nfixed;
    int i;
    double lt, ln;

    /* First find all the stations that can be connected by distances
       and angles */

    /* Clear out the traverse id's */
    FOR_ALL_STATIONS(st)
    {
        st->traverse = 0;
        st->travwgt = 0.0;
        st->travxy[0] = st->travxy[1] = 0.0;
    }

    /* Find a starting station.  That is a station which is not fixed, and
       which is connected to a fixed station by an horizontal angle and distance */

    start = NULL;
    start_ro_id = 0;
    FOR_ALL_STATIONS(st)
    {
        if( st->fixed & ST_FIXH ) continue;
        FOR_ALL_CONNECTIONS( st, cn )
        {
            if( (cn->to->fixed & ST_FIXH) &&
                    (cn->flag & CN_AZ ) &&
                    (cn->flag & CN_DS ) )
            {
                start = st;
                start_ro_id = cn->az.ro_id;
                break;
            }
        }
        if( start ) break;
    }

    if( !start ) return 0;  /* No go... */

    /* Now build out from here.  Connect to each station in turn which has an
       azimuth and a reverse azimuth from the start station.  Main loop
       goes through all traverse id's*/

    start->traverse = 1;
    start->travwgt = 1.0;

    current_id = 1;
    fixed_stns = 0;

    for(;;)
    {
        /* Find all stations that can be linked to the traverse from the
           last fix */
        new_count = 0;

        FOR_ALL_STATIONS(st)
        {
            if( st->traverse != current_id ) continue;
            if( st->fixed & ST_FIXH ) continue;  /* Control station - stop here! */
            /* Work out azimuth corrections for each ro at the station (usually
               only 1).  If this is the first station, then use the ro linked
               to the fixed station.  Otherwise base on the coordinates of
               previously fixed stations */

            if( current_id == 1 )
            {
                nro = 1;
                ro_id[0] = start_ro_id;
                ro_corr[0] = 0.0;
            }

            else
            {
                nro = 0;
                FOR_ALL_CONNECTIONS( st, cn )
                {
                    if( (cn->to->traverse == current_id - 1 ) &&
                            (cn->flag & CN_AZ) )
                    {
                        azcorr = bearing( st->travxy, cn->to->travxy ) - cn->az.az;
                        for( iro = 0; iro < nro; iro++ )
                        {
                            if( ro_id[iro] == cn->az.ro_id ) break;
                        }
                        if( iro == nro && iro < maxro )
                        {
                            nro++;
                            ro_id[iro] = cn->az.ro_id;
                            ro_corr[iro] = azcorr;
                            ro_wgt[iro] = 1.0;
                        }
                        else if( iro < nro )
                        {
                            double ac;
                            ac = ro_corr[iro]/ro_wgt[iro];
                            azcorr -= ac;
                            while( azcorr < -PI ) azcorr += TWOPI;
                            while( azcorr > PI ) azcorr -= TWOPI;
                            azcorr += ac;
                            ro_corr[iro] += azcorr;
                            ro_wgt[iro]++;
                        }
                    }
                }
                for( iro = 0; iro < nro; iro++ )
                {
                    ro_corr[iro] /= ro_wgt[iro];
                }
            }
            /* Sorted out the ro corrections - now assign coordinates to new
               adjacent points */
            FOR_ALL_CONNECTIONS(st,cn)
            {
                if( !cn->to->traverse &&
                        (cn->flag & CN_AZ) &&
                        (cn->flag & CN_DS) )
                {
                    double az, ds, wgt;
                    for( iro = 0; iro < nro; iro++ )
                    {
                        if( cn->az.ro_id == ro_id[iro] ) break;
                    }
                    if( iro >= nro ) continue;
                    az = cn->az.az + ro_corr[iro];
                    ds = cn->ds.ds;
                    wgt = (cn->ds.type == DS_SLP) ? SLP_DIST_FACTOR : 1.0;
                    cn->to->travxy[0] += wgt * (st->travxy[0] + ds*sin(az));
                    cn->to->travxy[1] += wgt * (st->travxy[1] + ds*cos(az));
                    cn->to->travwgt += wgt;
                    if( cn->to->traverse == 0 )
                    {
                        cn->to->traverse = current_id+1;
                        new_count++;
                    }
                }
            }
        }

        /* If we didn't connect any new stations to the traverse, then
           break out.. */

        if( !new_count ) break;

        /* Otherwise calculate the coordinates of the new stations.... */

        current_id++;
        FOR_ALL_STATIONS( st )
        {
            if( st->traverse == current_id )
            {
                st->travxy[0] /= st->travwgt;
                st->travxy[1] /= st->travwgt;
                if( st->fixed & ST_FIXH )
                {
                    st->traverse = -1;
                }
            }
        }
    }

    /* Fixed stations have traverse id -1.  First define a vertical direction
       to set up a traverse projection coordinate system */

    xyz[0] = xyz[1] = xyz[2] = 0;
    fixed_stns = 0;
    FOR_ALL_STATIONS(st)
    {
        int i;
        if( st->traverse != -1 ) continue;
        for( i = 0; i < 3; i++ ) xyz[i] += st->st->XYZ[i];
        fixed_stns++;
    }
    if( fixed_stns < 2 ) return 0;

    for( i = 0; i < 3; i++ ) xyz[i] /= fixed_stns;
    setup_plane_projection( xyz );

    /* Find the centroid of the fixed stations actual and traverse coords */

    ca[0] = ca[1] = 0.0;
    ct[0] = ct[1] = 0.0;
    FOR_ALL_STATIONS( st )
    {
        if( st->traverse != -1 ) continue;
        calc_projection_coords( st );
        ca[0] += st->xy[0]; ca[1] += st->xy[1];
        ct[0] += st->travxy[0]; ct[1] += st->travxy[1];
    }
    ca[0] /= fixed_stns;
    ca[1] /= fixed_stns;
    ct[0] /= fixed_stns;
    ct[1] /= fixed_stns;

    sc = ss = rr = 0.0;
    FOR_ALL_STATIONS( st )
    {
        double dxa, dya, dxt, dyt;
        if( st->traverse != -1 ) continue;
        dxa = st->xy[0] - ca[0];
        dya = st->xy[1] - ca[1];
        dxt = st->travxy[0] - ct[0];
        dyt = st->travxy[1] - ct[1];
        sc += dxa*dxt + dya*dyt;
        ss += dxa*dyt - dxt*dya;
        rr += dxt*dxt + dyt*dyt;
    }
    sc /= rr;
    ss /= rr;

    /* Finally apply correction to all coordinates */

    maxerr = 0.0;
    nfixed = 0;
    PRINTLOG(LOGFIX,("\nLocating stations by traverse\n"));
    FOR_ALL_STATIONS( st )
    {
        double dxt, dyt;
        if( ! st->traverse ) continue;
        dxt = st->travxy[0] - ct[0];
        dyt = st->travxy[1] - ct[1];
        st->travxy[0] = ca[0] + sc * dxt + ss * dyt;
        st->travxy[1] = ca[1] + sc * dyt - ss * dxt;
        if( st->traverse == -1 )
        {
            double err;
            err = distance( st->travxy, st->xy );
            PRINTLOG(LOGFIX,("Error at traverse control station %s is %.1lf\n",st->code,err));
            if( err > maxerr ) maxerr = err;
        }
    }
    PRINTLOG(LOGFIX,("Maximum error at traverse control stations %.1lf metres\n",maxerr));

    {
        char msg[80];
        sprintf(msg,"Fix %d stations by traverse",fixed_stns);
        if( !confirm_fix(msg) ) return 0;
    }

    FOR_ALL_STATIONS( st )
    {
        if( st->traverse > 0 )
        {
            plane_to_geodetic( st->travxy, &lt, &ln );
            PRINTLOG(LOGFIX,("Fixing %s by traverse\n",st->code));
            fix_station( st, lt, ln, 0.0, ST_FIXH );
            nfixed++;
        }
    }

    return nfixed;
}



/***********************************************************/
/* Fix unknown stations                                    */

static int fix_unknown_stations( void )
{
    stn *st;
    int fixed, fixtype;
    double lat, lon, hgt;

    while( confirm_status != CONFIRM_QUIT )
    {
        st = fix_list[0];

        /* Have we fixed everything we can... */

        if( (st->fixed & ST_FIXHV) == ST_FIXHV ) break;
        if( st->bestfix < MIN_FIX_QUALITY ) break;

        fixtype = st->besttype;

        /* For less robust methods see if we can fix stations by traverse first */

        if( fixtype >= FIX_BY_TRAVERSE && fix_by_traverse() ) continue;

        fixed = 0;
        lat = lon = hgt = 0.0;
        switch ( fixtype )
        {
        case FIX_GPS_PT:
            fixed = fix_with_gps_point( st, &lat, &lon, &hgt, &fixtype );
            break;

        case FIX_GPS_HV:
        case FIX_GPS_H:
            fixed = fix_with_gps( st, &lat, &lon, &hgt, &fixtype );
            break;

        case FIX_LV_V:
            fixed = fix_with_hgtdif( st, &lat, &lon, &hgt );
            break;

        case FIX_RAZDS_H:
            fixed = fix_with_azds( st, &lat, &lon, &hgt );
            break;

        case FIX_2RAZ_H:
            fixed = fix_with_azaz( st, &lat, &lon, &hgt );
            break;

        case FIX_2DS_H:
            fixed = fix_with_dsds( st, &lat, &lon, &hgt );
            break;

        case FIX_2AZDS_H:
            fixed = fix_with_dshaha( st, &lat, &lon, &hgt );
            break;

        case FIX_3AZ_H:
            fixed = fix_by_resection( st, &lat, &lon, &hgt );
            break;
        }

        /* If cannot fix, then downgrade quality and try again with
           (probably) next fixable station */

        if( !fixed )
        {
            PRINTLOG(LOGFIX,("Failed to fix station\n"));
            st->fix_quality[fixtype] /= 2.0;
            update_fix_order( st );
            continue;
        }

        fix_station( st, lat, lon, hgt, fix_effect[fixtype] );

    }
    return 0;
}




/*=====================================================================*/

static int print_unfixed_stations( FILE *out, char status, char mask, const char *prompt)
{
    stn *st;
    int lineno = 0;
    int nbad = 0;
    status &= mask;
    FOR_ALL_STATIONS( st )
    {
        if( (st->fixed & mask) != status ) continue;
        if( !lineno )
        {
            fprintf(out,"\n%s\n",prompt);
        }
        if( lineno == 6 )
        {
            fprintf(out,"\n");
            lineno = 0;
        }
        fprintf(out,"  %-10s",st->code);
        lineno++;
        nbad++;
    }
    if( lineno ) fprintf(out,"\n");
    return nbad;
}


static int check_fixed_stn( station *st )
{
    stn *s;
    if( !st->hook ) return 1;
    s = (stn *) (st->hook);
    return (s->fixed & ST_FIXH) ? 1 : 0;
}

/*=====================================================================*/
/* Get the location of the network                                     */

static char inrec[256];
static char * crdfname = 0;
static char *logname = 0;
static int gotroot = 0;
static char newcrdfile = 0;

static FILE *logfile = NULL;


static void set_logname( const char *name )
{
    int l;
    if( logname ) return;
    l = path_len(name,1);
    logname=(char *) check_malloc(strlen(name)+4+1);
    strncpy( logname, name, l );
    strcpy( logname+l,".lst" );
}


static void list_coordsys_codes()
{
    int i, nlines;
    printf("\nValid coordinate system codes are:\n");
    nlines = 0;
    for( i = 0; i < coordsys_list_count(); i++ )
    {
        if( nlines == 20 )
        {
            printf("    Press enter to continue: ");
            if( !fgets(inrec,256,stdin) || inrec[0] != '\n' ) break;
            nlines = 0;
        }
        printf("  %-10s  %s\n",coordsys_list_code(i),coordsys_list_desc(i));
        nlines++;
    }
    printf("\n");
}

static int get_net_coordsys( void )
{
    coordsys *cs;
    for(;;)
    {
        char *code;
        printf("\nEnter the coordinate system code or ?: ");
        if( !fgets(inrec,256,stdin) ) exit(0);
        code = strtok( inrec, " \t\n");
        if( !code ) return 0;
        if( strcmp(code,"?") == 0 )
        {
            list_coordsys_codes();
            continue;
        }
        _strupr(code);
        cs = load_coordsys( code );
        if( is_geocentric(cs) )
        {
            delete_coordsys( cs );
            cs = NULL;
            printf("Cannot use a geocentric (XYZ) coordinate system\n");
        }
        if( cs ) break;
    }
    set_network_coordsys( net, cs, 0.0, 0, 0, 0 );
    delete_coordsys( cs );
    return 1;
}


static int get_option( const char *prompt, int dflt )
{
    char *s;
    for(;;)
    {
        printf("%s",prompt);
        if( ! fgets(inrec,256,stdin) ) exit(0);
        s = strtok( inrec, " \t\n");
        if( !s ) break;
        if( *s == 'y' || *s == 'Y' ) return 1;
        if( *s == 'n' || *s == 'N' ) return 0;
    }
    return dflt;
}

// #pragma warning ( disable : 4127 )

static int add_stations( void )
{
    coordsys *csfrom;
    coord_conversion cnv;
    int geodetic;
    station *st;
    char code[11];
    int nnew;
    double llh[3];

    nnew = 0;
    csfrom = related_coordsys( net->crdsys, CSTP_GEODETIC );
    define_coord_conversion( &cnv, net->crdsys, csfrom );
    geodetic = is_geodetic( net->crdsys );

    for(;;)
    {
        printf("\n     Enter station id (blank to quit): " );
        if( !fgets(inrec,256,stdin) ) exit(0);
        if( sscanf(inrec,"%10s",code) != 1 ) break;
        _strupr(code);
        if( find_station( net, code ))
        {
            printf("     Station %s already defined\n",code);
            continue;
        }

        if( geodetic )
        {
            for(;;)
            {
                int deg, min;
                double sec;
                char hem[2];
                printf("     Enter station latitude (ddd mm ss.ss N|S):  ");
                if( !fgets(inrec,256,stdin) ) continue;
                if( sscanf(inrec,"%d%d%lf%1s",&deg,&min,&sec,hem) != 4 ||
                        deg < 0 || deg > 90 || min < 0 || min > 60
                        || sec < 0.0 || sec > 60.0 ||
                        (hem[0] != 's' && hem[0] != 'S' && hem[0] != 'n' && hem[0] != 'N'))
                {
                    printf("     Invalid input\n");
                    continue;
                }
                sec = (deg + min/60.0 + sec/3600.0)*DTOR;
                if( hem[0] == 's' || hem[0] == 'S' ) sec = - sec;
                llh[CRD_LAT] = sec;
                break;
            }

            for(;;)
            {
                int deg, min;
                double sec;
                char hem[2];
                printf("     Enter station longitude (ddd mm ss.ss E|W): ");
                if( !fgets(inrec,256,stdin) ) continue;
                if( sscanf(inrec,"%d%d%lf%1s",&deg,&min,&sec,hem) != 4 ||
                        deg < 0 || deg > 180 || min < 0 || min > 60
                        || sec < 0.0 || sec > 60.0 ||
                        (hem[0] != 'e' && hem[0] != 'E' && hem[0] != 'w' && hem[0] != 'W'))
                {
                    printf("     Invalid input\n");
                    continue;
                }
                sec = (deg + min/60.0 + sec/3600.0)*DTOR;
                if( hem[0] == 'w' || hem[0] == 'W' ) sec = - sec;
                llh[CRD_LON] = sec;
                break;
            }
        }
        else
        {
            for(;;)
            {
                printf("     Enter station easting: " );
                if( !fgets(inrec,256,stdin) || sscanf(inrec,"%lf",&llh[CRD_EAST]) != 1 ) continue;
                break;
            }
            for(;;)
            {
                printf("     Enter station northing: " );
                if( !fgets(inrec,256,stdin) || sscanf(inrec,"%lf",&llh[CRD_NORTH]) != 1 ) continue;
                break;
            }
        }
        for(;;)
        {
            printf("     Enter station elevation: " );
            if( !fgets(inrec,256,stdin) || sscanf(inrec,"%lf",&llh[CRD_HGT]) != 1 ) continue;
            break;
        }
        if( ! get_option("     Is this all correct? Y/N: ",1) ) continue;
        if( ! geodetic )
        {
            if( convert_coords( &cnv, llh, NULL, llh, NULL ) != OK )
            {
                printf("     Cannot convert these coordinates to latitude and longitude\n");
                continue;
            }
        }
        st = new_network_station( net, code, code, llh[CRD_LAT], llh[CRD_LON],
                                  llh[CRD_HGT], 0.0, 0.0, 0.0 );
        if( add_known_station( st ) ) nnew++;
    }
    delete_coordsys( csfrom );
    return nnew;
}



static void load_interactively( void )
{
    char fname[80];
    DATAFILE *d;

    printf("\nDAT2SITE requires a SNAP coordinate file and one or more SNAP data files\n\n");
    printf("\n=============================================================\n");

    net = new_network();
    for(;;)
    {
        printf("\nEnter input coordinate file name: ");
        if( !fgets(inrec,256,stdin) || sscanf(inrec,"%79s",crdfname) != 1 ) exit(0);
        if( !file_exists(crdfname) )
        {
            printf("File %s does not exist\n",crdfname);
            if( get_option("Do you want to create a new coordinate file? Y/N: ",0) &&
                    get_net_coordsys() )
            {
                set_network_name( net, "New network" );
                newcrdfile = 1;
                break;
            }
            printf("Cannot open file %s\n",crdfname);
        }
        else
        {
            if( read_network(net,crdfname,0) == OK ) break;
            printf("Error reading coordinate file %s\n",crdfname);
        }
    }
    add_network_stations();

    /* Load the data ... */

    set_logname( crdfname );

    printf("\n=============================================================\n");
    printf("\nEnter the names of the data files\n");
    printf("\nAfter the last file enter a blank line to start calculating coordinates\n");

    for(;;)
    {
        printf("\nEnter the SNAP data file name: ");
        if( !fgets(inrec,256,stdin) || sscanf(inrec,"%79s",fname) != 1 ) break;
        d = df_open_data_file( fname, "SNAP data file" );
        if( d )
        {
            read_snap_data( d, 0 );
            df_close_data_file( d );
        }
    }
}


void set_recalc_list()
{
    int istn, maxstns;
    maxstns = number_of_stations( net );
    for( istn = 1; istn <= maxstns; istn++ )
    {
        get_station( station_ptr(net,istn)->Code );
    }
    close_station_list();
}

static void load_data_files( char *coord_file, char **data_files, int ndatafiles,
                             int recalconly )
{
    DATAFILE *d;
    const char *f;
    f = NULL;
    if( gotroot ) f = find_file( coord_file, 0, 0, FF_TRYALL, 0 );
    if( !f  ) f = coord_file;
    crdfname=copy_string(f);

    net = new_network();
    if( read_network(net,crdfname,0) != OK )
    {
        printf("Error reading coordinate file %s\n",crdfname);
        exit(0);
    }
    add_network_stations();

    if( recalconly ) set_recalc_list();

    set_logname( crdfname );

    for( ; ndatafiles-- > 0 ; data_files++ )
    {
        f = find_file( *data_files, 0, 0, FF_TRYALL, 0 );
        if( !f ) f = *data_files;
        d = df_open_data_file( f, "SNAP data file" );
        if( d )
        {
            read_snap_data( d, 0 );
            df_close_data_file( d );
        }
    }
}

static int read_recode( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    if( ! stnrecode ) stnrecode=create_stn_recode_map( net );
    if( read_station_recode_definition( stnrecode, string, cfg->name ) != OK )
    {
        send_config_error(cfg,INVALID_DATA,"Errors encountered in recode command" );
    }
    return OK;
}

static int read_include_file( CFG_FILE *cfg, char *string, void *value, int len, int code );

static config_item snap_commands[] =
{
    {"coordinate_file",NULL,CFG_ABSOLUTE,0,load_coordinate_file,CFG_REQUIRED, 0},
    {"add_coordinate_file",NULL,CFG_ABSOLUTE,0,add_coordinate_file, 0, 0 },
    {"output_coordinate_file",NULL,CFG_ABSOLUTE,0,set_output_coordinate_file,CFG_ONEONLY, 0},
    {"data_file",NULL,CFG_ABSOLUTE,0,load_data_file,CFG_REQUIRED,1},
    {"recode",NULL,CFG_ABSOLUTE,0,read_recode,0,1},
    {"include",NULL,CFG_ABSOLUTE,0,read_include_file,0,0},
    {NULL}
};


static void load_command_file( const char *cmd_file, int recalconly, int included )
{
    CFG_FILE *cfg;
    const char *f;
    char *cfgfile;
    int tryopt=FF_TRYLOCAL;
    int sts;

    if( included ) tryopt |= FF_TRYPROJECT;

    f = find_file( cmd_file, DFLTCOMMAND_EXT2, 0, tryopt, 0 );
    if( !f ) f = find_file( cmd_file, DFLTCOMMAND_EXT, 0, tryopt, 0 );
    if( !f ) f = cmd_file;
    cfgfile=copy_string(f);

    cfg = open_config_file( cfgfile, COMMENT_CHAR );
    if( ! included ) set_logname( cfgfile );

    if(cfg)
    {
        // int pl=path_len(f,0);
        // char *pdir=pl ? copy_string_nch(f,pl) : 0;
        // if( pdir )
        // {
        //     set_project_dir( pdir );
        //     check_free(pdir);
        // }
        int options=included ? 0 : CFG_CHECK_MISSING; 
        options |= (CFG_IGNORE_BAD | CFG_SET_PATH);
        set_config_read_options( cfg, options );
        sts = read_config_file( cfg, snap_commands );
        close_config_file( cfg );
    }
    else
    {
        printf("\n\nCannot open command file %s\n",f);
        exit(0);
    }
    if( sts != OK )
    {
        printf("\n\nErrors encountered reading command file\n");
        exit(0);
    }
    if( ! included )
    {
        if( ! net || ! survey_data_file_count() )
        {
            printf("\n\nCoordinate or data file not found in configuration file\n");
            exit(0);
        }
        add_network_stations();
        if( recalconly ) set_recalc_list();
        read_data_files( stdout );
        crdfname=copy_string( station_filename );
        delete_survey_file_list();
    }
    check_free(cfgfile);
}

static int read_include_file( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *s;
    s = strtok(string," \t\n");
    if( !s ) return MISSING_DATA;
    load_command_file( s,  0, 1 );
    return OK;
}


static void printlog( const char *fmt, ... )
{
    va_list argptr;
    va_start(argptr, fmt);
    if( logfile ) { vfprintf(logfile, fmt, argptr); fflush(logfile); }
    va_end(argptr);
    va_start(argptr, fmt);
    vfprintf(stdout, fmt, argptr);
    va_end(argptr);
}

int main( int argc, char *argv[] )
{
    int i;
    int interactive;
    int syntax_error;
    int nfilelist;
    int recalc;
    int command_file;
    char **filelist;
    int nrecalclist;
    char **recalclist;
    char *outputfile = NULL;


    errcount = 0;
    errlog = stdout;

    interactive = 0;
    recalc = 0;
    syntax_error = 0;

    filelist = (char **) malloc( sizeof(char*)*argc );
    recalclist = (char **) malloc( sizeof(char*)*argc );
    if( !filelist || !recalclist )
    {
        printf("Not enough memory for program\n"); return 0;
    }

    nfilelist = 0;
    nrecalclist = 0;
    command_file = 1;

    for( i = 1; i < argc; i++ )
    {
        char *arg = argv[i];
        if( arg[0] == '-' )
        {
            switch ( arg[1] )
            {
            case 'c': case 'C': confirm_status = CONFIRM_FIXES; break;
            case 'f': case 'F': command_file = 0; break;
            case 'r': case 'R': recalc = 1; break;
            case 'l': case 'L': listonly = 1; break;
            case 'm': case 'M': listonly = 2; break;
            case 'u': case 'U': userejected = 1; break;
            case 'o': case 'O': i++;
                if( i > argc ) { syntax_error = 1; }
                else { outputfile = argv[i]; }
                break;
            default: syntax_error = 1; break;
            }
        }
        else if ( recalc )
        {
            recalclist[nrecalclist++] = arg;
        }
        else
        {
            filelist[nfilelist++] = arg;
        }
    }

    if( nfilelist == 0 ) interactive = 1;
    if( recalc && (interactive || !nrecalclist) ) syntax_error = 1;
    if( command_file && nfilelist > 1 ) syntax_error = 1;
    if( !command_file && nfilelist < 2 ) syntax_error = 1;

    if( interactive || syntax_error )
    {
        printf("\nDAT2SITE: Calculates trial coordinates using observational data\n\n");
    }

    if( syntax_error )
    {
        printf("Syntax:  dat2site options\n");
        printf("    or   dat2site  options command_file [-r stations...]\n");
        printf("    or   dat2site  options -f  coord_file  data_file data_file ... -r [stations]\n\n");
        printf("The first options loads files interactively\n");
        printf("The second reads the coordinate and data file names from the command file\n");
        printf("The third option allows the coordinate and data file names to be\n");
        printf("specified on the command line\n\n");
        printf("\nAvailable options are:\n");
        printf("    -c   confirm each calculation before continuing\n");
        printf("    -r   is followed by a list of stations to recalculate - others are unchanged\n");
        printf("    -l   just lists all the stations without doing calculations\n");
        printf("    -m   just list missing stations without doing calculations\n");
        printf("    -u   use rejected or ignored stations\n");
        printf("    -o filename  specifies a filename for station lists\n");
        return 0;
    }

    stnlist = create_list( sizeof(stn) );

    init_snap_globals();
    install_default_crdsys_file();

    /* Load the coordinate file */

        init_load_data( load_data, get_id, get_name, get_value );

    if( interactive )
    {
        load_interactively();
    }

    else if( command_file )
    {
        set_snap_command_file( filelist[0] );
        load_command_file( filelist[0], recalc, 0 );
    }
    else
    {
        load_data_files( filelist[0], filelist+1, nfilelist-1, recalc );
    }

    close_station_list();

    if( ! list_count(stnlist) )
    {
        printf("\nNo data has been loaded\n");
        return 0;
    }

    if( listonly )
    {
        FILE *out;
        stn *st;
        if( outputfile )
        {
            out = fopen(outputfile,"w");
            if( ! out )
            {
                printf("\nCannot open output file %s\n",outputfile);
                return 0;
            }
        }
        else
        {
            printf("%s stations in data files:\n",listonly == 1 ? "All" : "Missing");
            out = stdout;
        }

        FOR_ALL_STATIONS(st)
        {
            char *code = st->code;
            if( listonly > 1 )
            {
                if( find_station(net,code) ) continue;
            }
            fprintf(out,"%s\n",code);
        }

        if( outputfile ) fclose(out);
        return 0;
    }

    if( logname )
    {
        logfile = fopen( logname, "w" );
    }
    if( logfile )
    {
        fprintf(logfile,"DAT2SITE log file\n");
        fprintf(logfile,"\nAdding coordinates to file %s\n\n",crdfname );
    }

    /* Calculate height differences where available, and convert
       slope distances to horizontal... */

    fixup_all_zdds();

    /* Use reverse angles to link angle observations to common ro's.
       This ignores the difference between forward and reverse azimuths! */

    max_ro_diff = 0.0;
    resolve_ha_ros();
    if( max_ro_diff > 0.0 )
    {
        PRINTLOG(LOGFIX,("Maximum error resolving horizontal angle RO's %.1f deg",
                         (double)(max_ro_diff*RTOD) ));
    }

    /* Setup the list of stations in the order that they will be fixed -
       the preferred stations is always the first in this list */

    setup_fix_order();

    /* Fix the stations in the input coordinate file */

    fix_known_stations( recalclist, nrecalclist );

    /* Set up flags for potential point fixes */

    setup_point_fixes();

    do
    {

        /* Now look at fix unknown stations */

        fix_unknown_stations();

        /* Count the unfixed stations */

        if( interactive ) printf("\n=============================================================\n");
        i = print_unfixed_stations( stdout, 0, ST_FIXH,
                                    "The following stations have not been fixed");

        if( interactive ) interactive = i;
        if( confirm_status == CONFIRM_QUIT ) interactive = 0;
        if( interactive )
        {
            printf("\nEnter coordinates of one or more of these stations\n");
            printf("DAT2SITE will then attempt to calculate the rest\n");
            interactive = add_stations();
        }
    }
    while( interactive );

    print_unfixed_stations( stdout, ST_FIXH, ST_FIXH | ST_FIXV,
                            "The following stations have been fixed horizontally only");

    if( logfile )
    {
        print_unfixed_stations( logfile, 0, ST_FIXH,
                                "The following stations have not been fixed");
        print_unfixed_stations( logfile, ST_FIXH, ST_FIXH | ST_FIXV,
                                "The following stations have been fixed horizontally only");
    }

    /* Write the new coordinate file */

    if( ! newcrdfile )
    {
        i = path_len( crdfname, 1 );
        if( i < 75 ) strcpy(crdfname+i,".new");
    }

    write_station_file( "dat2site", 0, 0, 0, 0, 1 );

    printf("\nUpdated coordinates written to %s\n", crdfname );

    if( logfile )
    {
        fclose( logfile );
        printf("\nLog file written to %s\n\n",logname );
    }

    return 0;
}

