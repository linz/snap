#include "snapconfig.h"
/* NOTE: Haven't done anything with units here yet!! */

/*
   $Log: crdsysc2.c,v $
   Revision 1.2  2004/04/22 02:34:21  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 16:26:12  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_hrs_func.h"
#include "util/chkalloc.h"
#include "util/dateutil.h"
#include "util/errdef.h"
#include "util/pi.h"

/* Conversion of coordinates from one coordinate system to another     */
/* Converts coordinates, deflections, and undulations.  Input coords   */
/* are either projection northing, easting, or lat and long in radians */
/* Input heights are ellipsoidal.  Input deflections are in radians    */
/* Output coordinates may be written to the same vector as input, i.e. */
/* tneh == fneh is valid.  Input and output deflections/undulations    */
/* may be NULL.  Input are treated as 0,0,0 - output are ignored.      */


static int define_coord_conversion_base( coord_conversion *conv,
    coordsys *from, coordsys *to, double convepoch, int ellipsoidal )
{

    char conv_el;
    conv->from = from;
    conv->to = to;
    conv->ncrf = 0;
    conv->nhrf_from=0;
    conv->nhrf_to=0;
    conv->epochconv = convepoch;
    conv->needsepoch = 0;
    conv->errmsg[0] = 0;
    conv->need_xyz = 0;

    conv->valid = 0;

    /* Can we define a conversion epoch from the coordinate system definitions */

    if( convepoch == UNDEFINED_DATE )
    {
        if( from->rf->defepoch == UNDEFINED_DATE )
        {
            convepoch=to->rf->defepoch;
        }
        else if( to->rf->defepoch == UNDEFINED_DATE )
        {
            convepoch=from->rf->defepoch;
        }
        else if( from->rf->defepoch == to->rf->defepoch )
        {
            convepoch=from->rf->defepoch;
        }
    }
    conv->epochconv = convepoch;

    /* If not using the same the must have common base reference frame codes,
     */

    {
        char *common_rf=0;
        char *from_code=from->rf->code;
        ref_frame *from_rf=from->rf;
        int nfrom=0;
        int nto=0;
        int changeepoch=0; /* Different deformation ref epoch */

        /* Track from each reference frame to find a common base */

        while( ! common_rf )
        {
            char *to_code=to->rf->code;
            ref_frame *to_rf=to->rf;
            nto=0;
            while( ! common_rf )
            {
                if( _stricmp(from_code, to_code) == 0 )
                {
                    common_rf=from_code;
                    /* If have reached a common actual reference frame (rather than
                     * base code) then check we have the same deformation reference
                     * epoch */
                    if( from_rf && to_rf )
                    {
                        if( from_rf->defepoch != to_rf->defepoch )
                        {
                            changeepoch=1;
                            nfrom++;
                            nto++;
                        }
                    }
                    break;
                }
                if( ! to_rf ) break;
                if( to_rf )
                {
                    to_code=to_rf->refcode;
                    to_rf=to_rf->refrf;
                    nto++;
                }
                if( nfrom+nto > CONVMAXRF ) break;
                if( ! to_code ) break;
            }
            if( common_rf || ! from_rf ) break;
            if( from_rf )
            {
                from_code=from_rf->refcode;
                from_rf=from_rf->refrf;
                nfrom++;
            }
            if( nfrom > CONVMAXRF ) break;
            if( ! from_code ) break;
        }

        /* If there is no common rf then cannot do conversion */

        if( ! common_rf )
        {
            conv->valid=0;
            sprintf(conv->errmsg,
                    "Conversion between coordinate systems %.20s and %.20s is not possible",
                    from->code, to->code);
        }

        else if( nfrom+nto > CONVMAXRF  )
        {
            conv->valid=0;
            sprintf(conv->errmsg,
                    "Conversion between coordinate systems %.20s and %.20s is too complex (> %d steps)",
                    from->code, to->code, CONVMAXRF );
        }
        else
        {
            ref_frame *crf=from->rf;
            ref_frame *nextrf;
            int i;
            int needsepoch=0;

            for( i=0; i<nfrom; i++, crf=crf->refrf )
            {
                conv->crf[i].rf=crf;
                conv->crf[i].xyz_to_std=1;
                conv->crf[i].def_only=0;
                conv->crf[i].need_xyz=0;
            }
            crf=to->rf;
            for( i=nto; i-- > 0; crf=crf->refrf )
            {
                conv->crf[i+nfrom].rf=crf;
                conv->crf[i+nfrom].xyz_to_std=0;
                conv->crf[i+nfrom].def_only=0;
                conv->crf[i+nfrom].need_xyz=0;
            }
            /* If we are changing epoch, then the common reference frame is
             * included in the transformation, but don't need to apply xyz_to_std
             * and back (as it would be a null transformation)
             */
            if( changeepoch )
            {
                conv->crf[nfrom-1].def_only=1;
                conv->crf[nfrom].def_only=1;
            }
            conv->ncrf=nfrom+nto;
            conv->valid = 1;

            nextrf=to->rf;
            for( i=conv->ncrf; i--;)
            {
                crf=conv->crf[i].rf;

                /* Check  whether changing ellipsoid, so must have xyz coords */
                if( ! identical_ellipsoids(crf->el,nextrf->el) ) conv->crf[i].need_xyz=1;
                nextrf=crf;
                if( ! needsepoch && ! conv->crf[i].def_only )
                {
                    if( crf->def ||
                            crf->dtxyz[0] != 0.0 ||
                            crf->dtxyz[1] != 0.0 ||
                            crf->dtxyz[2] != 0.0 ||
                            crf->drxyz[0] != 0.0 ||
                            crf->drxyz[1] != 0.0 ||
                            crf->drxyz[2] != 0.0 ||
                            crf->dscale != 0.0 )
                        needsepoch =1;
                }
            }
            if( ! identical_ellipsoids(from->rf->el,nextrf->el) ) conv->need_xyz=1;

            if( needsepoch && convepoch==0.0 )
            {
                conv->valid=0;
                sprintf(conv->errmsg,
                        "Conversion between reference frames %.20s and %.20s requires a date",
                        from->rf->code, to->rf->code);
            }
            conv->needsepoch = needsepoch;
        }
    }

    conv_el = conv->ncrf > 0;
    if( !conv_el )
    {
        conv_el = !identical_ellipsoids( from->rf->el, to->rf->el );
    }

    conv->from_prj=is_projection(from);
    conv->to_prj=is_projection(to);
    conv->from_geoc = is_geocentric(from);
    conv->to_geoc = is_geocentric(to);

    /* Handle vertical datums */

    if( ! ellipsoidal && (from->hrs || to->hrs) )
    {
        int nhrf_from=0;
        int nhrf_to=0;
        vdatum *fhrs;
        vdatum *thrs;
        for( fhrs=from->hrs; fhrs; fhrs=fhrs->basehrs ) nhrf_from++;
        for( thrs=to->hrs; thrs; thrs=thrs->basehrs ) nhrf_to++;

        /* If have vertical datum conversion on from and to, and identical
         * reference frames, then see if we can reduce the number of 
         * steps */

        if(  nhrf_from && nhrf_to && ! conv_el )
        {
            int nmin=nhrf_from > nhrf_to ? nhrf_to : nhrf_from;
            int nhrff=nhrf_from;
            int nhrft=nhrf_to;
            fhrs=from->hrs;
            thrs=to->hrs;
            while( nhrff > nmin ){ nhrff--; fhrs=fhrs->basehrs; }
            while( nhrft > nmin ){ nhrft--; thrs=thrs->basehrs; }
            while( nhrff > 0 )
            {
                nhrff--;
                if( ! identical_vdatum_func( fhrs->func, thrs->func ) ) nmin=nhrff;
                fhrs=fhrs->basehrs;
                thrs=thrs->basehrs;
            }
            nhrf_from -= nmin;
            nhrf_to -= nmin;
        }

        if( nhrf_from+nhrf_to > CONVMAXRF  )
        {
            conv->valid=0;
            sprintf(conv->errmsg,
                    "Conversion between vertical datums %.20s and %.20s is too complex (> %d steps)",
                    from->hrs ? from->hrs->code : "ellipsoid", 
                    to->hrs ? to->hrs->code : "ellipsoid", 
                    CONVMAXRF );
        }
        else
        {
            int nlast=nhrf_from+nhrf_to-1;
            fhrs=from->hrs;
            thrs=to->hrs;
            for( int i=0; i<nhrf_from; i++ ){ conv->hrf[i]=fhrs->func; fhrs=fhrs->basehrs; }
            for( int i=0; i<nhrf_to; i++ ){ conv->hrf[nlast-i]=thrs->func; thrs=thrs->basehrs; }
            conv->nhrf_from=nhrf_from;
            conv->nhrf_to=nhrf_to;
        }
    }

    if( ! conv->valid ) return INVALID_DATA;
    return OK;
}

int define_coord_conversion( coord_conversion *conv,
                             coordsys *from, coordsys *to )
{
    return define_coord_conversion_base( conv, from, to, 0.0, 0 );
}

int define_coord_conversion_epoch( coord_conversion *conv,
    coordsys *from, coordsys *to, double convepoch )
{
    return define_coord_conversion_base( conv, from, to, convepoch, 0 );
}

int define_ellipsoidal_coord_conversion_epoch( coord_conversion *conv,
    coordsys *from, coordsys *to, double convepoch )
{
    return define_coord_conversion_base( conv, from, to, convepoch, 1 );
}


coordsys *conversion_from_coordsys( coord_conversion *conv )
{
    return conv->from;
}

coordsys *conversion_to_coordsys( coord_conversion *conv )
{
    return conv->to;
}

static int check_crdsys_range( coordsys *cs, double llh[3] )
{
    if( llh[CRD_LAT] < cs->ltmin || llh[CRD_LAT] > cs->ltmax ) return 0;
    while( llh[CRD_LON] < cs->lnmin ) llh[CRD_LON] += TWOPI;
    while( llh[CRD_LON] > cs->lnmax ) llh[CRD_LON] -= TWOPI;
    if( llh[CRD_LON] < cs->lnmin ) return 0;
    return 1;
}


int convert_coords( coord_conversion *conv,
                    double *fenh, double *fexu,
                    double *tenh, double *texu )
{

    int i;
    double xyz[3], gllh[3];
    coordsys *from, *to;
    int geoid, in_range;
    int sts;
    int isgeoc;

    sts = OK;

    /* Do the reference frames have a consistent frame */

    if( ! conv->valid ) return INVALID_DATA;

    /* Clear out any old error message */
    conv->errmsg[0] = 0;

    from = conv->from;
    to = conv->to;

    geoid = texu != 0;

    for( i = 0; i<3; i++ )
    {
        xyz[i] = fenh[i];
        gllh[i] = fexu ? fexu[i] : 0.0;
    }

    /* Convert from projection to lat long height */

    if( conv->from_prj ) proj_to_geog( from->prj,
                                           xyz[CRD_EAST], xyz[CRD_NORTH], xyz+CRD_LON, xyz+CRD_LAT );
    isgeoc = conv->from_geoc;

    /* Convert the undulation to orthometric height, and convert the
     deflections to astronomical lats, longs */

    if( geoid )
    {
        if( isgeoc ) { xyz_to_llh( from->rf->el, xyz, xyz ); isgeoc=0; }
        gllh[CRD_LAT] = xyz[CRD_LAT] + gllh[CRD_LAT];
        gllh[CRD_LON] = xyz[CRD_LON] + gllh[CRD_LON]/cos(xyz[CRD_LAT]);
        gllh[CRD_HGT] = xyz[CRD_HGT] - gllh[CRD_HGT];
    }

    if( sts == OK && conv->nhrf_from )
    {
        if( isgeoc ) { xyz_to_llh( from->rf->el, xyz, xyz ); isgeoc=0; }
        for( int i=0; sts==OK && i<conv->nhrf_from; i++ )
        {
            double offset;
            sts=calc_vdatum_func( conv->hrf[i], xyz, &offset, 0 );
            xyz[CRD_HGT] += offset;
        }
        if( sts != OK )
        {
            sprintf(conv->errmsg,
                 "Cannot calculate %s height",
                                    conv->from->hrs->code);
        }
    }

    if( conv->need_xyz && ! isgeoc )
    {
        llh_to_xyz( from->rf->el, xyz, xyz, 0, 0 );
        isgeoc=1;
    }

    if( conv->ncrf )
    {
        int i;
        for( i=0; i < conv->ncrf; i ++ )
        {
            coord_conversion_rf *crf = &(conv->crf[i]);
            ref_frame *rf=crf->rf;
            int defonly=crf->def_only;

            /* If the system has a deformation model to apply */
            if( crf->xyz_to_std )
            {
                if( rf->def )
                {
                    double tgtepoch= defonly ? 0.0 : conv->epochconv;
                    if( isgeoc ) { xyz_to_llh( rf->el, xyz, xyz ); isgeoc=0; }
                    if( tgtepoch != rf->defepoch )
                    {
                        if( geoid ) { int ia; for( ia=0; ia<3; ia++ ) gllh[i]-=xyz[i]; }
                        sts=apply_ref_deformation_llh( rf, xyz, rf->defepoch, tgtepoch );
                        if( sts != OK ) 
                        {
                            sprintf(conv->errmsg,
                                    "Cannot apply %s deformation model",
                                    rf->code);
                            break;
                        }
                        if( geoid ) { int ia; for( ia=0; ia<3; ia++ ) gllh[i]+=xyz[i]; }
                    }
                }
                /*  Apply change to reference frame axes */
                if( ! defonly )
                {
                    if( ! isgeoc ) { llh_to_xyz( rf->el, xyz, xyz, 0, 0 ); isgeoc=1; }
                    sts = xyz_to_std( rf, xyz, conv->epochconv );
                    if( sts != OK ) 
                    {
                        sprintf(conv->errmsg,
                                "Cannot convert from %s to %s",
                                rf->code, rf->refcode ? rf->refcode : "base" );
                        break;
                    }
                }
            }
            else
            {
                if( ! defonly )
                {
                    if( ! isgeoc ) { llh_to_xyz( rf->el, xyz, xyz, 0, 0 ); isgeoc=1; }
                    sts = std_to_xyz( rf, xyz, conv->epochconv );
                    if( sts != OK ) 
                    {
                        if( sts != OK ) 
                        {
                            sprintf(conv->errmsg,
                                    "Cannot convert from %s to %s",
                                    rf->refcode ? rf->refcode : "base", rf->code );
                            break;
                        }
                    }
                }
                /*  Apply deformation model */
                if( rf->def )
                {
                    double srcepoch=defonly ? 0.0 : conv->epochconv;
                    if( isgeoc ) { xyz_to_llh( rf->el, xyz, xyz ); isgeoc=0; }
                    if( srcepoch != rf->defepoch )
                    {
                        if( geoid ) { int ia; for( ia=0; ia<3; ia++ ) gllh[i]-=xyz[i]; }
                        sts=apply_ref_deformation_llh( rf, xyz, srcepoch, rf->defepoch );
                        if( sts != OK ) 
                        {
                            sprintf(conv->errmsg,
                                    "Cannot apply %s deformation model",
                                    rf->code);
                            break;
                        }
                        if( geoid ) { int ia; for( ia=0; ia<3; ia++ ) gllh[i]+=xyz[i]; }
                    }
                }
            }

            if( crf->need_xyz && ! isgeoc )
            {
                llh_to_xyz( rf->el, xyz, xyz, 0, 0 );
                isgeoc=1;
            }
        }
    }

    /* If output coordinates are on orthometric surface */

    if( sts == OK && conv->nhrf_to )
    {
        if( isgeoc ) { xyz_to_llh( from->rf->el, xyz, xyz ); isgeoc=0; }
        for( int i=0; sts==OK && i<conv->nhrf_to; i++ )
        {
            double offset;
            sts=calc_vdatum_func( conv->hrf[conv->nhrf_from+i], xyz, &offset, 0 );
            xyz[CRD_HGT] -= offset;
        }
        if( sts != OK )
        {
            sprintf(conv->errmsg,
                 "Cannot calculate %s height",
                                    conv->to->hrs->code);
        }
    }

    /* Recompute the deflections etc */
    if( sts == OK && geoid )
    {
        if( geoid )
        {
            if( isgeoc ) { xyz_to_llh( to->rf->el, xyz, xyz ); isgeoc=0; }
            gllh[CRD_LAT] = gllh[CRD_LAT] - xyz[CRD_LAT];
            gllh[CRD_LON] = (gllh[CRD_LON] - xyz[CRD_LON]) * cos(xyz[CRD_LAT]);
            gllh[CRD_HGT] = xyz[CRD_HGT] - gllh[CRD_HGT];
        }
    }

    /* Check that the points lie within a valid range for the coordinate
       system */

    if( sts == OK )
    {
        if( to->gotrange && to->rf->el )
        {
            if( isgeoc ) { xyz_to_llh( to->rf->el, xyz, xyz ); isgeoc=0; }
            in_range = check_crdsys_range( to, xyz );
            if( ! in_range )
            {
                sts=INCONSISTENT_DATA;
                sprintf(conv->errmsg,
                        "Converted coordinates are outside range of %.20s coordinate system",
                        to->code);
            }
        }
        else
        {
            in_range = 1;
        }

        /* Convert back to a projection/geocentric if required */

        if( in_range && conv->to_prj )
        {
            if( isgeoc ) { xyz_to_llh( to->rf->el, xyz, xyz ); isgeoc=0; }
            geog_to_proj( to->prj,
                          xyz[CRD_LON], xyz[CRD_LAT], xyz+CRD_EAST, xyz+CRD_NORTH );
        }
        else if( conv->to_geoc )
        {
            if( ! isgeoc ) llh_to_xyz( to->rf->el, xyz, xyz, 0, 0 );
        }
        else if( isgeoc )
        {
            xyz_to_llh( to->rf->el, xyz, xyz ); isgeoc=0;
        }
    }

    /* And copy back into the output vectors */

    if( sts == OK )
    {
        if( tenh )
        {
            for( i=0; i<3; i++ ) tenh[i] = xyz[i];
        }

        if( texu )
        {
            for( i=0; i<3; i++ ) texu[i] = geoid ? gllh[i] : 0.0;
        }
    }

    return sts;
}

/* Routines to check whether a point is in range */

int en_coords_in_range( coordsys *cs, double e, double n )
{
    if( !cs->gotrange ) return 1;
    if( !is_projection(cs) ) return 1;
    if( e >= cs->emin && e <= cs->emax && n >= cs->nmin && n <= cs->nmax )
    {
        return 1;
    }
    return 0;
}


int ll_coords_in_range( coordsys *cs, double *lon, double *lat )
{
    double llh[3];
    int result;
    if( !cs->gotrange ) return 1;
    llh[CRD_LAT] = *lat;
    llh[CRD_LON] = *lon;
    result = check_crdsys_range( cs, llh );
    if( result ) *lon = llh[CRD_LON];
    return result;
}
