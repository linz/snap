#include "snapconfig.h"
/* NOTE: Haven't done enything with units here yet!! */

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
#include "util/errdef.h"
#include "util/pi.h"

static char rcsid[]="$Id: crdsysc2.c,v 1.2 2004/04/22 02:34:21 ccrook Exp $";

/* Conversion of coordinates from one coordinate system to another     */
/* Converts coordinates, deflections, and undulations.  Input coords   */
/* are either projection northing, easting, or lat and long in radians */
/* Input heights are ellipsoidal.  Input deflections are in radians    */
/* Output coordinates may be written to the same vector as input, i.e. */
/* tneh == fneh is valid.  Input and output deflections/undulations    */
/* may be NULL.  Input are treated as 0,0,0 - output are ignored.      */


int define_coord_conversion( coord_conversion *conv,
                             coordsys *from, coordsys *to )
{
    return define_coord_conversion_epoch( conv, from, to, 0.0 );
}


int define_coord_conversion_epoch( coord_conversion *conv,
                                   coordsys *from, coordsys *to, double convepoch )
{
    ref_deformation *fromdef;
    ref_deformation *todef;
    char conv_el;
    conv->from = from;
    conv->to = to;
    conv->from_def = 0;
    conv->to_def = 0;
    conv->epochfrom = 0;
    conv->epochto = 0;
    conv->epochconv = convepoch;

    conv->valid = strcmp(from->rf->refcode,to->rf->refcode) == 0;
    conv->conv_rf = !identical_ref_frame_axes( from->rf, to->rf );
    conv_el = conv->conv_rf;
    if( !conv_el )
    {
        conv_el = !identical_ellipsoids( from->rf->el, to->rf->el );
    }

    if( is_projection(from) )
    {
        conv->from_prj = 1;
        conv->from_el = 1;
    }
    else if( is_geodetic(from) )
    {
        conv->from_prj = 0;
        conv->from_el = 1;
    }
    else
    {
        conv->from_prj = 0;
        conv->from_el = 0;
    }

    if( is_projection(to) )
    {
        conv->to_prj = 1;
        conv->to_el = 1;
    }
    else if( is_geodetic(to) )
    {
        conv->to_prj = 0;
        conv->to_el = 1;
    }
    else
    {
        conv->to_prj = 0;
        conv->to_el = 0;
    }

    if( !conv_el && conv->to_el && conv->from_el )
    {
        conv->to_el = 0;
        conv->from_el = 0;
    }

    conv->from_geoc = is_geocentric(from);
    conv->to_geoc = is_geocentric(to);

    fromdef = from->rf->def;
    todef = to->rf->def;
    if( ! fromdef && ! todef ) return conv->valid ? OK : INVALID_DATA;
    /* Check for compatible base epoch */
    if( convepoch == 0 && fromdef && todef && fromdef->conv_epoch != todef->conv_epoch )
    {
        conv->valid = 0;
        return INVALID_DATA;
    }

    /* Setup deformation conversions */
    if( fromdef )
    {
        if( convepoch == 0 ) conv->epochconv = fromdef->conv_epoch;
        conv->epochfrom = from->epoch;
        conv->from_def = conv->epochfrom == conv->epochconv ? 0 : 1;
    }
    /* If have deformation, but not changing reference frame, then apply
       deformation in one hit rather than to and from base epoch */
    if( ! conv->conv_rf )
    {
        conv->epochconv = to->epoch;
        conv->from_def = conv->epochfrom==conv->epochconv ? 0 : 1;
    }
    else if( has_deformation_model(to) )
    {
        conv->epochto = to->epoch;
        if( convepoch == 0 ) conv->epochconv = todef->conv_epoch;
        conv->to_def = conv->epochto == conv->epochconv ? 0 : 1;
    }

    /* Ensure we have ellipsoidal coordinates available as required by deformation */

    if( conv->from_def && conv->from_geoc  ) { conv->from_el = 2; }
    if( conv->to_def && conv->to_geoc  ) { conv->to_el = 2; }
    return OK;
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
    char from_el;
    char to_el;
    int sts, failed_sts;

    sts = OK;
    failed_sts = OK;

    /* Do the reference frames have a consistent frame */

    if( !conv->valid ) return INVALID_DATA;

    from = conv->from;
    to = conv->to;
    from_el = conv->from_el;
    to_el = conv->to_el;

    geoid = texu != 0;
    if( geoid && conv->from_geoc ) { from_el = 2; }
    if( geoid && conv->to_geoc ) { to_el = 2; }

    for( i = 0; i<3; i++ )
    {
        xyz[i] = fenh[i];
        gllh[i] = fexu ? fexu[i] : 0.0;
    }

    /* Convert from projection to lat long height */

    if( conv->from_prj ) proj_to_geog( from->prj,
                                           xyz[CRD_EAST], xyz[CRD_NORTH], xyz+CRD_LON, xyz+CRD_LAT );

    else if( from_el == 2 )
    {
        xyz_to_llh( from->rf->el, xyz, xyz );
    }

    /* Now convert to xyz coordinates -> standard ref frame
    -> new ref frame -> new ellipsoidal coords */

    if( conv->from_def )
    {
        apply_ref_deformation_llh( conv->from->rf, xyz, conv->epochfrom, conv->epochconv );
    }

    /* Convert the undulation to orthometric height, and convert the
     deflections to astronomical lats, longs */

    if( geoid )
    {
        gllh[CRD_LAT] = xyz[CRD_LAT] + gllh[CRD_LAT];
        gllh[CRD_LON] = xyz[CRD_LON] + gllh[CRD_LON]/cos(xyz[CRD_LAT]);
        gllh[CRD_HGT] = xyz[CRD_HGT] - gllh[CRD_HGT];
    }

    if( ! (from_el && to_el) || conv->conv_rf )
    {
        if( from_el ) llh_to_xyz( from->rf->el, xyz, xyz, NULL, NULL );

        if( conv->conv_rf )
        {
            sts = xyz_to_std( from->rf, xyz );
            if( sts != OK ) failed_sts = sts;
            sts = std_to_xyz( to->rf, xyz );
            if( sts != OK ) failed_sts = sts;
        }

        if( to_el ) xyz_to_llh( to->rf->el, xyz, xyz );
    }

    /* Recompute the deflections etc */

    if( geoid )
    {
        gllh[CRD_LAT] = gllh[CRD_LAT] - xyz[CRD_LAT];
        gllh[CRD_LON] = (gllh[CRD_LON] - xyz[CRD_LON]) * cos(xyz[CRD_LAT]);
        gllh[CRD_HGT] = xyz[CRD_HGT] - gllh[CRD_HGT];
    }

    /* Apply the target system deformation model */

    if( conv->to_def )
    {
        apply_ref_deformation_llh( conv->to->rf, xyz, conv->epochconv, conv->epochto );
    }

    /* Check that the points lie within a valid range for the coordinate
     system */

    if( to->gotrange && to->rf->el )
    {
        in_range = check_crdsys_range( to, xyz );
    }
    else
    {
        in_range = 1;
    }

    /* Convert back to a projection/geocentric if required */

    if( in_range && conv->to_prj )
    {
        geog_to_proj( to->prj,
                      xyz[CRD_LON], xyz[CRD_LAT], xyz+CRD_EAST, xyz+CRD_NORTH );
    }
    else if( to_el == 2 )
    {
        llh_to_xyz( to->rf->el, xyz, xyz, 0, 0 );
    }

    /* And copy back into the output vectors */

    if( tenh )
    {
        for( i=0; i<3; i++ ) tenh[i] = xyz[i];
    }

    if( texu )
    {
        for( i=0; i<3; i++ ) texu[i] = geoid ? gllh[i] : 0.0;
    }

    return failed_sts != OK ? failed_sts :
           in_range ? OK : INCONSISTENT_DATA;
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
