#include "snapconfig.h"

/* Code for managing vector data */

/*
   $Log: vecdata.c,v $
   Revision 1.6  2004/04/22 02:35:44  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.5  2003/11/25 01:29:59  ccrook
   Updated SNAP to allow calculation of projection bearings in coordinate
   systems other than that of the coordinate file

   Revision 1.4  1999/05/26 07:00:14  ccrook
   Modified to allow listing of worst residuals without listing all residuals

   Revision 1.3  1998/05/21 04:01:49  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.2  1996/02/23 17:03:38  CHRIS
   Added calculation of MDE field to residual listing

   Revision 1.1  1996/01/03 22:13:15  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vecdata.h"
#include "bindata2.h"
#include "output.h"
#include "snap/snapglob.h"
#include "snap/rftrans.h"
#include "rftrnadj.h"
#include "stnobseq.h"
#include "util/lsobseq.h"
#include "snap/survfile.h"
#include "util/geodetic.h"
#include "util/errdef.h"
#include "util/symmatrx.h"
#include "notedata.h"
#include "residual.h"
#include "util/classify.h"
#include "coefs.h"
#include "snapdata/gpscvr.h"
#include "snap/datastat.h"

int vecdata_obseq( survdata  *v, void *hA, int nextra )
{
    vector3 dif;
    vector3 dst1[3], dst2[3];
    station *st1, *st2;
    double hgt1, hgt2;
    vecdata *t;
    int irow, jrow, nrow, iobs, axis;
    ltmat oecvr, veccvr;
    char unused;
    char isvecdif;

    /* Initiallize the observation equations for two station plus up to 7
       parameters plus a possible reference frame scale error */

    init_oe( hA, v->nobs*3, 2*dimension + 8 + nextra, OE_LOWERTRI_CVR );

    isvecdif = v->obs.vdata->tgt.type == GB;

    st1 = isvecdif ? stnptr( v->from ) : NULL;
    hgt1 = v->fromhgt;

    /* Process each observation (vector) in turn */

    for( iobs = 0, t=v->obs.vdata, irow = 1; iobs < v->nobs; iobs++, t++, irow += 3 )
    {

        st2 = stnptr( t->tgt.to );
        hgt2 = t->tgt.tohgt;

        switch ( t->tgt.type )
        {

        case GB:  /* Calculate the vector difference in the local frame */

            calc_vec_dif( st1, hgt1, st2, hgt2, dif, dst1, dst2 );

            /* Convert to the GPS frame (and save parameters in obs.vdata eqns ) */

            vd_rftrans_corr_vector( v->reffrm, dif, dst1, dst2, hA, irow );

            /* Apply the global reference frame scale error */

            for( axis = 0; axis < 3; axis++ )
            {
                dif[axis] += rf_scale_error( dif[axis], hA, irow+axis );
            }
            break;

        case GX: /* Calculate the point vector .. */


            calc_xyz( st2, hgt2, dif, dst2 );

            /* Convert to the GPS frame (and save parameters in obs.vdata eqns ) */

            vd_rftrans_corr_point( v->reffrm, dif, dst2, hA, irow );

            /* Apply the global reference frame scale error */
            /* This is not properly valid for an XYZ position?

            for( axis = 0; axis < 3; axis++ ) {
               dif[axis] += rf_scale_error( dif[axis], hA, irow+axis );
               }
              */
            break;
        }

        /* Copy the data to the observation equations */

        unused = t->tgt.unused ||
                 (isvecdif && rejected_station( v->from )) ||
                 rejected_station( t->tgt.to );

        for( axis = 0; axis < 3; axis++ )
        {
            oe_value( hA, irow+axis, t->vector[axis] - dif[axis] );
            /* set station obseq also applies any station deformation to the
               the observed value */
            if( isvecdif ) set_station_obseq( st1, dst1[axis], hA, irow+axis, v->date );
            set_station_obseq( st2, dst2[axis], hA, irow+axis, v->date );
            if( unused ) oe_flag_unused( hA, irow+axis );
        }
    }

    /* Copy the covariance to the observation equation */

    oecvr = oe_covar_ptr( hA );
    veccvr = v->cvr;
    nrow = v->nobs * 3;

    for( irow = 0; irow < nrow; irow ++ )
        for( jrow = 0; jrow <= irow; jrow++ )
            *oecvr++ = *veccvr++;

    return OK;
}


void list_vecdata( FILE *out, survdata  *v )
{
    vecdata *t;
    int iobs;
    int axis, j;
    char isvecdif;

    fputs("\n", out);

    for(iobs = 0; iobs < v->nobs; iobs++ )
    {
        t = &v->obs.vdata[iobs];
        isvecdif = t->tgt.type == GB;

        list_note( out, t->tgt.noteloc );
        for( axis = 0; axis < 3; axis++ )
        {
            if( axis == 0 )
            {
                fprintf(out,"%2d:%-4d  ",(int)(v->file+1),(int)(t->tgt.lineno));
            }
            else
            {
                fprintf(out,"         ");
            }

            if( isvecdif )
            {
                if( axis == 0 && iobs == 0 )
                {
                    fprintf( out, "%-*s %5.3lf  ", stn_name_width,
                             station_code(v->from), v->fromhgt );
                }
                else
                {
                    fprintf( out, "%*s        ",stn_name_width,"");
                }
            }

            if( axis == 0 )
            {
                fprintf( out, "%-*s %5.3lf  ", stn_name_width,
                         station_code(t->tgt.to), t->tgt.tohgt );
            }
            else
            {
                fprintf( out, "%*s        ",stn_name_width,"");
            }

            if( ! isvecdif ) fprintf( out, "%*s        ",stn_name_width,"");

            if( axis == 0 )
            {
                fprintf( out, "%2s%c  ",datatype[t->tgt.type].code,
                         t->tgt.unused ? '*' : ' ');
            }
            else
            {
                fprintf( out, "     ");
            }

            fprintf(out,"%11.4lf  ",t->vector[axis]);

            if( v->nobs == 1 )
            {
                for( j = 0; j<3; j++ ) fprintf(out,"  %12.8lf",Lij(v->cvr,axis,j));
            }

            if( axis == 0 && iobs == 0 ) fprintf(out,"   %s",
                                                     rftrans_name(v->reffrm) );

            fputs("\n", out );
        }

        {
            classdata *clsf;
            int i;
            clsf = v->clsf + t->tgt.iclass;
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
            sd = v->syserr + t->tgt.isyserr;
            for( i = 0; i < t->tgt.nsyserr; i+=3,sd +=3 )
            {
                const char *name;
                name = syserr_name( sd->prm_id );
                fprintf(out,"     Systematic error: %s = %lf %lf %lf\n",
                        name,sd[0].influence,sd[1].influence,sd[2].influence);
            }
        }

    }


    if( v->nobs > 1 )
    {
        fprintf(out,"\n      Covariance matrix\n");
        print_ltmat( out, v->cvr, v->nobs*3, "%12.8lf", 6 );
    }
}


void list_vecdata_residuals( FILE *out, survdata  *v, double semult )
{
    vecdata *t;
    int axis, iobs;
    char unused, rfunused, firstobs, ndp;
    double vec[3], vecse[3], calc[3], calcse[3], res[3], resse[3];
    double sres[3], redundancy[3];
    double obslength = 0.0, serlim;
    double mde[3];
    char isvecdif;


    if( !output_rejected_stations && rejected_station( v->from ) ) return;

    firstobs = 1;

    for(iobs = 0; iobs < v->nobs; iobs++ )
    {

        t = &v->obs.vdata[iobs];
        isvecdif = t->tgt.type == GB;

        if( output_rejected_stations || !rejected_station( t->tgt.to ) )
        {

            /* Set the unused flag bit */

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

            /* Now get this lot listed out */

            if( firstobs )
            {
                set_residual_listing_data_type( out, t->tgt.type );
                list_file_location( out, v->file, v->obs.vdata[0].tgt.lineno );
            }

            fprintf(out,"\n");
            list_note( out, t->tgt.noteloc );

            /* Determine the parameters to be printed */

            calc_vecdata_vector( v, VD_REF_STN, iobs, VD_OBSVEC | VD_STDERR,
                                 vec, vecse );
            calc_vecdata_vector( v, VD_REF_STN, iobs, VD_CALCVEC | VD_STDERR,
                                 calc, calcse );
            calc_vecdata_vector( v, VD_REF_STN, iobs,
                                 VD_RESVEC | VD_STDERR | VD_TOPOCENTRIC,
                                 res, resse );

            calc_vecdata_vector( v, VD_REF_STN, iobs,
                                 VD_OBSVEC | VD_STDERR | VD_TOPOCENTRIC,
                                 NULL, redundancy );


            serlim = (redundancy[0]+redundancy[1]+redundancy[2])*0.3e-4;

            for( axis = 0; axis < 3; axis ++ )
            {
                mde[axis] = calculate_mde( redundancy[axis], resse[axis] );
                if( resse[axis] < serlim )
                {
                    sres[axis] = -1.0;
                }
                else
                {
                    sres[axis] = res[axis]/resse[axis];
                    if( sres[axis] < 0.0 ) sres[axis] = -sres[axis];
                }
                if( unused == ' ' && redundancy[axis] > 0.0 )
                {
                    redundancy[axis] = resse[axis]/redundancy[axis];
                }
                else
                {
                    redundancy[axis] = -1;
                }
            }

            ndp = 0;
            for( axis = 0; axis < 3; axis ++ )
            {

                if( firstobs )
                {
                    set_survdata_fields(v);
                    firstobs = 0;
                }

                if( axis == 0 )
                {
                    ndp = obs_precision[t->tgt.type];
                    obslength = veclen(t->vector);
                    if( ndp && obslength > 999999.0 ) ndp--;
                    if( obslength <= 0.0 ) obslength = 1.0;  /* Avoid divide by 0 */
                    obslength /= 1.0e6;
                    set_trgtdata_fields(&t->tgt,v);
                    sprintf(get_field_buffer(OF_TYPE),"%2s%c",
                            datatype[t->tgt.type].code, unused );
                }

                set_residual_field_value( OF_OBS, ndp, vec[axis] );
                set_residual_field_value( OF_OBSERR, ndp, vecse[axis]*semult);
                set_residual_field_value( OF_CALC, ndp, calc[axis] );
                set_residual_field_value( OF_CALCERR, ndp, calcse[axis]*semult);
                set_residual_field_value( OF_RES, ndp, res[axis] );
                set_residual_field_value( OF_RESERR, ndp, resse[axis]*semult);
                set_residual_field_value( OF_ALTRES, 2, res[axis]/obslength);
                set_residual_field_value( OF_MDE, ndp, mde[axis] );

                if( sres[axis] < 0.0 )
                {
                    set_residual_field( OF_SRES, "-  ");
                }
                else
                {
                    set_residual_field_value( OF_SRES, 2, sres[axis]/semult );
                }

                if( redundancy[axis] < 0.0 )
                {
                    set_residual_field( OF_REDUNDANCY, "-  ");
                }
                else
                {
                    set_residual_field_value( OF_REDUNDANCY,2,redundancy[axis]);
                }

                rfunused = unused;
                if( rfunused == ' ' && redundancy[axis] >= 0.0
                        && redundancy[axis] < redundancy_flag_level )
                {
                    rfunused = LOW_REDUNDANCY_FLAG;
                }

                if( sres[axis] < 0.0 )
                {
                    sprintf(get_field_buffer(OF_FLAGS),"%c",rfunused);
                }
                else
                {
                    sprintf(get_field_buffer(OF_FLAGS),"%c%s",rfunused,
                            residual_flag( unused == ' ', 1, sres[axis]) );
                }
                print_residual_line( out );
            }

            /* Finally print out the vector standardised residual */

            set_residual_field_value( OF_OBS, ndp, veclen(vec) );
            set_residual_field_value( OF_CALC, ndp, veclen(calc) );
            set_residual_field_value( OF_RES, ndp, veclen(res) );
            set_residual_field_value( OF_ALTRES, 2, veclen(res)/obslength);
            if( t->rank )
            {
                set_residual_field_value( OF_SRES, 2, t->vsres/semult );
                sprintf(get_field_buffer(OF_FLAGS),"%c%s",unused,
                        residual_flag( unused == ' ', t->rank, t->vsres/semult ) );
            }
            else
            {
                set_residual_field(OF_SRES,"-  ");
                sprintf(get_field_buffer(OF_FLAGS),"%c",unused);
            }
            print_residual_line( out );
        }
    }
}




void calc_vecdata_residuals( survdata  *v, lsdata *l )
{
    vecdata *t;
    int axis, iobs, iobs3;
    ltmat ccvr, rcvr, sccvr, srcvr;
    double *lres, res[3], rescvr[6];
    int cvrsize;
    char unused;
    char unused_flag;
    char isvecdif;

    sccvr = l->calccvr;
    ccvr = v->calccvr;
    srcvr = l->rescvr;
    rcvr = v->rescvr;

    isvecdif = v->obs.vdata->tgt.type == GB;

    unused = isvecdif ? rejected_station( v->from ) : 0;

    for(iobs = 0; iobs < v->nobs; iobs++ )
    {


        t = &v->obs.vdata[iobs];
        if( unused || rejected_station(t->tgt.to) || t->tgt.unused )
        {
            t->tgt.unused |= UNUSED_OBS_BIT;
        }

        lres  = l->res  + iobs*3;

        /* Calculate all the values we want to print out */

        for( axis = 0; axis < 3; axis++, lres++ )
        {
            res[axis] = *lres;
            t->residual[axis] = res[axis];
            t->calc[axis] = t->vector[axis] - res[axis];
        }

        /* Work out a standardised residual for the vector */
        /* Note the this corrupts rescvr and the copy of res */

        iobs3 = iobs*3;
        rescvr[0] = Lij(srcvr,iobs3, iobs3 );
        rescvr[1] = Lij(srcvr,iobs3+1,iobs3);
        rescvr[2] = Lij(srcvr,iobs3+1,iobs3+1);
        rescvr[3] = Lij(srcvr,iobs3+2,iobs3);
        rescvr[4] = Lij(srcvr,iobs3+2,iobs3+1);
        rescvr[5] = Lij(srcvr,iobs3+2,iobs3+2);

        t->vsres = vector_standardised_residual( res, rescvr, &t->rank );
        if( !t->rank ) t->vsres = -1.0;

        /* Save the residual ... */

        if( output_rejected_stations ||
                ((!isvecdif || !rejected_station( v->from )) && !rejected_station( t->tgt.to )) )
        {

            /* Set the unused flag bit */

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

            /* Save the residual */

            save_residual( v->from, t->tgt.to, t->tgt.id, t->tgt.type,
                           v->file, t->tgt.lineno, unused_flag, t->rank, t->vsres,
                           t->tgt.noteloc );
        }

    }

    /* Copy across the covariance matrices */

    iobs = v->nobs*3;
    cvrsize = (iobs * (iobs+1))/2;
    while( cvrsize-- ) { *ccvr++ = *sccvr++; *rcvr++ = *srcvr++; }
}

