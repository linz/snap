#include "snapconfig.h"
/*

These routines apply the following modifications to data as it is loaded
into SNAP

1) Apply error factors

   Currently do not permit independent scaling of vector e,n,u components

   Currently do not permit definition of error factors within the data file?

   *** For GPS multistation data, cannot apply different weights
   to different baselines. i.e. weighting of first line applies to
   all lines. NOTE: This means classifications within GPS data are
   not fully respected. ***

2) Modify unused flag to reflect explicit rejection, or
   rejection by station? or by class.

   (Note that when list of stations to be calculated is modified
   this will not be correct i.e. will still need to check for rejected
   stations)

3) Record connections of data

4) Complete removal of observations based upon classification,
   type, or data file, or ignored station.

5) Optionally break up data by type to allow sorting
   Record locations of data blocks to enable sorting

6) Record usage of various types of parameters
   (stations, refcoef, syserrs and specific params, ref-frames)

7) Convert distance ratios to distances if required

8) Check for two data types using Schreiber's equations.

*/

/*
   $Log: loadsnap.c,v $
   Revision 1.3  2003/11/25 01:29:58  ccrook
   Updated SNAP to allow calculation of projection bearings in coordinate
   systems other than that of the coordinate file

   Revision 1.2  1998/05/21 14:40:34  CHRIS
   Added facility to reject observations without dates if the adjustment
   includes a deformation model.

   Revision 1.1  1996/01/03 21:59:37  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "snap/snapglob.h"
#include "loadsnap.h"
#include "snapdata/datatype.h"
#include "snapdata/survdata.h"
#include "snapdata/loaddata.h"
#include "snap/survfilr.h"
#include "snap/bindata.h"
#include "reorder.h"
#include "sortobs.h"
#include "snap/stnadj.h"
#include "stnobseq.h"
#include "snapdata/survdata.h"
#include "adjparam.h"
#include "util/classify.h"
#include "snap/rftrans.h"
#include "snap/bearing.h"
#include "util/classify.h"
#include "util/symmatrx.h"
#include "notedata.h"
#include "snapdata/gpscvr.h"
#include "snap/gpscvr2.h"
#include "coefs.h"
#include "util/dateutil.h"
#include "util/chkalloc.h"
#include "util/errdef.h"


/*********************************************************************/
/* Code to manage a list of missing stations...                      */
/* Callback function used by loaddata to get id's of various objects */

/* Id returned for stations that will be ignored                     */

#define IGNORE_ID -1 

typedef struct missing_stn_s
{
    struct missing_stn_s *next;
    char *code;
    int refcount;
    int id;
} missing_stn;


static missing_stn *missing = NULL;
static int missing_id = IGNORE_ID;

static int ignore_missing_stations = 0;

static int need_obs_date = 0;

static int missing_data = 0;

void set_ignore_missing_stations( int option )
{
    ignore_missing_stations = option;
}

void set_require_obs_date( int option )
{
    need_obs_date = option;
}

static int missing_station_id( const char *code )
{
    missing_stn *ms, *prev, *newst;
    if( !code ) return 0;
    if( !ignore_missing_stations ) return 0;
    for( ms = missing, prev = NULL; ms; prev = ms, ms = ms->next )
    {
        int cmp;
        cmp = stncodecmp( ms->code, code );
        if( cmp == 0 ) { ms->refcount++; return ms->id;}
        if( cmp > 0 ) break;
    }
    newst = (missing_stn *) check_malloc( sizeof(missing_stn) + strlen(code) + 1 );
    newst->next = ms;
    if( prev ) prev->next = newst; else missing = newst;
    newst->id = --missing_id;
    newst->refcount = 1;
    newst->code = ((char *)(void *)newst)+sizeof(missing_stn);
    strcpy( newst->code, code );
    return newst->id;
}

static char *missing_station_name( int id )
{
    missing_stn *ms;
    for( ms = missing; ms; ms = ms->next )
    {
        if( ms->id == id ) return ms->code;
    }
    return NULL;
}

static void delete_missing_station_list( void )
{
    missing_stn *ms;
    while( missing )
    {
        ms = missing->next;
        check_free( missing );
        missing = ms;
    }
    missing_id = IGNORE_ID;
}

static void list_missing_stations( void )
{
    missing_stn *ms;
    char buf[80];
    for( ms = missing; ms; ms = ms->next )
    {
        sprintf(buf,"Station %-10s is not in the coordinate file.  Used %d times",
                ms->code,ms->refcount );
        handle_error(WARNING_ERROR, buf, NO_MESSAGE );
    }
}

/***********************************************************************/

static int convert_distance_ratios = 0;

void set_convert_ratios_to_distance( int option )
{
    convert_distance_ratios = option;
}

static void convert_ratios_to_distances( survdata *sd )
{
    int i;
    trgtdata *t;

    for( i=0; i<sd->nobs; i++ )
    {
        t = get_trgtdata( sd, i );
        if( t->type == DR ) t->type = SD;
    }
}


static void record_connections( survdata *sd )
{
    int i, i2, from;
    trgtdata *t, *t2;

    from = sd->from;
    for( i=0; i < sd->nobs; i++ )
    {
        t = get_trgtdata(sd,i);
        if( !t->to ) continue;
        if( from && t->to ) add_connection( from, t->to );
        if( !datatype[t->type].joinsgroup ) continue;
        for( i2 = i+1; i2<sd->nobs; i2++ )
        {
            t2 = get_trgtdata(sd, i2 );
            if( t2->type == t->type && t2->to ) add_connection( t->to, t2->to );
        }
    }
}

static void record_parameter_usage( survdata *sd )
{
    trgtdata *t;
    datatypedef *dt;
    int i;
    char unused;
    int type;

    for( i=0; i<sd->nobs; i++ )
    {

        t = get_trgtdata(sd,i);

        unused = t->unused;
        type = t->type;
        dt = datatype + type;

        count_obs( type, sd->file, sd->date, unused );
        if( sd->from ) count_stn_obs( type, sd->from, unused );
        if( t->to ) count_stn_obs( type, t->to, unused );

        /* If the observations could be used (no guarantee - stations
           may still be rejected) then note parameters that may be used */

        if ( !unused &&
                (!sd->from || !rejected_station( sd->from )) &&
                (!t->to || !rejected_station(t->to)) )
        {

            int is;

            for( is = 0; is < t->nsyserr; is++ )
            {
                flag_param_used(sd->syserr[is+t->isyserr].prm_id);
            }

            switch( sd->format )
            {

            case SD_OBSDATA:
            {
                obsdata *od;
                od = & sd->obs.odata[i];
                if( od->prm_id ) flag_param_used( od->prm_id );
                if( t->type == ZD ) flag_param_used( od->refcoef );
            }
            break;

            case SD_VECDATA:
                flag_rftrans_used( rftrans_from_id(sd->reffrm), 
                        dt->ispoint ? FRF_ABSOLUTE : FRF_VECDIFF );
                break;

            case SD_PNTDATA:
                break;

            default:
                handle_error( INTERNAL_ERROR,
                              "Invalid survdata.format in record_parameter_usage",
                              NO_MESSAGE );
            }
        }
    }
}


/* Apply error factors and rejection by file, classification, and data type */
/* On input the unused flag implies rejection if it is non-zero */
/* On exit it may have the REJECT_OBS_BIT or the IGNORE_OBS_BIT set */
/* Function returns the number of obs that are to be ignored */

static int set_usage_flag( survdata *sd )
{
    int i, ic;
    trgtdata *t;
    int nignore;

    nignore = 0;
    for( i=0; i<sd->nobs; i++ )
    {
        t = get_trgtdata( sd, i );
        t->unused |= obs_usage[t->type] | survey_data_file_ptr( sd->file )->usage;
        for( ic = 0; ic < t->nclass; ic++ )
        {
            classdata *cd;
            cd = sd->clsf + ic + t->iclass;
            t->unused |= get_class_usage( &obs_classes, cd->class_id, cd->name_id );
        }
        if( t->unused & IGNORE_OBS_BIT ) nignore++;
    }
    return nignore;
}

static double get_errfct( survdata *sd, trgtdata *t )
{
    double errfct;
    int ic;

    errfct = survey_data_file_errfct( sd->file );
    errfct *= obs_errfct[ t->type ];
    for( ic = 0; ic < t->nclass; ic++ )
    {
        classdata *cd;
        cd = sd->clsf + ic + t->iclass;
        errfct *= get_class_errfct( &obs_classes, cd->class_id, cd->name_id );
    }

    return errfct;
}


static void apply_error_factors( survdata *sd )
{
    int i;

    switch (sd->format)
    {

    case SD_OBSDATA:
    {
        obsdata *od;
        for( i = 0, od=sd->obs.odata; i<sd->nobs; i++, od++ )
        {
            od->error  *= get_errfct( sd, &od->tgt );
        }
    }
    break;

    case SD_VECDATA:
    {
        int row, col;
        double errfct;

        /* Note: with the covariance, cannot apply error factors
           selectively to individual observations */

        errfct = get_errfct( sd, &sd->obs.vdata->tgt );
        errfct *= errfct;
        for( row = 0; row < sd->ncvr; row++ ) for( col = 0; col <= row; col++ )
            {
                Lij( sd->cvr, row, col ) *= errfct;
            }
    }
    break;

    case SD_PNTDATA:
    {
        pntdata *pd;
        double errfct;
        int nerr, j;

        for( i = 0, pd=sd->obs.pdata; i<sd->nobs; i++, pd++ )
        {
            errfct = get_errfct( sd, &pd->tgt );
            nerr = datatype[pd->tgt.type].isvector ? 6 : 1;
            for( j = 0; j < nerr; j++ ) pd->error *= errfct;
        }
    }
    break;

    }
}


/* Check whether we need to split up the observations to
   apply Schreibers method (ie if we have both distance ratio and
   horizontal angle data */

static int split_obsdata_for_schreiber( survdata *sd )
{
    int schtype = -1;
    obsdata *od;
    int i;

    for( i = 0, od = sd->obs.odata; i<sd->nobs; i++, od++ )
    {
        int type;
        type = od->tgt.type;
        if( type == DR || type == HA )
        {
            if( schtype != type && schtype >= 0 ) return 1;
            schtype = type;
        }
    }
    return 0;
}


/* Routine to load observations by subsets based upon type */

static void save_split_survdata( survdata *sd )
{
    unsigned char typesaved[NOBSTYPE];
    long loc;
    int iobs, sobs, to;
    trgtdata *t;

    for( iobs = 0; iobs< NOBSTYPE; iobs++ ) typesaved[iobs] = 0;

    for( iobs = 0; iobs < sd->nobs; iobs++ )
    {
        t = get_trgtdata( sd, iobs );
        if( typesaved[t->type] ) continue;
        if( datatype[t->type].joinsgroup )
        {
            sobs = -1;
            typesaved[t->type] = 1;
        }
        else
        {
            sobs = iobs;
        }
        loc = save_survdata_subset( sd, sobs, t->type );
        to = t->to;
        if( sobs < 0 ) to = 0;
        if( t->type == GB && sd->nobs == 1 ) to = t->to;
        save_observation( sd->from, to, t->type, loc );
    }
}

static void save_whole_survdata( survdata *sd )
{
    long loc;
    int to;
    trgtdata *t;
    loc = save_survdata( sd );
    to = 0;
    t = get_trgtdata( sd, 0 );
    if( sd->nobs == 1 ) to = t->to;
    save_observation( sd->from, to, t->type, loc );
}

//case ID_REFCOEF:    id = refcoef_prm( code ); break;
//case ID_DISTSF:     id = distsf_prm( code ); break;
//case ID_BRNGREF:    id = brngref_prm( code ); break;
static void load_snap( survdata *sd )
{
    int nignore;

    /* Fail if need observation date and is not provided */

    if( need_obs_date && sd->date == UNDEFINED_DATE )
    {
        char location[80];
        sprintf(location,"In %.50s line %d",
                survey_data_file_name(sd->file), sd->obs.vdata[0].tgt.lineno );
        handle_error(INVALID_DATA,"Observation date not defined", location);
        missing_data++;
        return;
    }

    /* Fail if covariance not defined for observation */

    if( sd->format == SD_VECDATA && ! sd->cvr )
    {
        char location[80];
        sprintf(location,"In %.50s line %d",
                survey_data_file_name(sd->file), sd->obs.vdata[0].tgt.lineno );
        handle_error(INVALID_DATA,"Observation covariance not defined", location);
        missing_data++;
        return;
    }

    /* Determine which observations are being ignored, and reject them */

    nignore = set_usage_flag( sd );
    if( nignore == sd->nobs ) return;

    /* Apply error factors to the data */

    apply_error_factors( sd );

    /* Convert distance ratios to distances if required */

    if( convert_distance_ratios ) convert_ratios_to_distances( sd );

    /* Record the usage (for identifying which parameters can be calculated),
       and the connections (for minimizing bandwidth) */

    record_connections( sd );
    record_parameter_usage( sd );

    /* Decide whether to split the observations or not */

    if( nignore > 0 ||
            ( sd->format == SD_OBSDATA &&
              ( sort_obs || split_obsdata_for_schreiber(sd) ) ) )
    {

        save_split_survdata( sd );
    }
    else
    {
        save_whole_survdata( sd );
    }
}


/* Callback function used by loaddata to get id's of various objects */


static long snap_id( int type, int group_id, const char *code )
{
    long id;
    id = 0;
    switch (type)
    {
    case ID_STATION:    
        id = find_station( net, code );
        if( id )
        {
            if( ignored_station(id) ) id=IGNORE_ID;
        }
        else
        { 
            if( !id ) id = missing_station_id( code );
        }
        break;
    case ID_COEF:
        switch( group_id )
        {
        case COEF_CLASS_DISTSF:  id = distsf_prm( code ); break;
        case COEF_CLASS_BRNGREF: id = brngref_prm( code ); break;
        case COEF_CLASS_REFCOEF: id = refcoef_prm( code ); break;
        case COEF_CLASS_REFFRM:  id = get_rftrans_id( code, REFFRM_DEFAULT ); break;
        }
        break;
    case ID_PROJCTN:    id = get_bproj( code ); break;
    case ID_SYSERR:     id = syserr_prm( code ); break;
    case ID_CLASSTYPE:  id = classification_id( &obs_classes, code, 1 ); break;
    case ID_CLASSNAME:  id = class_value_id( &obs_classes, group_id, code, 1 ); break;
    case ID_NOTE:       id = save_note( code, group_id ); break;
    }
    return id;
}

static const char *snap_name( int type, int group_id, long id )
{
    const char *name;
    name = NULL;
    switch (type)
    {
    case ID_STATION:   if( id < 0 ) name = missing_station_name( (int) id );
        else name = station_code( (int) id );
        break;
    case ID_COEF:
        switch( group_id )
        {
        case COEF_CLASS_DISTSF:  name = distsf_name( (int) id ); break;
        case COEF_CLASS_BRNGREF: name = brngref_name( (int) id ); break;
        case COEF_CLASS_REFCOEF: name = refcoef_name( (int) id ); break;
        case COEF_CLASS_REFFRM:  break;
        }
        break;
    case ID_SYSERR:    name = syserr_name( (int) id ); break;
    case ID_PROJCTN:   name = bproj_name( (int) id ); break;
    case ID_CLASSTYPE: name = classification_name( &obs_classes, (int) id ); break;
    case ID_CLASSNAME: name = class_value_name( &obs_classes, group_id, (int) id ); break;
    case ID_NOTE:    break;
    }
    return name;
}

static double snap_calc_value( int type, long id1, long id2 )
{
    if( type == CALC_DISTANCE )
    {
        double dist;
        station *st1 = stnptr(id1);
        station *st2 = stnptr(id2);
        if( ! st1 || ! st2 ) return 0.0;
        dist = calc_distance( st1, 0.0, st2, 0.0, NULL, NULL );
        return dist;
    }
    else if ( type == CALC_HDIST )
    {
        double dist;
        station *st1 = stnptr(id1);
        station *st2 = stnptr(id2);
        if( ! st1 || ! st2 ) return 0.0;
        dist = calc_horizontal_distance( st1, st2, NULL, NULL );
        return dist;
    }
    return 0.0;
}

void init_load_snap( void )
{
    set_require_obs_date( deformation == NULL ? 0 : 1 );
    init_load_data( load_snap, snap_id, snap_name, snap_calc_value );
    init_snap_gps_covariance();
    missing_data = 0;
}

int term_load_snap( void )
{
    term_load_data();
    list_missing_stations();
    delete_missing_station_list();
    return missing_data == 0 ? OK : MISSING_DATA;
}
