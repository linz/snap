#include "snapconfig.h"
/*
   $Log: plotstns.c,v $
   Revision 1.2  1996/07/12 20:33:24  CHRIS
   Modified to support hidden stations.

   Revision 1.1  1996/01/03 22:29:00  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "util/geodetic.h"
#include "util/chkalloc.h"
#include "util/dms.h"
#include "util/pi.h"

#include "util/binfile.h"
#include "snap/stnadj.h"
#include "snap/snapglob.h"  /* For dimension, coord_precision */
#include "plotstns.h"
#include "plotpens.h"
#include "plotfunc.h"
#include "plotscal.h"
#include "coordsys/tmprojr.h"

/* Structure used to hold plotting information about a station */

typedef struct
{
    double easting, northing;
    float de, dn, dh;
    float e_offset, n_offset;
    int symbol;  /* Actually usage!! */
    unsigned char flags;
} stn_plot_s;

typedef struct
{
    double emax, emin, az;
    double sehgt;
} covariance;

typedef union
{
    int iValue;
    float fValue;
    const char *cPtr;
} sortobj;

#define BUF_SIZE 1024
#define MAXCOLWIDTH 50

static char arbitrary_projection;  /* Set to 1 if coords were geodetic */
static void *latfmt = NULL;               /* Format for printing coordinates if coord system is lat/lon */
static void *lonfmt = NULL;
static stn_plot_s *stns = NULL;    /* Array of station plot information */
static int *xyindex = NULL;      /* Array of pointers sorted by easting */
static char xyindex_valid = 0;     /* True when the index is valid */
static long xyindex_version = 0;   /* Incremented at each change of index */
static int *sortIndex = NULL;    /* Array of indices of non-ignored stations */
static sortobj *sortValues = NULL; /* Array of values used for sorting */
static int *slist_field; /* Array station list fields */
static int slist_ncols = 0;
static char slist_buf[BUF_SIZE];
static char indexCol = STNF_CODE;        /* Can be set to index codes or names */
static int no_good_stations = 0;     /* Number of non-ignored stations */
static int nadjust3=0;               /* Counts of stations adjusted */
static int nadjusth=0;
static int nadjustv=0;
static covariance *covar = NULL;   /* Array of covariances */
static double nmax, nmin, emax, emin;   /* The range of coordinates */
static int stations_adjusted;
static int nstns;
static int nhighlight;
static int noffset;
static char apply_station_offsets = 0;
static double cvrmaxh = 0.0;
static double cvrmaxv = 0.0;
static double adjmaxh = 0.0;
static double adjmaxv = 0.0;
static coordsys *plot_crdsys = NULL;

static int stn_colourby_class = 0;
static int symbol_pen[N_STN_SYM];
static int symbol_opt[N_STN_SYM];

void set_plot_projection( coordsys *cs )
{
    plot_crdsys = cs;
}

coordsys *plot_projection()
{
    return plot_crdsys;
}

char geodetic_coordsys()
{
    return arbitrary_projection;
}

void format_plot_coords( double e, double n, char *buf )
{
    if( ! buf || ! plot_crdsys ) return;

    buf[0] = 0;
    if( using_station_offsets() && offset_station_count() )
    {
        strcpy(buf,"Note: Some stations are offset\r\n");
        buf += strlen(buf);
    }
    if( arbitrary_projection && latfmt && lonfmt )
    {
        double lat, lon;
        proj_to_geog( plot_crdsys->prj, e, n, &lon, &lat );
        dms_string( lat*RTOD, latfmt, buf );
        buf += strlen(buf);
        strcpy(buf,"\r\n");
        buf += 2;
        dms_string( lon*RTOD, lonfmt, buf+strlen(buf) );
    }
    else
    {
        sprintf(buf, "%12.3lf\r\n%12.3lf", e, n );
    }
}

char *plot_crdsys_name()
{
    return plot_crdsys->name;
}

/* Calculate the axes of an error ellipse given the elements of a
   covariance matrix.  Assumes that the covariance matrix is lower
   triangular for E,N coordinates.  Returns the size and orientation
   of the error ellipse.  The orientation is measured from N through
   E */


void calc_error_ellipse( double cvr[], double *emax, double *emin, double *azemax )
{
    double v1, v2, v3, v4;

    v1 = (cvr[2]+cvr[0])/2.0;
    v2 = (cvr[2]-cvr[0])/2.0;
    v3 = cvr[1];
    v4 = v2*v2+v3*v3;
    if( v4 > 0.0 ) v4 = sqrt(v4);
    *azemax = v4 > 0.0 ? atan2( v3, v2 ) / 2.0 : 0.0 ;
    v2 = v1-v4;
    v1 = v1+v4;
    *emax = v1 > 0.0 ? sqrt(v1) : 0.0;
    *emin = v2 > 0.0 ? sqrt(v2) : 0.0;
}

/* Stations as used for plotting */

static int cmp_stn_easting( const void *sp1, const void *sp2 )
{
    double de;
    int i1 = * (int *) sp1;
    int i2 = * (int *) sp2;
    de = stns[i1].easting - stns[i2].easting;
    if( apply_station_offsets ) { de += stns[i1].e_offset - stns[i2].e_offset; }
    return de > 0.0 ? 1 : de < 0.0 ? -1 : 0;
}

static int station_flag_status_id( stn_adjustment *sa )
{
    int statusid = 0;
    if( sa->flag.rejected )
    {
        if( sa->flag.autoreject )
        {
            statusid = 2;
        }
        else
        {
            statusid = 1;
        }
    }
    else
    {
        int inc = 1;
        int inc2 = 0;
        statusid = 3;
        if( dimension != 2 )
        {
            inc = inc2 = 3;
            if( sa->flag.float_v )
            {
                statusid++;
            }
            else if( sa->flag.adj_v )
            {
                statusid += 2;
            }
        }
        if( dimension != 1 )
        {
            statusid += 3 + inc2;
            if( sa->flag.float_h )
            {
                statusid += inc;
            }
            else if( sa->flag.adj_h )
            {
                statusid += (inc+inc);
            }
        }
    }
    return statusid;
}

static const char *station_flag_status( stn_adjustment *sa )
{
    const char *statusnames[] =
    {
        "",
        "Rejected",
        "Rejected-auto",
        "Fix",
        "Float V",
        "Adjust V",
        "Fix",
        "Float H",
        "Adjust H",
        "Fix",
        "Fix H Float V",
        "Fix H Adjust V",
        "Float H Float V",
        "Float HV",
        "Float H Adjust V",
        "Adjust H Fix V",
        "Adjust H Float V",
        "Adjust HV"
    };
    int statusid = station_flag_status_id(sa);
    return statusnames[ statusid ];
}


static int cmp_sortobj_ivalue( const void *sp1, const void *sp2 )
{
    int s1 = (*(int *) sp1 );
    int s2 =  (*(int *) sp2 );
    return sortValues[s1].iValue - sortValues[s2].iValue;
}

static int cmp_sortobj_fvalue( const void *sp1, const void *sp2 )
{
    int s1 = (*(int *) sp1 );
    int s2 =  (*(int *) sp2 );
    float diff = sortValues[s1].fValue - sortValues[s2].fValue;
    return diff < 0 ? -1 : diff > 0 ? 1 : 0;
}

static int cmp_sortobj_reverse_fvalue( const void *sp1, const void *sp2 )
{
    return cmp_sortobj_fvalue( sp2, sp1 );
}

/*
static int cmp_sortobj_cptr( const void *sp1, const void *sp2 )
{
    int s1 = (*(int *) sp1 );
    int s2 =  (*(int *) sp2 );
    return _stricmp(sortValues[s1].cPtr,sortValues[s2].cPtr);
}
*/

static int cmp_sortobj_code( const void *sp1, const void *sp2 )
{
    int s1 = (*(int *) sp1 );
    int s2 =  (*(int *) sp2 );
    return stncodecmp(sortValues[s1].cPtr,sortValues[s2].cPtr);
}

static void build_sort_index( void )
{
    int i;
    int classid;
    int valueid;
    station *stn;
    const char *dash = "-";
    double emax, emin, b1, dxyz[3], hgterr;

    if( ! no_good_stations || ! sortIndex || ! sortValues ) return;
    for( i = 0; i < no_good_stations; i++ )
    {
        int istn = sortIndex[i];
        stn = stnptr(istn);

        switch( indexCol )
        {
        case STNF_CODE: sortValues[istn].cPtr = stn->Code; break;
        case STNF_NAME: sortValues[istn].cPtr = stn->Name; break;
        case STNF_LAT:  sortValues[istn].fValue =  stn->ELat; break;
        case STNF_LON:  sortValues[istn].fValue =  stn->ELon; break;
        case STNF_EAST: sortValues[istn].fValue =  stns[istn].easting; break;
        case STNF_NRTH: sortValues[istn].fValue =  stns[istn].northing; break;
        case STNF_HGT:  sortValues[istn].fValue =  stn->OHgt; break;
        case STNF_STS:  sortValues[istn].iValue =  station_flag_status_id(stnadj(stn)); break;
        case STNF_HERR:
            get_error_ellipse( istn, &emax, &emin, &b1 );
            sortValues[istn].fValue = emax;
            break;
        case STNF_HADJ:
            get_station_adjustment( istn, dxyz );
            sortValues[istn].fValue = (dxyz[0]*dxyz[0]+dxyz[1]*dxyz[1]);
            break;
        case STNF_VERR:
            get_height_error( istn, &hgterr );
            sortValues[istn].fValue = hgterr;
            break;
        case STNF_VADJ:
            get_station_adjustment( istn, dxyz );
            sortValues[istn].fValue = fabs(dxyz[2]);
            break;
        default:
            classid = indexCol - STNF_CLASS;
            valueid = get_station_class( stn, classid );
            sortValues[istn].cPtr = network_class_value( net, classid, valueid );
            break;

        }
    }

    switch( indexCol )
    {
    case STNF_CODE:
    case STNF_NAME:
        qsort( sortIndex, no_good_stations, sizeof(int), cmp_sortobj_code );
        break;
    case STNF_STS:
        qsort( sortIndex, no_good_stations, sizeof(int), cmp_sortobj_ivalue );
        break;
    case STNF_LAT:
    case STNF_LON:
    case STNF_EAST:
    case STNF_NRTH:
    case STNF_HGT:
        qsort( sortIndex, no_good_stations, sizeof(int), cmp_sortobj_fvalue );
        break;

    case STNF_HERR:
    case STNF_HADJ:
    case STNF_VERR:
    case STNF_VADJ:
        qsort( sortIndex, no_good_stations, sizeof(int), cmp_sortobj_reverse_fvalue );
        break;
    default:
        qsort( sortIndex, no_good_stations, sizeof(int), cmp_sortobj_code );
    }
}

static void reverse_sort_index()
{
    int i0;
    int i1;

    if( sortIndex && no_good_stations )
    {
        for( i0 = 0, i1 = no_good_stations - 1; i0 < i1; i0++, i1-- )
        {
            int tmp = sortIndex[i0];
            sortIndex[i0] = sortIndex[i1];
            sortIndex[i1] = tmp;
        }
    }
}

/* Code for the station list */

void init_station_list()
{
    int nclass = network_classification_count(net);
    if( slist_field ) check_free( slist_field );
    slist_field = (int *) check_malloc( (STNF_CLASS+1+nclass) * sizeof(int));
    slist_ncols = 0;

    /* No stations loaded yet, or none to list, then return */

    if( ! no_good_stations ) return;
    slist_field[ slist_ncols++] = STNF_CODE;
    if( projection_defined() )
    {
        slist_field[ slist_ncols++] = STNF_EAST;
        slist_field[ slist_ncols++] = STNF_NRTH;
    }
    else
    {
        if( !latfmt )
        {
            latfmt = create_dms_format( 3, 6, 0, NULL, NULL, NULL, "N", "S" );
            lonfmt = create_dms_format( 3, 6, 0, NULL, NULL, NULL, "E", "W" );
        }
        slist_field[ slist_ncols++] = STNF_LAT;
        slist_field[ slist_ncols++] = STNF_LON;
    }
    slist_field[ slist_ncols++] = STNF_HGT;

    if( got_covariances() )
    {
        slist_field[ slist_ncols++] = STNF_STS;
        if( dimension != 1 )
        {
            slist_field[ slist_ncols++] = STNF_HERR;
        }
        if( dimension != 2 )
        {
            slist_field[ slist_ncols++] = STNF_VERR;
        }

        if( dimension != 1 )
        {
            slist_field[ slist_ncols++] = STNF_HADJ;
        }
        if( dimension != 2 )
        {
            slist_field[ slist_ncols++] = STNF_VADJ;
        }
    }
    if( nclass )
    {
        int i;
        for( i = 0; i++ < nclass; )
        {
            slist_field[ slist_ncols++] = STNF_CLASS+i;
        }
    }
    slist_field[ slist_ncols++] = STNF_NAME;
    build_sort_index();
}

char *station_list_header( void )
{
    int icol;
    int classid;
    char *buf = slist_buf;
    *buf = 0;

    for( icol = 0; icol < slist_ncols; icol++ )
    {
        int len = 0;
        int nc = 0;
        if( icol ) {*buf++ = '\t'; *buf = 0; }
        switch( slist_field[icol] )
        {
        case STNF_CODE: strcpy(buf,"Code"); len=12; break;
        case STNF_NAME: strcpy(buf,"Name"); len=50; break;
        case STNF_LAT:  strcpy(buf," Latitude"); len=15; break;
        case STNF_LON:  strcpy(buf," Longitude"); len = 15; break;
        case STNF_EAST:  strcpy(buf," Easting"); len = 13; break;
        case STNF_NRTH:  strcpy(buf," Northing"); len = 13; break;
        case STNF_HGT:  strcpy(buf," Height"); len = 13; break;
        case STNF_STS:    strcpy(buf,"Status"); len = 13;  break;
        case STNF_HERR:  strcpy(buf," Hor.Err"); len = 13; break;
        case STNF_HADJ: strcpy(buf," Hor.Adj"); len = 13; break;
        case STNF_VERR: strcpy(buf," Vrt.Err"); len = 13; break;
        case STNF_VADJ: strcpy(buf," Vrt.Adj"); len = 13; break;
        default:
            classid = slist_field[icol]-STNF_CLASS;
            strncpy(buf,network_class_name(net,classid),MAXCOLWIDTH);
            buf[MAXCOLWIDTH-1] = 0;
            len = 12;
            break;
        }

        // Messy business setting column to desired length, etc ..
        if( *buf == ' ' ) { len++; }
        nc = strlen(buf);
        buf += nc;
        len -= nc;
        // TODO: Fix up header for sorted column  .. requires resetting header without
        // changing column widths ..
        // if( slist_field[icol] == indexCol ) { *buf++ = '*'; len--; }
        while( len-- > 0 ) { *buf++ = ' '; }
        *buf = 0;

        if( strlen( slist_buf ) + MAXCOLWIDTH > BUF_SIZE ) break;
    }

    return slist_buf;
}

static void replace_tabs( char *buf )
{
    char *c;
    for( c =  buf; *c; c++ )
    {
        if( *c == '\t' ) *c = ' ';
    }
}

char *station_list_item( int istnsrt )
{
    int icol;
    int istn;
    int classid;
    int valueid;
    station *stn;
    stn_adjustment *sa;
    double emax, emin, b1;
    double hgterr;
    double dxyz[3];
    char gotadj = 0;
    char *buf = slist_buf;
    int buflen = BUF_SIZE;

    *buf = 0;
    istn = sortIndex[istnsrt];
    stn = stnptr(istn);
    sa = stnadj( stnptr(istn) );

    for( icol = 0; icol < slist_ncols; icol++ )
    {
        if( icol ) {*buf++ = '\t'; *buf = 0; }
        switch( slist_field[icol] )
        {
        case STNF_CODE: strcpy(buf,stn->Code); break;
        case STNF_NAME: strncpy(buf,stn->Name,MAXCOLWIDTH); buf[MAXCOLWIDTH] = 0; replace_tabs(buf); break;
        case STNF_LAT:  dms_string( stn->ELat * RTOD, latfmt, buf ); strcat(buf," "); break;
        case STNF_LON:  dms_string( stn->ELon * RTOD, lonfmt, buf ); strcat(buf," "); break;
        case STNF_EAST: sprintf(buf,"%.*lf ",coord_precision,stns[istn].easting); break;
        case STNF_NRTH: sprintf(buf,"%.*lf ",coord_precision,stns[istn].northing); break;
        case STNF_HGT:  sprintf(buf,"%.*lf ",coord_precision,stn->OHgt); break;
        case STNF_STS:  strcpy(buf,station_flag_status(sa)); break;
        case STNF_HERR:
            get_error_ellipse( istn, &emax, &emin, &b1 );
            emax *= errell_factor;
            sprintf(buf,"%.*lf ",coord_precision,emax);
            break;
        case STNF_HADJ:
            if( ! gotadj ) {get_station_adjustment( istn, dxyz ); gotadj = 1; }
            sprintf(buf,"%.*lf ",coord_precision,_hypot(dxyz[0],dxyz[1]));
            break;
        case STNF_VERR:
            get_height_error( istn, &hgterr );
            hgterr *= hgterr_factor;
            sprintf(buf,"%.*lf ",coord_precision,hgterr);
            break;
        case STNF_VADJ:
            if( ! gotadj ) {get_station_adjustment( istn, dxyz ); gotadj = 1; }
            sprintf(buf,"%.*lf ",coord_precision,dxyz[2]);
            break;
        default:
            classid = slist_field[icol]-STNF_CLASS;
            valueid = get_station_class( stn, classid );
            strncpy(buf,network_class_value(net,classid,valueid),MAXCOLWIDTH);
            buf[MAXCOLWIDTH] = 0;
            replace_tabs(buf);
            break;
        }
        buf += strlen(buf);
        buflen -= strlen(buf);
        if( buflen < MAXCOLWIDTH ) break;
    }

    return slist_buf;
}

void station_item_info( int istnsrt, PutTextInfo *jmp )
{
    jmp->type = ptfStation;
    jmp->type = ptfStation;
    jmp->from = sortIndex[istnsrt];
    jmp->to = 0;
    jmp->obs_id = 0;
}

void list_station_summary( void *dest, PutTextFunc f )
{
    int nch;
    sprintf(slist_buf,"Adjustment includes %d stations",no_good_stations);
    (*f)(dest,NULL,slist_buf);
    if( nadjust3 > 0 )
    {
        sprintf(slist_buf,"%d stations adjusted horizontally and vertically",nadjust3);
        (*f)(dest,NULL,slist_buf);
    }
    if( nadjusth > 0 )
    {
        sprintf(slist_buf,"%d stations adjusted horizontally",nadjusth);
        (*f)(dest,NULL,slist_buf);
    }
    if( nadjustv > 0 )
    {
        sprintf(slist_buf,"%d stations adjusted vertically",nadjustv);
        (*f)(dest,NULL,slist_buf);
    }
    if( got_covariances())
    {
        if( nadjust3 + nadjusth > 0 )
        {
            sprintf(slist_buf,"Maximum horizontal adjustment %.1lf mm",adjmaxh*1000.0);
            (*f)(dest,NULL,slist_buf);

            if( aposteriori_errors )
            {
                strcpy(slist_buf,"Maximum aposteriori ");
            }
            else
            {
                strcpy(slist_buf,"Maximum apriori ");
            }
            nch = strlen(slist_buf);
            if( use_confidence_limit )
            {
                sprintf(slist_buf+nch,"%.2lf%% conf. lim. ",confidence_limit);
            }
            else if( confidence_limit != 1.0 )
            {
                sprintf(slist_buf+nch,"%.lf times ",confidence_limit);
            }
            nch = strlen(slist_buf);
            sprintf(slist_buf+nch," horizontal error %.1lf mm",cvrmaxh* errell_factor * 1000.0);
            (*f)( dest, NULL, slist_buf );
        }

        if( nadjust3+nadjustv > 0 )
        {
            sprintf(slist_buf,"Maximum vertical adjustment %.1lf mm",adjmaxv*1000.0);
            (*f)(dest,NULL,slist_buf);

            if( aposteriori_errors )
            {
                strcpy(slist_buf,"Maximum aposteriori ");
            }
            else
            {
                strcpy(slist_buf,"Maximum apriori ");
            }
            nch = strlen(slist_buf);
            if( use_confidence_limit )
            {
                sprintf(slist_buf+nch,"%.2lf%% conf. lim. ",confidence_limit);
            }
            else if( confidence_limit != 1.0 )
            {
                sprintf(slist_buf+nch,"%.lf times ",confidence_limit);
            }
            nch = strlen(slist_buf);
            sprintf(slist_buf+nch," vertical error %.1lf mm",cvrmaxv* hgterr_factor * 1000.0);
            (*f)( dest, NULL, slist_buf );

        }
    }
}

void list_station_details( void *dest, PutTextFunc f, int istn )
{
    station *stn;
    int nch;
    int nclass;
    double dxyz[3];

    stn = stnptr(istn);
    sprintf( slist_buf,"Station %s: %.50s",stn->Code,stn->Name);
    replace_tabs( slist_buf );
    (*f)( dest, NULL, slist_buf );

    nclass = network_classification_count(net);
    if( nclass > 0 )
    {
        int i;
        (*f)( dest, NULL, "");
        for( i = 0; i++ < nclass; )
        {
            sprintf(slist_buf,"%s: %s",
                    network_class_name(net,i),
                    network_class_value(net,i,get_station_class(stn,i)));
            replace_tabs(slist_buf);
            (*f)(dest, NULL, slist_buf);
        }
    }
    (*f)( dest, NULL, "");

    if( projection_defined() )
    {
        double e, n;
        e = stns[istn].easting;
        n = stns[istn].northing;
        sprintf(slist_buf,"%.4lf   %.4lf",e,n);
    }
    else
    {
        if( !latfmt )
        {
            latfmt = create_dms_format( 3, 6, 0, NULL, NULL, NULL, "N", "S" );
            lonfmt = create_dms_format( 3, 6, 0, NULL, NULL, NULL, "E", "W" );
        }
        dms_string( stn->ELat * RTOD, latfmt, slist_buf );
        nch = strlen(slist_buf);
        strcpy( slist_buf+nch, "   ");
        nch += 3;
        dms_string( stn->ELon * RTOD, lonfmt, slist_buf+nch );
    }
    nch = strlen( slist_buf );
    sprintf(slist_buf+nch,"   %.4lf\n",stn->OHgt );
    (*f)( dest, NULL, slist_buf );

    if( got_covariances() )
    {
        stn_adjustment *sa = stnadj( stn );

        if( sa->flag.rejected )
        {
            strcpy(slist_buf,"Rejected from the adjustment");
            if( sa->flag.autoreject )
            {
                /* Wanted to use strcat, but Borland did something odd with it*/
                char *b = strchr( slist_buf, 0 );
                if( b ) strcpy(b," by SNAP (not enough data)");
            }
        }
        else
        {
            char *b = slist_buf;
            *b = 0;
            if( dimension != 1 )
            {
                if( sa->flag.float_h )
                {
                    sprintf(b,"Floated horizontally (%.4fm)",sa->herror);
                }
                else if( sa->flag.adj_h )
                {
                    strcpy(b,"Adjusted horizontally");
                }
                else
                {
                    strcpy(b,"Fixed horizontally");
                }
                b = strchr(b,0);
                if( dimension != 2 ) {strcpy(b,", "); b+= 2;}
            }
            if( dimension != 2 )
            {
                if( sa->flag.float_v )
                {
                    sprintf(b,"Floated vertically (%.4fm)",sa->verror);
                }
                else if( sa->flag.adj_v )
                {
                    strcpy(b,"Adjusted vertically");
                }
                else
                {
                    strcpy(b,"Fixed vertically");
                }
            }
        }

        (*f)( dest, NULL, slist_buf );

        if( aposteriori_errors )
        {
            strcpy(slist_buf,"Aposteriori ");
        }
        else
        {
            strcpy(slist_buf,"Apriori ");
        }
        nch = strlen(slist_buf);
        if( use_confidence_limit )
        {
            sprintf(slist_buf+nch,"%.2lf%% conf. lim. ",confidence_limit);
        }
        else if( confidence_limit != 1.0 )
        {
            sprintf(slist_buf+nch,"%.1lf times ",confidence_limit);
        }
        nch = strlen(slist_buf);

        if( sa->flag.adj_h && dimension != 1 )
        {
            double b1, b2, emax, emin;
            get_error_ellipse( istn, &emax, &emin, &b1 );
            b1 *= RTOD;
            while( b1 < 0.0 ) b1 += 180.0;
            while( b1 > 180.0 ) b1 -= 180.0;
            b2 = b1 + 90.0;
            if( b2 > 180.0 ) b2 -= 180.0;

            emax *= errell_factor * 1000.0;
            emin *= errell_factor * 1000.0;

            sprintf(slist_buf+nch,"error ellipse %.1lfmm at N%.0lfE, %.1lfmm at N%.0lfE",
                    emax,b1,emin,b2 );
            (*f)( dest, NULL, slist_buf );
        }

        if( sa->flag.adj_v && dimension != 2 )
        {
            double hgterr;
            get_height_error( istn, &hgterr );
            hgterr *= hgterr_factor * 1000.0;
            sprintf(slist_buf+nch,"height error %.1lfmm",hgterr);
            (*f)( dest, NULL, slist_buf );
        }

        get_station_adjustment( istn, dxyz );
        sprintf(slist_buf,"Station adjustment (E,N,Z):  %7.3lfm %7.3lfm %7.3lfm\n",
                dxyz[0], dxyz[1], dxyz[2] );
        (*f)( dest, NULL, slist_buf );
    }
}



void sort_station_list_col( int icol )
{
    if( icol >= 0 && icol < slist_ncols )
    {
        sort_station_list(slist_field[icol]);
    }
}

void sort_station_list( int opt )
{
    if( ! no_good_stations ) return;
    /* If already sorted ... */
    if( sortIndex && indexCol == opt )
    {
        reverse_sort_index();
    }
    else
    {
        indexCol = opt;
        build_sort_index();
    }
}



void init_plotstns( int adjusted )
{
    double cm, rlat, n, e, ds;
    int istn;
    int symbol;
    station *st;
    stn_adjustment *sa;
    coordsys *csdata;
    coord_conversion cnv;

    stations_adjusted = adjusted;

    csdata = related_coordsys( net->crdsys, CSTP_GEODETIC );

    arbitrary_projection = 0;
    if( !plot_crdsys )
    {
        if( is_projection( net->crdsys ) )
        {
            plot_crdsys = copy_coordsys( net->crdsys );
        }
        else
        {
            projection *prj;
            get_network_topocentre( net, &rlat, &cm );
            prj = create_tm_projection( cm, 1.0, rlat, 0.0, 0.0, 1.0 );
            plot_crdsys = create_coordsys( "LTM", net->crdsys->name,
                                           CSTP_PROJECTION, copy_ref_frame( net->crdsys->rf ), prj );
            lonfmt = create_dms_format(3, 6, 0, 0, 0, 0, " E", " W" );
            latfmt = create_dms_format(3, 6, 0, 0, 0, 0, " N", " S" );
            arbitrary_projection = 1;
        }
    }

    if( ! plot_crdsys || ! csdata ||
            define_coord_conversion( &cnv, csdata, plot_crdsys ) != OK )
    {
        handle_error( FATAL_ERROR, "Unable to convert coordinates to plot projection",
                      NO_MESSAGE );
    }

    /* Set up the array of station coordinates and symbol types */

    symbol_opt[FREE_STN_SYM] = FREE_STN_OPT;
    symbol_opt[FIXED_STN_SYM] = FIXED_STN_OPT;
    symbol_opt[HOR_FIXED_STN_SYM] = HOR_FIXED_STN_OPT;
    symbol_opt[VRT_FIXED_STN_SYM] = VRT_FIXED_STN_OPT;
    symbol_opt[REJECTED_STN_SYM] = REJECTED_STN_OPT;

    symbol_pen[FREE_STN_SYM] = FREE_STN_PEN;
    symbol_pen[FIXED_STN_SYM] = FIXED_STN_PEN;
    symbol_pen[HOR_FIXED_STN_SYM] = HOR_FIXED_STN_PEN;
    symbol_pen[VRT_FIXED_STN_SYM] = VRT_FIXED_STN_PEN;
    symbol_pen[REJECTED_STN_SYM] = REJECTED_STN_PEN;

    nstns = number_of_stations( net );
    stns = (stn_plot_s *) check_malloc( (nstns+1) * sizeof(stn_plot_s) );

    no_good_stations = 0;
    nadjust3 = 0;
    nadjusth = 0;
    nadjustv = 0;
    for( istn = 0; istn++ < nstns; )
    {
        double xyz[3];

        st = stnptr(istn);
        sa = stnadj( st );

        xyz[CRD_LAT] = st->ELat;
        xyz[CRD_LON] = st->ELon;
        xyz[CRD_HGT] = st->OHgt;
        convert_coords( &cnv, xyz, NULL, xyz, NULL );

        stns[istn].northing = n = xyz[CRD_NORTH];
        stns[istn].easting  = e = xyz[CRD_EAST];
        stns[istn].flags = 0;
        stns[istn].e_offset = 0.0;
        stns[istn].n_offset = 0.0;
        stns[istn].symbol = REJECTED_STN_SYM;

        if( adjusted )
        {
            xyz[CRD_LAT] = sa->initELat;
            xyz[CRD_LON] = sa->initELon;
            xyz[CRD_HGT] = sa->initOHgt;
            convert_coords( &cnv, xyz, NULL, xyz, NULL );
            stns[istn].dn = n - xyz[CRD_NORTH];
            stns[istn].de = e - xyz[CRD_EAST];
            ds = _hypot( stns[istn].de, stns[istn].dn );
            if( ds > adjmaxh ) adjmaxh = ds;
            stns[istn].dh = st->OHgt - sa->initOHgt;
            if( fabs(stns[istn].dh) > adjmaxv ) adjmaxv = fabs(stns[istn].dh);
        }
        else
        {
            stns[istn].dn = stns[istn].de = stns[istn].dh = 0.0;
        }

        if( sa->flag.ignored ) continue;

        if( ! no_good_stations++ )
        {
            nmax = nmin = n;
            emax = emin = e;
        }
        else
        {
            if( n > nmax ) nmax = n; else if (n < nmin ) nmin = n;
            if( e > emax ) emax = e; else if (e < emin ) emin = e;
        }

        if( sa->flag.adj_h )
        {
            if( sa->flag.adj_v ) nadjust3++; else nadjusth++;
        }
        else if( sa->flag.adj_v ) nadjustv++;

        symbol = FREE_STN_SYM;
        switch( dimension )
        {
        case 1: if( !sa->flag.adj_v ) symbol = VRT_FIXED_STN_SYM; break;
        case 2: if( !sa->flag.adj_h ) symbol = HOR_FIXED_STN_SYM; break;
        case 3: if( !sa->flag.adj_v || !sa->flag.adj_h )
            {
                symbol = FIXED_STN_SYM;
                if( sa->flag.adj_h ) { symbol = VRT_FIXED_STN_SYM; }
                if( sa->flag.adj_v ) { symbol = HOR_FIXED_STN_SYM; }
            }
            break;
        }
        if( sa->flag.rejected ) { symbol = REJECTED_STN_SYM; }
        stns[istn].symbol = symbol;
    }

    if( no_good_stations )
    {
        int iGood = 0;
        xyindex = (int *) check_malloc( no_good_stations * (sizeof( int)));
        sortIndex = (int *) check_malloc( no_good_stations * sizeof(int) );
        sortValues = (sortobj *) check_malloc( (nstns+1) * sizeof(sortobj) );
        for( istn = 0; istn++ < nstns;  )
        {
            st = stnptr(istn);
            sa = stnadj( st );
            if( sa->flag.ignored ) continue;
            xyindex[iGood] = istn;
            sortIndex[iGood] = istn;
            iGood++;
            if( iGood >= no_good_stations ) break;
        }
        no_good_stations = iGood;
    }

    nhighlight = 0;
    noffset = 0;
    slist_ncols = 0;
}

static void  validate_xyindex( void )
{
    if( !xyindex_valid )
    {
        qsort( xyindex, no_good_stations, sizeof(int), cmp_stn_easting );
        xyindex_valid = 1;
        xyindex_version++;
    }
}

long get_xyindex_version()
{
    validate_xyindex();
    return xyindex_version;
}

int used_station_count()
{
    return no_good_stations;
}

int sorted_station_number( int i )
{
    if( i < 0 || i >= no_good_stations || !sortIndex ) return -1;
    return sortIndex[i];
}

int projection_defined( void )
{
    return arbitrary_projection ? 0 : 1;
}


int reload_covariances( BINARY_FILE *b )
{
    int istn;
    double cvr[6];
    int nstns;

    cvrmaxh = cvrmaxv = 0.0;

    if( find_section( b, "STATION_COVARIANCES" ) != OK ) return MISSING_DATA;

    nstns = number_of_stations( net );
    covar = (covariance *) check_malloc( sizeof(covariance) * (nstns+1) );

    for( istn = 0; istn++ < nstns; )
    {
        fread( cvr, sizeof(cvr), 1, b->f );
        calc_error_ellipse( cvr, &covar[istn].emax, &covar[istn].emin,
                            &covar[istn].az );
        if( covar[istn].emax > cvrmaxh ) cvrmaxh = covar[istn].emax;
        covar[istn].sehgt = cvr[5] > 0.0 ? sqrt(cvr[5]) : 0.0;
        if( covar[istn].sehgt > cvrmaxv ) cvrmaxv = covar[istn].sehgt;
    }

    return check_end_section( b );
}

int got_covariances( void )
{
    return covar ? 1 : 0;
}

void max_covariance_component( double *h, double *v )
{
    if( h ) *h = cvrmaxh;
    if( v ) *v = cvrmaxv;
}

void max_adjustment_component( double *h, double *v)
{
    if( h ) *h = adjmaxh;
    if( v ) *v = adjmaxv;
}


void get_error_ellipse( int istn, double *emax, double *emin, double *brng )
{
    *emax = covar[istn].emax;
    *emin = covar[istn].emin;
    *brng = covar[istn].az;
}

void get_height_error( int istn, double *hgterr )
{
    *hgterr = covar[istn].sehgt;
}



static int find_spanning( double px )
{
    int i1,i2,i3;
    double x1, x2, x3, y;
    if( no_good_stations <= 1 ) return 0;
    validate_xyindex();
    i1=0;
    i2=no_good_stations-1;
    get_station_coordinates( xyindex[i1], &x1, &y );
    if( x1 >= px) return i1;
    get_station_coordinates( xyindex[i2], &x2, &y );
    if( x2 < px) return i2;
    while( i1+1 < i2 )
    {
        i3 = (i1+i2)/2;
        get_station_coordinates( xyindex[i3], &x3, &y );
        if( x3 < px) i1 = i3; else i2 = i3;
    }
    return i1;
}

int first_station_past_x( double x ) { return find_spanning(x); }
int sorted_x_station_number( int id ) { return xyindex[id]; }

void set_station_offset( int istn, double oe, double on )
{
    char was_offset;
    stns[istn].e_offset = (float) oe;
    stns[istn].n_offset = (float) on;
    was_offset = stns[istn].flags & PSFLG_OFFSET;
    if( fabs(oe) > 0.001 || fabs(on) > 0.001 )
    {
        stns[istn].flags |= PSFLG_OFFSET;
        if( !was_offset ) noffset++;
    }
    else
    {
        stns[istn].flags &= ~PSFLG_OFFSET;
        if( was_offset ) noffset--;
    }
    if( apply_station_offsets ) xyindex_valid = 0;
}

int get_station_offset( int istn, double *oe, double *on )
{
    if( stns[istn].flags & PSFLG_OFFSET )
    {
        *oe = stns[istn].e_offset;
        *on = stns[istn].n_offset;
        return 1;
    }
    else
    {
        *oe = 0.0;
        *on = 0.0;
        return 0;
    }
}

int offset_station_count() { return noffset; }

void use_station_offsets( int on )
{
    if( on )
    {
        if( noffset && ! apply_station_offsets ) xyindex_valid = 0;
        apply_station_offsets = 1;
    }
    else
    {
        if( noffset && apply_station_offsets ) xyindex_valid = 0;
        apply_station_offsets = 0;
    }
}

int using_station_offsets( void ) { return apply_station_offsets; }

void get_station_coordinates( int istn, double *e, double *n )
{
    *e = stns[istn].easting;
    *n = stns[istn].northing;
    if( apply_station_offsets && stns[istn].flags & PSFLG_OFFSET )
    {
        *e += stns[istn].e_offset;
        *n += stns[istn].n_offset;
    }
}

void get_station_adjustment( int istn, double dxyz[3] )
{
    stn_plot_s *s = &stns[istn];
    dxyz[0] = s->de;
    dxyz[1] = s->dn;
    dxyz[2] = s->dh;
}

void get_projection_coordinates( int istn, double *e, double *n )
{
    *e = stns[istn].easting;
    *n = stns[istn].northing;
}

int nearest_station( double px, double py, double tol )
{
    int valid;
    int i1,i2,in;
    double de,dst,idst;
    double x, y;

    if( no_good_stations < 1 ) return 0;
    validate_xyindex();

    in = i1 = i2 = find_spanning( px );
    get_station_coordinates( xyindex[in], &x, &y );
    dst = fabs(x - px) + fabs(y - py);
    valid = stns[xyindex[in]].flags & PSFLG_VISIBLE && dst < tol;
    if( !valid ) dst = tol;
    while( i1 > 0 || i2 < no_good_stations-1 )
    {
        if( --i1 >= 0 )
        {
            get_station_coordinates( xyindex[i1], &x, &y );
            de = px - x;
            if( de>dst )
                i1 = 0;
            else if( stns[xyindex[i1]].flags & PSFLG_VISIBLE &&
                     (idst=de+fabs(py-y)) < dst )
                { in=i1; dst=idst; valid = 1; }
        }

        if( ++i2 < no_good_stations )
        {
            get_station_coordinates( xyindex[i2], &x, &y );
            de = x - px;
            if( de>dst )
                i2 = no_good_stations;
            else if( stns[xyindex[i2]].flags & PSFLG_VISIBLE &&
                     (idst=de+fabs(py-y)) < dst)
                { in=i2; dst=idst; valid = 1;}
        }
    }
    return valid ? xyindex[in] : 0;
}


void get_station_range( double *emn, double *nmn, double *emx, double *nmx )
{
    *emn = emin; *nmn = nmin;
    *emx = emax; *nmx = nmax;
}

int station_count( void )
{
    return nstns;
}

int station_in_view( int istn )
{
    double x, y;
    get_station_coordinates( istn, &x, &y );
    return  x >= plot_emin &&
            x <= plot_emax &&
            y >= plot_nmin &&
            y <= plot_nmax;

}

int station_showable( int istn )
{
    if( station_hidden( istn ) ) return 0;
    if( ignored_station( istn ) ) return 0;
    if( ! option_selected(symbol_opt[stns[istn].symbol]) ) return 0;
    if( stn_colourby_class > 0 )
    {
        int cvalue = get_station_class(stnptr(istn),stn_colourby_class);
        if( ! pen_selected(station_class_pen( cvalue )) ) return 0;
    }
    return 1;
}

int station_plotable( int istn )
{
    return station_showable(istn) && station_in_view( istn );
}

void highlight_station( int istn )
{
    if( !(stns[istn].flags & PSFLG_HIGHLIGHT) )
    {
        stns[istn].flags |= PSFLG_HIGHLIGHT;
        nhighlight++;
    }
}

void unhighlight_station( int istn )
{
    if( (stns[istn].flags & PSFLG_HIGHLIGHT) )
    {
        stns[istn].flags &= ~PSFLG_HIGHLIGHT;
        nhighlight--;
    }
}

int station_highlighted( int istn )
{
    return (stns[istn].flags & PSFLG_HIGHLIGHT) ? 1 : 0;
}

int highlighted_stations( void )
{
    return nhighlight;
}

void clear_all_highlights( void )
{
    int istn;
    for( istn = 0; istn++ < nstns; )
    {
        stns[istn].flags &= ~PSFLG_HIGHLIGHT;
    }
    nhighlight = 0;
}

void hide_station( int istn )
{
    if( !(stns[istn].flags & PSFLG_HIDDEN) )
    {
        stns[istn].flags |= PSFLG_HIDDEN;
    }
}

void unhide_station( int istn )
{
    if( (stns[istn].flags & PSFLG_HIDDEN) )
    {
        stns[istn].flags &= ~PSFLG_HIDDEN;
    }
}

int station_hidden( int istn )
{
    return (stns[istn].flags & PSFLG_HIDDEN) ? 1 : 0;
}

void flag_station_visible( int istn )
{
    stns[istn].flags |= PSFLG_VISIBLE;
}

void setup_station_pens( int class_id )
{
    if( class_id < 0 || class_id > network_classification_count(net)) class_id = 0;
    if( class_id == stn_colourby_class ) return;
    stn_colourby_class = class_id;
    setup_station_layers(class_id);
}

void get_stationpen_definition( char *def )
{
    if( stn_colourby_class == 0 ) strcpy(def,"usage");
    else strcpy(def,network_class_name(net,stn_colourby_class));
}

void init_plotting_stations( void )
{
    int i;
    for( i = 0; i++ < nstns; )
    {
        stns[i].flags &= ~PSFLG_VISIBLE;
    }
}


void plot_station_symbol( map_plotter *plotter, double px, double py, int symbolid, int pen )
{
    SYMBOL( plotter, px, py, pen, symbolid );
}


int plot_stations( map_plotter *plotter, int first, int highlightonly )
{
    int istn, count, plot_them, pen, symbol;

    plot_them = option_selected( SYMBOL_OPT );

    if( !plot_them ) return ALL_DONE;

    if( first < 0 ) { first = 0; count = nstns; }
    else {count = 5; }

    for( istn = first; istn++ < nstns; )
    {
        double x, y;
        int highlighted;

        if( !count-- ) { return istn-1; }

        if( ! station_plotable(istn) ) continue;

        if( !plot_them ) continue;

        symbol = stns[istn].symbol;
        highlighted = station_highlighted( istn );

        if( highlighted )
        {
            pen = get_pen(HIGHLIGHT_PEN);
        }
        else
        {
            if( highlightonly ) continue;
            if( stn_colourby_class > 0 )
            {
                pen = get_station_class( stnptr(istn), stn_colourby_class );
                pen = station_class_pen( pen );
            }
            else
            {
                pen = get_pen(symbol_pen[symbol]);
            }
        }

        get_station_coordinates( istn, &x, &y );
        plot_station_symbol( plotter, x, y, symbol, pen );

        flag_station_visible( istn );
    }

    return ALL_DONE;
}



int plot_covariances( map_plotter *plotter, int first )
{
    int istn, pen;
    double x, y, a, b;
    int count;
    double eemult;

    if( dimension == 1 ) return ALL_DONE;
    if( !covar ) return ALL_DONE;

    pen = get_pen(ELLIPSE_PEN);
    if( !pen_visible( pen ) || !option_selected(ELLIPSE_OPT) ) return ALL_DONE;

    if( first < 0 ) {first = 0; count = nstns; }
    else count = 5;

    eemult = errell_factor * errell_scale;

    for( istn = first; istn++ < nstns; )
    {

        if( !count-- ) { return istn-1; }

        if( ! station_plotable( istn ) ) continue;

        get_station_coordinates( istn, &x, &y );

        a = covar[istn].emax * eemult;
        b = covar[istn].emin * eemult;

        ELLIPSE( plotter, x, y, a, b, PI/2.0 - covar[istn].az, pen );

        flag_station_visible( istn );
    }

    return ALL_DONE;
}



int plot_height_errors( map_plotter *plotter, int first )
{
    int istn, count, pen;
    double x, y, he, s1;

    if( dimension == 2 ) return ALL_DONE;
    if( !covar ) return ALL_DONE;

    pen = get_pen(HGTERR_PEN);
    if( !pen_visible(pen) || !option_selected(HGTERR_OPT) ) return ALL_DONE;

    s1 = stn_symbol_size  / 2.0;

    if( first < 0 ) {first = 0; count = nstns; }
    else count = 5;

    for( istn = first; istn++ < nstns; )
    {

        if( !count-- ) { return istn-1; }

        if( ! station_plotable( istn ) ) continue;

        get_station_coordinates( istn, &x, &y );

        he = covar[istn].sehgt  * hgterr_factor * hgterr_scale;

        if( he > 0 )
        {
            LINE(plotter, x-s1, y+he, pen);
            LINE(plotter, x+s1, y+he, CONTINUE_LINE );
            LINE(plotter, x, y+he, pen);
            LINE(plotter, x, y-he, CONTINUE_LINE );
            LINE(plotter, x-s1, y-he, pen);
            LINE(plotter, x+s1, y-he, CONTINUE_LINE );

            flag_station_visible( istn );
        }
    }

    return ALL_DONE;
}


int plot_station_names( map_plotter *plotter, int first )
{
    int istn, nch, count, pen;
    double x, y, s1;
    char name[80];
    int pltnames, pltcodes;

    pltnames = option_selected(NAME_OPT);
    pltcodes = option_selected(CODE_OPT);

    if( !pltnames && !pltcodes ) return ALL_DONE;

    pen = get_pen(TEXT_PEN);
    if( !pen_visible(pen) ) return ALL_DONE;

    s1 = stn_symbol_size*0.6;

    if( first < 0 ) {first = 0; count = nstns; }
    else count = 5;

    for( istn = first; istn++ < nstns; )
    {

        if( !count-- ) { return istn-1; }

        if( ! station_plotable( istn ) ) continue;

        get_station_coordinates( istn, &x, &y );

        nch = 0;
        name[0] = 0;
        if( pltcodes )
        {
            strcpy( name, stnptr(istn)->Code );
        }

        if( pltnames )
        {
            if( pltcodes )
            {
                nch = strlen(name);
                strcpy( name+nch, "  ");
                nch += 2;
            }
            strncpy( name+nch, stnptr(istn)->Name, 80-nch);
            name[79] = 0;
            replace_tabs( name+nch  );
        }

        PLOTTEXT( plotter, x+s1, y+s1, stn_name_size, pen, name );
        flag_station_visible( istn );
    }

    return ALL_DONE;
}


int plot_adjustments( map_plotter *plotter, int first )
{
    int istn, count;
    double x, y, dx, dy, dh;
    int plthor, pltvrt, penhor, penvrt;

    if( !stations_adjusted ) return ALL_DONE;

    penhor = get_pen(HOR_ADJ_PEN);
    penvrt = get_pen(HGT_ADJ_PEN);

    plthor = dimension != 1 && option_selected(HOR_ADJ_OPT)
             && pen_visible(penhor);
    pltvrt = dimension != 2 && option_selected(HGT_ADJ_OPT)
             && pen_visible(penvrt);

    if( !plthor && !pltvrt ) return ALL_DONE;

    if( first < 0 ) {first = 0; count = nstns; }
    else count = 5;

    for( istn = first; istn++ < nstns; )
    {

        if( !count-- ) { return istn-1; }

        if( ! station_plotable( istn ) ) continue;

        get_station_coordinates( istn, &x, &y );

        dx = stns[istn].de * errell_scale;
        dy = stns[istn].dn * errell_scale;
        dh = stns[istn].dh * hgterr_scale;

        if( plthor )
        {
            LINE(plotter, x, y, penhor);
            LINE(plotter, x+dx, y+dy, CONTINUE_LINE );
        }

        if( pltvrt )
        {
            LINE(plotter, x, y, penvrt);
            LINE(plotter, x, y+dh, CONTINUE_LINE );
        }

        flag_station_visible( istn );
    }

    return ALL_DONE;
}

void free_station_resources()
{
    if( stns ) check_free( stns );
    stns = NULL;
    if( xyindex ) check_free( xyindex );
    xyindex = NULL;
    if( sortIndex ) check_free( sortIndex );
    sortIndex = NULL;
    if( sortValues ) check_free( sortValues );
    sortValues = NULL;
    if( covar ) check_free( covar );
    covar = NULL;
}
