#include "snapconfig.h"

/* Header file for routines managing point data - lats, longs, doppler, etc */

/*
   $Log: pntdata.c,v $
   Revision 1.6  2004/04/22 02:35:44  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.5  2003/11/25 01:29:59  ccrook
   Updated SNAP to allow calculation of projection bearings in coordinate
   systems other than that of the coordinate file

   Revision 1.4  1999/05/26 07:01:54  ccrook
   Modified to allow listing of worst residuals without listing all residuals

   Revision 1.3  1998/05/21 14:32:22  CHRIS
   Added support for deformation model.

   Revision 1.2  1996/02/23 17:02:41  CHRIS
   Added calculation of MDE field to residual listing.  Also fixed
   redundancy which was not being written to a residual listing field
   when it should have been.

   Revision 1.1  1996/01/03 22:04:06  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>

#include "snap/snapglob.h"
#include "output.h"
#include "snapdata/survdata.h"
#include "snap/bindata.h"
#include "pntdata.h"
#include "notedata.h"
#include "sortobs.h"
#include "snap/stnadj.h"
#include "util/geodetic.h"
#include "util/lsobseq.h"
#include "snapdata/survdata.h"
#include "stnobseq.h"
#include "residual.h"
#include "util/classify.h"
#include "coefs.h"
#include "snap/datastat.h"
#include "util/dms.h"
#include "util/pi.h"
#include "util/errdef.h"

static void *latobsfmt = NULL;
static void *lonobsfmt = NULL;

void apply_pntdata_options( survdata *p )
{
    /* None defined yet */
}

int pntdata_obseq( survdata *p, void *hA, int nextra )
{
    station *st;
    double value = 0.0;
    vector3 dst;
    char unused;
    int type;
    pntdata *pd;

    /* Initiallize the observation equations */

    if( p->nobs > 1 )
    {
        handle_error( INTERNAL_ERROR, "Point data format cannot handle grouped obs",
                      "pntdata_obseq" );
        return INTERNAL_ERROR;
    }

    pd = p->obs.pdata;
    type = pd->tgt.type;

    init_oe( hA, 1, 1+nextra, OE_LOWERTRI_CVR );

    st = stnptr(p->from);

    switch( type )
    {
    case LT:  value = calc_lat( st, dst ); break;
    case LN:  value = calc_lon( st, dst ); break;
    case OH:  value = calc_ohgt( st, p->fromhgt, dst ); break;
    case EH:  value = calc_ehgt( st, p->fromhgt, dst ); break;
    }

    unused = pd->tgt.unused || rejected_station( p->from );

    oe_value( hA, 1, pd->value - value );
    /* set station obseq also applies any station deformation to the
       the observed value */
    set_station_obseq( st, dst, hA, 1, p->date );
    if( unused ) oe_flag_unused( hA, 1 );
    oe_covar( hA, 1, 1, pd->error*pd->error );

    return OK;
}

static void create_dms_formats( void )
{
    latobsfmt = create_dms_format(3,obs_precision[LT],0,NULL,NULL,NULL," N"," S");
    lonobsfmt = create_dms_format(3,obs_precision[LN],0,NULL,NULL,NULL," E"," W");
}


void list_pntdata( FILE *out, survdata *p )
{
    int type;
    pntdata *pd;
    trgtdata *t;

    pd = p->obs.pdata;
    t = &pd->tgt;
    type = t->type;

    if( (type == LT || type == LN) && !latobsfmt ) create_dms_formats();

    fputs("\n", out );

    list_note( out, t->noteloc );

    fprintf(out, "%2d:%-4d  %-*s %5.3lf  ",(int)(p->file+1),(int)(t->lineno),
            stn_name_width, station_code(p->from),0.0);

    fprintf( out, "%*s        ",stn_name_width,"");
    fprintf(out,"%2s%c  ",datatype[type].code,
            t->unused ? '*' : ' ');

    switch( type )
    {
    case LT:  fprintf(out,"%14s  %6.2lf",dms_string( pd->value*RTOD, latobsfmt, NULL ),
                          pd->error * RTOS );
        break;
    case LN:  fprintf(out,"%14s  %6.2lf",dms_string( pd->value*RTOD, lonobsfmt, NULL ),
                          pd->error * RTOS);
        break;
    case OH:
    case EH:  fprintf(out,"%14.4lf  %6.4lf",pd->value, pd->error); \
        break;
    }

    fputs("\n",out);

    {
        classdata *clsf;
        int i;
        clsf = p->clsf + t->iclass;
        for( i = 0; i< t->nclass; i++, clsf++ )
        {
            char *class_name;
            char *class_value;
            class_name = classification_name( &obs_classes, clsf->class_id );
            class_value = class_value_name( &obs_classes, clsf->class_id, clsf->name_id );
            fprintf(out, "     %s = %s\n",class_name,class_value );
        }
    }

    {
        syserrdata *sd;
        int i;
        sd = p->syserr + t->isyserr;
        for( i = 0; i < t->nsyserr; i++, sd++ )
        {
            const char *name;
            name = syserr_name( sd->prm_id );
            fprintf(out,"     Systematic error: %s = %lf",
                    name,sd->influence);
            fprintf(out,"\n");
        }
    }
}


void list_pntdata_residuals( FILE *out, survdata *p, double semult )
{
    double calc, se, ser, sec, seo, sres, redundancy;
    char unused, rfunused;
    void *fmt;
    int ndp;
    int type;
    pntdata *pd;
    double errmult;

    if( !output_rejected_stations && rejected_station( p->from ) ) return;

    pd = p->obs.pdata;
    type = pd->tgt.type;

    set_residual_listing_data_type( out, type );

    list_note( out, pd->tgt.noteloc );

    if( pd->tgt.unused & REJECT_OBS_BIT )
    {
        unused = REJECTED_OBS_FLAG;
    }
    else if ( pd->tgt.unused )
    {
        unused = REJECTED_STN_FLAG;
    }
    else
    {
        unused = ' ';
    }

    fputc('\n',out);

    calc = pd->calc;
    se = pd->error;
    seo = se * semult;
    sec = pd->calcerr*semult;
    ser = pd->reserr*semult;
    sres = pd->sres/semult;
    redundancy = -1.0;
    if( unused == ' ' && se > 0.0 )
    {
        redundancy = pd->reserr/se;
    }

    set_survdata_fields( p );
    set_trgtdata_fields( &pd->tgt, p );
    sprintf( get_field_buffer(OF_TYPE), "%2s%c",
             datatype[type].code, unused);

    ndp = obs_precision[type];

    if( type != LT && type != LN && type != OH && type != EH )
    {
        handle_error( INTERNAL_ERROR, "Invalid PNTDATA type in list_pntdata_residuals",0);
    }

    errmult = 1.0;

    if( type == LT || type == LN )
    {

        if( !latobsfmt ) create_dms_formats();
        errmult = RTOS;
        fmt = (type == LT) ? latobsfmt : lonobsfmt;
        set_residual_field_dms( OF_OBS, fmt, pd->value*RTOD );
        set_residual_field_dms( OF_CALC, fmt, calc*RTOD );
    }
    else
    {
        set_residual_field_value( OF_OBS, ndp, pd->value );
        set_residual_field_value( OF_CALC, ndp, calc );
    }

    set_residual_field_value( OF_OBSERR, ndp, seo * errmult );
    set_residual_field_value( OF_CALCERR, ndp, sec * errmult );
    set_residual_field_value( OF_RES, ndp, pd->residual*errmult );
    set_residual_field_value( OF_RESERR, ndp, ser * errmult );
    set_residual_field_value( OF_MDE, ndp, calculate_mde(seo,ser)*errmult );

    rfunused = unused;
    if( rfunused == ' ' && redundancy < redundancy_flag_level )
    {
        rfunused = LOW_REDUNDANCY_FLAG;
    }


    if( sres < 0.0 )
    {
        set_residual_field( OF_SRES, "-  ");
        sprintf(get_field_buffer(OF_FLAGS),"%c",rfunused);
    }
    else
    {
        set_residual_field_value( OF_SRES, 2, sres );
        sprintf(get_field_buffer(OF_FLAGS),"%c%s",rfunused,
                residual_flag((unused != ' '), 1, sres ));
    }


    if( redundancy < 0 )
    {
        set_residual_field( OF_REDUNDANCY, "-  ");
    }
    else
    {
        set_residual_field_value( OF_REDUNDANCY, 2, redundancy );
    }

    print_residual_line( out );
}


void calc_pntdata_residuals( survdata *p, lsdata *l )
{
    double sres;
    pntdata *pd;
    double error;
    char unused_flag;

    pd = p->obs.pdata;

    if( rejected_station(p->from) || pd->tgt.unused )
    {
        pd->tgt.unused |= UNUSED_OBS_BIT;
    }

    pd->calc   = pd->value - l->res[0];
    pd->calcerr = l->calccvr[0] > 0.0 ? sqrt(l->calccvr[0]) : 0.0;
    pd->residual = l->res[0];
    pd->reserr = l->rescvr[0] > 0.0 ? sqrt(l->rescvr[0]) : 0.0;

    error  = pd->error;

    if( pd->reserr < error * 1.0e-4 )
    {
        sres = -1.0;
    }
    else
    {
        sres = pd->residual/pd->reserr;
        if( sres < 0.0 ) sres = -sres;
    }
    pd->sres = sres;

    if( output_rejected_stations || ! rejected_station( p->from ) )
    {

        if( pd->tgt.unused & REJECT_OBS_BIT )
        {
            unused_flag = REJECTED_OBS_FLAG;
        }
        else if ( pd->tgt.unused )
        {
            unused_flag = REJECTED_STN_FLAG;
        }
        else
        {
            unused_flag = ' ';
        }

        save_residual( p->from, 0, pd->tgt.id, pd->tgt.type,
                       p->file, pd->tgt.lineno, unused_flag, 1, pd->sres, pd->tgt.noteloc );
    }

}
