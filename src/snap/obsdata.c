#include "snapconfig.h"
/*
   $Log: obsdata.c,v $
   Revision 1.6  2004/04/22 02:35:43  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.5  2003/11/25 01:29:58  ccrook
   Updated SNAP to allow calculation of projection bearings in coordinate
   systems other than that of the coordinate file

   Revision 1.4  1999/05/26 07:01:03  ccrook
   Modified to allow listing of worst residuals without listing all residuals

   Revision 1.3  1998/05/21 04:01:50  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.2  1996/02/23 17:02:16  CHRIS
   Added calculation of MDE field to residual listing

   Revision 1.1  1996/01/03 22:02:29  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "snap/snapglob.h"
#include "output.h"
#include "obsdata.h"
#include "bindata2.h"
#include "snapdata/loaddata.h"
#include "snap/stnadj.h"
#include "stnobseq.h"
#include "util/classify.h"
#include "coefs.h"
#include "util/lsobseq.h"
#include "residual.h"
#include "snap/bearing.h"
#include "util/dms.h"
#include "notedata.h"
#include "snap/datastat.h"
#include "util/errdef.h"
#include "util/pi.h"


static void *hafmt = NULL;
static void *azfmt = NULL;
static void *pbfmt = NULL;
static void *zdfmt = NULL;

static void *angle_format( int type )
{
    if( !hafmt )
    {
        hafmt = create_dms_format( 3,obs_precision[HA],0,NULL,NULL,NULL,NULL,NULL);
        azfmt = create_dms_format( 3,obs_precision[AZ],0,NULL,NULL,NULL,NULL,NULL);
        pbfmt = create_dms_format( 3,obs_precision[PB],0,NULL,NULL,NULL,NULL,NULL);
        zdfmt = create_dms_format( 3,obs_precision[ZD],0,NULL,NULL,NULL,NULL,NULL);
    }
    if( type == HA ) return hafmt;
    if( type == PB ) return pbfmt;
    if( type == AZ ) return azfmt;
    if( type == ZD ) return zdfmt;
    return hafmt;
}


void list_obsdata( FILE *out, survdata *o )
{
    obsdata *t;
    int iobs, to;
    double tohgt;
    int type;

    fputs("\n", out);

    to = -1;
    tohgt = 0.0;

    for(iobs = 0; iobs < o->nobs; iobs++ )
    {
        t = &o->obs.odata[iobs];
        type = t->tgt.type;
        list_note( out, t->tgt.noteloc );
        fprintf(out,"%2d:%-4d  ",(int)(o->file+1),(int)(t->tgt.lineno));
        if( iobs == 0 )
        {
            fprintf( out, "%-*s %5.3lf  ", stn_name_width,station_code(o->from), o->fromhgt );
        }
        else
        {
            fprintf( out, "%*s        ",stn_name_width,"");
        }

        if( t->tgt.to != to || t->tgt.tohgt != tohgt )
        {
            fprintf( out, "%-*s %5.3lf  ", stn_name_width,station_code(t->tgt.to), t->tgt.tohgt );
            to = t->tgt.to;
            tohgt = t->tgt.tohgt;
        }
        else
        {
            fprintf( out, "%*s        ",stn_name_width,"");
        }

        fprintf( out, "%2s%c  ",datatype[type].code, t->tgt.unused ? '*' : ' ');

        if( datatype[type].isangle )
        {
            fprintf( out, "%11s   %6.1lf",dms_string(t->value*RTOD,angle_format(type),NULL),t->error*RTOS);
        }
        else
        {
            fprintf( out, "%11.4lf   %6.4lf",t->value,t->error);
        }

        if( type == ZD )
        {
            fprintf( out, "   %s", refcoef_name( t->refcoef ) );
        }
        else if( t->prm_id && (type == ED || type == MD || type == HD || type == SD || type == DR ))
        {
            fprintf( out, "   %s", distsf_name( t->prm_id ));
        }
        else if( t->prm_id && (type == AZ || type == PB) )
        {
            fprintf( out, "   %s", brngref_name( t->prm_id ));
        }
        fputs("\n", out );

        {
            classdata *clsf;
            int i;
            clsf = o->clsf + t->tgt.iclass;
            for( i = 0; i< t->tgt.nclass; i++, clsf++ )
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
            sd = o->syserr + t->tgt.isyserr;
            for( i = 0; i < t->tgt.nsyserr; i++, sd++ )
            {
                const char *name;
                name = syserr_name( sd->prm_id );
                fprintf(out,"     Systematic error: %s = %lf\n",name,sd->influence);
            }
        }

    }
}


int obsdata_obseq( survdata *o, void *hA, int nextra )
{
    station *st1, *st2;
    vector3 dst1, dst2;
    double hgt1, hgt2, dhgt1, dhgt2;
    double value, cvr;
    double refaz;
    int firstha;
    int iobs;
    int obsstatus;
    int status;
    obsdata *t;

    status = OK;

    /* Initiallize for the two stations plus up to 3 extra parameters */

    init_oe( hA, o->nobs, 2*dimension+3+nextra, OE_DIAGONAL_CVR );

    st1 = stnptr( o->from );
    hgt1 = o->fromhgt;

    firstha = 1;
    refaz = 0.0;

    for( iobs = 0, t=o->obs.odata; iobs++ < o->nobs; t++ )
    {

        st2 = stnptr( t->tgt.to );
        hgt2 = t->tgt.tohgt;

        /* Check that the stations are not collocated.  Calculate distance
           type appropriate to observations, and if it is 0 then set the error
         status and continue.  Set value to 1.0 so initially so that untested
         types don't flag an error. */

        value = 1.0;
        switch( t->tgt.type )
        {
        case SD:
        case DR:
        case ZD:
            value = calc_distance( st1, hgt1, st2, hgt2, dst1, dst2 );
            break;

        case HD:
            dhgt1 = (st2->OHgt+hgt2 - (st1->OHgt+hgt1))/2.0;
            dhgt2 = -dhgt1;
            value = calc_distance( st1, hgt1+dhgt1, st2, hgt2+dhgt2, dst1, dst2 );
            break;


        case ED:
        case HA:
        case PB:
        case AZ:
            value = calc_ellipsoidal_distance( st1, st2,dst1, dst2 );
            break;

        case MD:
            value = calc_msl_distance( st1, st2,dst1, dst2 );
            break;
        }

        if( value < 0.0001 )
        {
            char buf[256];
            sprintf(buf,"Unable to calculate %s obs from colocated stations %s to %s - obs not used",
                    datatype[t->tgt.type].name,st1->Code,st2->Code );

            handle_error( WARNING_ERROR, buf, NO_MESSAGE );
            status = INVALID_DATA;
            continue;
        }


        /* Note: for distance data types the value has already been calculated by the
           above check */

        switch( t->tgt.type )
        {
        case SD:
        case DR:  value = calc_distance( st1, hgt1, st2, hgt2, dst1, dst2 );
            value += rf_scale_error( value, hA, iobs );
            value += distsf_correction( t->prm_id, value, hA, iobs );
            if( t->tgt.type == DR )
            {
                oe_schreiber( hA, iobs, value*1.0e-6 );
            }
            break;

            /* For apply scale corrections to horizontal distances */

        case HD:
        case ED:
        case MD:  value += rf_scale_error( value, hA, iobs );
            value += distsf_correction( t->prm_id, value, hA, iobs );
            break;

            /* For azimuths need to correct for multiples of 2*pi */

        case HA:
        case AZ:  value = calc_azimuth( st1, hgt1, st2, hgt2,
                                            t->tgt.type == HA, dst1, dst2 );
            value = t->value - value;
            if( t->tgt.type == HA )
            {
                if( firstha ) { refaz = value; firstha = 0; }
                value -= refaz;
                oe_schreiber( hA, iobs, 1.0);
            }
            else
            {
                value -= brngref_correction( t->prm_id, hA, iobs );
            }

            while( value > PI ) value -= PI*2;
            while( value <- PI ) value += PI*2;
            value = t->value - value;
            break;

        case PB:  obsstatus = calc_prj_azimuth2( o->reffrm,
                                  st1, hgt1, st2, hgt2, &value, dst1, dst2 );
            if( obsstatus != OK )
            {
                char buf[256];
                sprintf(buf,"Unable to calculate %s projection bearing from %s to %s - obs not used",
                        bproj_name(o->reffrm),st1->Code,st2->Code );
                handle_error( WARNING_ERROR, buf, NO_MESSAGE );
                continue;
            }
            value = t->value - value;
            value -= brngref_correction( t->prm_id, hA, iobs );
            while( value > PI ) value -= PI*2;
            while( value < -PI ) value += PI*2;
            value = t->value - value;
            break;


            /* For zenith distances apply a refraction correction */

        case ZD:  value = calc_zenith_dist( st1, hgt1, st2, hgt2, dst1, dst2 );
            value += zd_ref_correction( t->refcoef, st1, st2, hA, iobs );
            break;

        case LV:  value = calc_hgt_diff( st1, hgt1, st2, hgt2, dst1, dst2 );
            break;
        }

        oe_value( hA, iobs, t->value - value );

        /* set station obseq also applies any station deformation to the
           the observed value */
        set_station_obseq( st1, dst1, hA, iobs, o->date );
        set_station_obseq( st2, dst2, hA, iobs, o->date );


        cvr = t->error;
        cvr *= cvr;
        oe_covar( hA, iobs, iobs, cvr );

        if( t->tgt.unused || rejected_station( o->from ) || rejected_station (t->tgt.to ))
        {
            oe_flag_unused( hA, iobs );
        }
    }
    return status;
}


void list_obsdata_residuals( FILE *out, survdata *o, double semult )
{
    obsdata *t;
    int iobs, to, itype, maxtype, type, ndp;
    double calc, seo, sec, ser, sres, redundancy;
    char unused, rfunused, firstobs, firstoutput;
    char ok;
    char distratios;
    char *typecode;
    double obslength;
    double altres;
    double mde;


    if( !output_rejected_stations && rejected_station( o->from ) ) return;

    maxtype = output_sort_by_type ? NOBSTYPE : 1;

    firstoutput = 1;

    for( itype = 0; itype < maxtype; itype++ )
    {
        firstobs = 1;
        to = -1;
        distratios = 0;

        for(iobs = 0; iobs < o->nobs; iobs++ )
        {

            t = &o->obs.odata[iobs];
            type = t->tgt.type;

            ok = output_sort_by_type && t->tgt.type == itype;

            if( ok ) ok = output_rejected_stations || !rejected_station( t->tgt.to );

            if( ok )
            {

                if( set_residual_listing_data_type( out, t->tgt.type ) )
                {
                    firstoutput = 1;
                    firstobs = 1;
                    to = -1;
                }

                if( firstoutput )
                {
                    list_file_location( out, o->file, o->obs.odata[0].tgt.lineno );
                    firstoutput = 0;
                }

                list_note( out, t->tgt.noteloc );

                if( firstobs )
                {
                    fprintf(out,"\n");
                    set_survdata_fields(o);
                    firstobs = 0;
                }

                if( t->tgt.to != to )
                {
                    set_trgtdata_fields( &t->tgt, o );
                    to = t->tgt.to;
                }


                if( t->tgt.unused & REJECT_OBS_BIT )
                {
                    unused = REJECTED_OBS_FLAG;
                }
                else if( t->tgt.unused )
                {
                    unused = REJECTED_STN_FLAG;
                }
                else
                {
                    unused = ' ';
                }

                typecode = get_field_buffer( OF_TYPE );
                strcpy( typecode, datatype[type].code );
                if( unused != ' ' )
                {
                    int l = strlen(typecode );
                    typecode[l++] = unused;
                    typecode[l] = 0;
                }

                calc = t->calc;
                seo = t->error * semult;
                sec = t->calcerr*semult;
                ser = t->reserr*semult;
                sres = t->sres;
                if( sres >= 0.0 ) sres /= semult;
                mde = calculate_mde( seo, ser );

                if( unused == ' ' && t->error > 0.0 )
                {
                    redundancy = t->reserr/t->error;
                }
                else
                {
                    redundancy = -1;
                }

                ndp = obs_precision[type];
                if( datatype[type].isangle )
                {
                    calc *= RTOD;
                    if( calc < 0.0 ) calc += 360.0;
                    if( calc >= 360.0 ) calc -= 360.0;
                    set_residual_field_dms( OF_OBS, angle_format(type), t->value*RTOD);
                    set_residual_field_value( OF_OBSERR, ndp, seo*RTOS );
                    set_residual_field_dms( OF_CALC, angle_format(type), calc );
                    set_residual_field_value( OF_CALCERR, ndp, sec*RTOS );
                    set_residual_field_value( OF_RES, ndp, t->residual*RTOS );
                    set_residual_field_value( OF_RESERR, ndp, ser*RTOS );
                    set_residual_field_value( OF_MDE, ndp, mde*RTOS );
                }
                else
                {
                    set_residual_field_value( OF_OBS    , ndp, t->value );
                    set_residual_field_value( OF_OBSERR , ndp, seo );
                    set_residual_field_value( OF_CALC   , ndp, calc );
                    set_residual_field_value( OF_CALCERR, ndp, sec );
                    set_residual_field_value( OF_RES    , ndp, t->residual );
                    set_residual_field_value( OF_RESERR , ndp, ser );
                    set_residual_field_value( OF_MDE, ndp, mde );
                }

                /* Calculate an alternative form for the residual */

                switch( type )
                {
                case SD:
                case ED:
                case MD:
                case HD:
                case DR:
                    if( t->value > 0.0 )
                    {
                        set_residual_field_value( OF_ALTRES, 2,
                                                  1.0e6*t->residual/t->value );
                    }
                    break;

                case AZ:
                case HA:
                case ZD:
                case PB:
                    obslength = calc_distance( stnptr(o->from), 0.0, stnptr(t->tgt.to), 0.0,NULL,NULL);
                    altres = t->residual*obslength;
                    if( type == ZD ) altres *= sin( t->value );
                    set_residual_field_value( OF_ALTRES, 3, altres );
                    break;

                case LV:
                    obslength = calc_distance( stnptr(o->from), 0.0, stnptr(t->tgt.to), 0.0,NULL,NULL);
                    if( obslength > 0 )
                    {
                        set_residual_field_value( OF_ALTRES, 2,
                                                  1.0e6*t->residual/obslength );
                    }
                    break;
                }


                if( sres < 0 )
                {
                    set_residual_field( OF_SRES, "-  " );
                }
                else
                {
                    set_residual_field_value( OF_SRES, 2, sres );
                }

                if( redundancy < 0 )
                {
                    set_residual_field( OF_REDUNDANCY, "-  ");
                }
                else
                {
                    set_residual_field_value( OF_REDUNDANCY, 2, redundancy );
                }

                rfunused = unused;
                if( rfunused == ' ' && redundancy > -0.5 && redundancy < redundancy_flag_level )
                {
                    rfunused = LOW_REDUNDANCY_FLAG;
                }

                sprintf(get_field_buffer(OF_FLAGS),"%c%s",rfunused,
                        residual_flag((unused != ' '), 1, sres ));

                print_residual_line( out );

                if( type == DR ) distratios = 1;
            }

        }

        if( distratios )
        {
            double se;
            se = o->schvar;
            if( se > 0.0 ) se = sqrt(se);
            se *= semult;

            fprintf(out,"\n%*sDistance scale factor %.1lfppm +/- %.1lfppm\n",
                    stn_name_width*2+2,"",o->schval,se );
        }

    }
}




void calc_obsdata_residuals( survdata *o, lsdata *l )
{
    obsdata *t;
    int iobs;
    ltmat ccvr, rcvr;
    double sres;
    char unused;
    char unused_flag;

    ccvr = l->calccvr;
    rcvr = l->rescvr;

    o->schval = l->sch;
    o->schvar = l->schvar;

    unused = rejected_station( o->from );

    for(iobs = 0; iobs < o->nobs; iobs++ )
    {

        t = &o->obs.odata[iobs];
        if( unused || rejected_station(t->tgt.to) || t->tgt.unused )
            t->tgt.unused |= UNUSED_OBS_BIT;

        t->calc = t->value - l->res[iobs];
        t->calcerr = *ccvr > 0.0 ? sqrt(*ccvr) : 0.0;
        t->residual = l->res[iobs];
        t->reserr = *rcvr > 0.0 ? sqrt(*rcvr) : 0.0;
        if( t->reserr < t->error*1.0e-4 )
        {
            t->sres = -1.0;
        }
        else
        {
            sres = l->res[iobs]/t->reserr;
            t->sres = sres >= 0.0 ? sres : -sres;
        }

        ccvr++;
        rcvr++;
        if( !l->diagonal )
        {
            ccvr += iobs;
            rcvr += iobs;
        }

        if( output_rejected_stations ||
                (! rejected_station( o->from ) && !rejected_station( t->tgt.to )) )
        {
            if( t->tgt.unused & REJECT_OBS_BIT )
            {
                unused_flag = REJECTED_OBS_FLAG;
            }
            else if( t->tgt.unused )
            {
                unused_flag = REJECTED_STN_FLAG;
            }
            else
            {
                unused_flag = ' ';
            }

            save_residual( o->from, t->tgt.to, t->tgt.id, t->tgt.type,
                           o->file, t->tgt.lineno, unused_flag, 1, t->sres, t->tgt.noteloc );
        }
    }
}
