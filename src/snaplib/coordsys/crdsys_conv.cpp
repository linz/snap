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
#include <boost/algorithm/string.hpp>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_hrs_func.h"
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


coord_conversion::coord_conversion( coordsys *fromCoordsys, coordsys *toCoordsys, double convepoch, const bool ellipsoidal )
{
    from = fromCoordsys;
    to = toCoordsys;
    convepoch = _defaultEpoch( convepoch );
    epochconv = convepoch;

    int nfrom=0;
    int nto=0;
    bool changeepoch=false; /* Different deformation ref epoch */

    /* If not using the same the must have common base reference frame codes,
       and if there is no common rf then cannot do conversion */

    if( ! _findCommonReferenceFrame( nfrom, nto, changeepoch ) )
    {
        valid=0;
        errmsg="Conversion between coordinate systems "+from->code.substr(0,20)+" and "+
               to->code.substr(0,20)+" is not possible";
    }
    else if( nfrom+nto > CONVMAXRF  )
    {
        valid=0;
        errmsg="Conversion between coordinate systems "+from->code.substr(0,20)+" and "+
               to->code.substr(0,20)+" is too complex (> "+std::to_string(CONVMAXRF)+" steps)";
    }
    else
    {
        _defineReferenceFrameSteps( nfrom, nto, changeepoch, convepoch );
    }

    bool refFrameChanges = ncrf > 0;
    if( !refFrameChanges )
    {
        refFrameChanges = !identical_ellipsoids( from->rf->el, to->rf->el );
    }

    from_prj=is_projection(from);
    to_prj=is_projection(to);
    from_geoc = is_geocentric(from);
    to_geoc = is_geocentric(to);

    /* Handle vertical datums */

    if( ! ellipsoidal && (from->hrs || to->hrs) )
    {
        _defineVerticalDatumSteps( refFrameChanges );
    }
}

/* Can we define a conversion epoch from the coordinate system definitions */

double coord_conversion::_defaultEpoch( double convepoch ) const
{
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
    return convepoch;
}

bool coord_conversion::_findCommonReferenceFrame( int &nfrom, int &nto, bool &changeepoch ) const
{
    bool common_rf=false;
    std::optional<std::string> fromCode=from->rf->code;
    ref_frame *from_rf=from->rf;
    nfrom=0;
    nto=0;
    changeepoch=false;

    /* Track from each reference frame to find a common base */

    while( ! common_rf )
    {
        std::optional<std::string> toCode=to->rf->code;
        ref_frame *to_rf=to->rf;
        nto=0;
        while( ! common_rf )
        {
            if( boost::algorithm::iequals( *fromCode, *toCode ) )
            {
                common_rf=true;
                /* If have reached a common actual reference frame (rather than
                 * base code) then check we have the same deformation reference
                 * epoch */
                if( from_rf && to_rf )
                {
                    if( from_rf->defepoch != to_rf->defepoch )
                    {
                        changeepoch=true;
                        nfrom++;
                        nto++;
                    }
                }
                break;
            }
            if( ! to_rf ) break;
            toCode = to_rf->refcode;
            to_rf=to_rf->refrf;
            nto++;
            if( nfrom+nto > CONVMAXRF ) break;
            if( ! toCode ) break;
        }
        if( common_rf || ! from_rf ) break;
        fromCode = from_rf->refcode;
        from_rf=from_rf->refrf;
        nfrom++;
        if( nfrom > CONVMAXRF ) break;
        if( ! fromCode ) break;
    }
    return common_rf;
}

void coord_conversion::_defineReferenceFrameSteps( const int nfrom, const int nto, const bool changeepoch, const double convepoch )
{
    ref_frame *rframe=from->rf;
    ref_frame *nextrf;
    int i;
    bool epochNeeded=false;

    for( i=0; i<nfrom; i++, rframe=rframe->refrf )
    {
        crf[i].rf=rframe;
        crf[i].xyz_to_std=1;
        crf[i].def_only=0;
        crf[i].need_xyz=0;
    }
    rframe=to->rf;
    for( i=nto; i-- > 0; rframe=rframe->refrf )
    {
        crf[i+nfrom].rf=rframe;
        crf[i+nfrom].xyz_to_std=0;
        crf[i+nfrom].def_only=0;
        crf[i+nfrom].need_xyz=0;
    }
    /* If we are changing epoch, then the common reference frame is
     * included in the transformation, but don't need to apply xyz_to_std
     * and back (as it would be a null transformation)
     */
    if( changeepoch )
    {
        crf[nfrom-1].def_only=1;
        crf[nfrom].def_only=1;
    }
    ncrf=nfrom+nto;
    valid = 1;

    nextrf=to->rf;
    for( i=ncrf; i--;)
    {
        rframe=crf[i].rf;

        /* Check  whether changing ellipsoid, so must have xyz coords */
        if( ! identical_ellipsoids(rframe->el,nextrf->el) ) crf[i].need_xyz=1;
        nextrf=rframe;
        if( ! epochNeeded && ! crf[i].def_only )
        {
            if( rframe->def ||
                    rframe->dtxyz[0] != 0.0 ||
                    rframe->dtxyz[1] != 0.0 ||
                    rframe->dtxyz[2] != 0.0 ||
                    rframe->drxyz[0] != 0.0 ||
                    rframe->drxyz[1] != 0.0 ||
                    rframe->drxyz[2] != 0.0 ||
                    rframe->dscale != 0.0 )
                epochNeeded=true;
        }
    }
    if( ! identical_ellipsoids(from->rf->el,nextrf->el) ) need_xyz=1;

    if( epochNeeded && convepoch==0.0 )
    {
        valid=0;
        errmsg="Conversion between reference frames "+from->rf->code.substr(0,20)+" and "+
               to->rf->code.substr(0,20)+" requires a date";
    }
    needsepoch = epochNeeded;
}

void coord_conversion::_defineVerticalDatumSteps( const bool refFrameChanges )
{
    int nfromHrs=0;
    int ntoHrs=0;
    vdatum *fhrs;
    vdatum *thrs;
    for( fhrs=from->hrs; fhrs; fhrs=fhrs->basehrs ) nfromHrs++;
    for( thrs=to->hrs; thrs; thrs=thrs->basehrs ) ntoHrs++;

    /* If have vertical datum conversion on from and to, and identical
     * reference frames, then see if we can reduce the number of
     * steps */

    if(  nfromHrs && ntoHrs && ! refFrameChanges )
    {
        int nmin=nfromHrs > ntoHrs ? ntoHrs : nfromHrs;
        int nhrff=nfromHrs;
        int nhrft=ntoHrs;
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
        nfromHrs -= nmin;
        ntoHrs -= nmin;
    }

    if( nfromHrs+ntoHrs > CONVMAXRF  )
    {
        valid=0;
        errmsg="Conversion between vertical datums "+
               (from->hrs ? from->hrs->code.substr(0,20) : std::string("ellipsoid"))+" and "+
               (to->hrs ? to->hrs->code.substr(0,20) : std::string("ellipsoid"))+
               " is too complex (> "+std::to_string(CONVMAXRF)+" steps)";
    }
    else
    {
        int nlast=nfromHrs+ntoHrs-1;
        fhrs=from->hrs;
        thrs=to->hrs;
        for( int i=0; i<nfromHrs; i++ ){ hrf[i]=fhrs->func; fhrs=fhrs->basehrs; }
        for( int i=0; i<ntoHrs; i++ ){ hrf[nlast-i]=thrs->func; thrs=thrs->basehrs; }
        nhrf_from=nfromHrs;
        nhrf_to=ntoHrs;
    }
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
    conv->errmsg.clear();

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
            conv->errmsg="Cannot calculate "+conv->from->hrs->code+" height";
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
                            conv->errmsg="Cannot apply "+rf->code+" deformation model";
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
                        conv->errmsg="Cannot convert from "+rf->code+" to "+(rf->refcode ? *rf->refcode : std::string("base"));
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
                            conv->errmsg="Cannot convert from "+(rf->refcode ? *rf->refcode : std::string("base"))+" to "+rf->code;
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
                            conv->errmsg="Cannot apply "+rf->code+" deformation model";
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
            conv->errmsg="Cannot calculate "+conv->to->hrs->code+" height";
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
                conv->errmsg="Converted coordinates are outside range of "+to->code.substr(0,20)+" coordinate system";
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
