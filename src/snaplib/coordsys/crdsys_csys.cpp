#include "snapconfig.h"
/* crdsyscs.c:  Coordinate system management for coordinate system routines */

/*
   $Log: crdsysc0.c,v $
   Revision 1.1  1995/12/22 16:25:13  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "coordsys/coordsys.h"
#include "util/errdef.h"
#include "util/pi.h"
#include <boost/algorithm/string/case_conv.hpp>

static const std::string metre_units = "m";
static const std::string radian_units = "rad";

coordsys::coordsys( const std::string &code_, const std::string &name_, int type,
                     ref_frame *rf_, projection *prj_,
                     std::optional<std::string> source_ ) :
    code( boost::algorithm::to_upper_copy(code_) ),
    name( name_ ),
    source( std::move(source_) ),
    rf( rf_ ),
    prj( prj_ ),
    hrs( nullptr ),
    crdtype( (char)( (type != CSTP_CARTESIAN && type != CSTP_PROJECTION) ? CSTP_GEODETIC : type ) ),
    gotrange( 0 ),
    ownsrf( 1 ),
    setrf( 0 ),
    hunits( crdtype == CSTP_GEODETIC ? radian_units : metre_units ),
    hmult( 1.0 ),
    vunits( metre_units ),
    vmult( 1.0 )
{
    if( prj && rf->el ) set_projection_ellipsoid( prj, rf->el );
}

coordsys::~coordsys()
{
    if( ownsrf ) delete rf;
    delete_projection( prj );
    delete hrs;
}

coordsys *copy_coordsys( coordsys *cs )
{
    if( !cs ) return nullptr;
    coordsys *copy = new coordsys( cs->code, cs->name, cs->crdtype,
                                    copy_ref_frame(cs->rf), copy_projection( cs->prj ),
                                    cs->source );
    copy->hrs = copy_vdatum( cs->hrs );
    if( copy->prj && copy->rf->el )
        set_projection_ellipsoid( copy->prj, copy->rf->el );
    if( cs->gotrange )
    {
        if( cs->crdtype == CSTP_PROJECTION )
        {
            define_coordsys_range( copy, cs->emin, cs->nmin,
                                   cs->emax, cs->nmax );
        }
        else
        {
            define_coordsys_range( copy, cs->lnmin, cs->ltmin,
                                   cs->lnmax, cs->ltmax );
        }
    }
    define_coordsys_units( copy, cs->hunits, cs->hmult, cs->vunits, cs->vmult );
    copy->setrf=cs->setrf;
    return copy;
}

coordsys *related_coordsys( coordsys *cs, int type )
{
    coordsys *copy;
    if( type == CSTP_PROJECTION && cs->crdtype != CSTP_PROJECTION ) return NULL;
    copy = copy_coordsys( cs );
    if( !copy ) return copy;
    if( type != CSTP_CARTESIAN && type != CSTP_PROJECTION ) type = CSTP_GEODETIC;
    copy->crdtype = (char) type;
    if( type == CSTP_GEODETIC )
    {
        define_coordsys_units( copy, radian_units, 1.0, metre_units, 1.0 );
    }
    else
    {
        define_coordsys_units( copy, metre_units, 1.0, metre_units, 1.0 );
    }
    copy->gotrange = 0;
    return copy;
}

bool coordsys_vdatum_compatible( coordsys *cs, vdatum *hrs )
{
    if( is_geocentric( cs ) ) return false;
    return identical_datum( vdatum_ref_frame(hrs), cs->rf );
}

vdatum *coordsys_vdatum( coordsys *cs )
{
    return cs->hrs;
}

int set_coordsys_ref_frame( coordsys *cs, ref_frame *rf )
{
    int sts=OK;
    if( cs->ownsrf ) delete cs->rf;
    cs->ownsrf=1;
    cs->setrf=1;
    cs->rf=rf;
    return sts;
}

int set_coordsys_vdatum( coordsys *cs, vdatum *hrs )
{
    int sts=OK;
    if( cs->hrs ) { delete cs->hrs; cs->hrs=nullptr; }
    if( ! hrs ) return sts;

    /* Check that vertical datum datum matches coordinate datum,
     * If not then just delete it. */

    if( coordsys_vdatum_compatible( cs, hrs ) )
    {
        cs->hrs=hrs;
    }
    else
    {
        char errmsg[100];
        sprintf( errmsg, "Vertical datum %.20s not compatible with coordinate system %.20s",
                hrs->code.c_str(),cs->code.c_str());
        handle_error( INVALID_DATA, errmsg, nullptr );
        delete hrs;
        sts=INVALID_DATA;
    }
    return sts;
}

void set_coordsys_geoid( coordsys *cs, const char *geoidfile )
{
    set_coordsys_vdatum( cs, geoid_vdatum( geoidfile, cs->rf ) );
}

bool coordsys_heights_orthometric( coordsys *cs )
{
    return cs->hrs ? true : false;
}

void define_deformation_model_epoch( coordsys *cs, double epoch )
{
    cs->rf->defepoch = epoch;
}

void define_coordsys_units( coordsys *cs,
                            const std::string &hunits, const double hmult,
                            const std::string &vunits, const double vmult )
{
    cs->hunits = hunits;
    cs->vunits = vunits;
    cs->hmult = hmult;
    cs->vmult = vmult;
}

/* Code defining and managing the definition of a valid range for
	coordinate systems */

void define_coordsys_range( coordsys *cs, double emin, double nmin, double emax, double nmax )
{
    cs->gotrange = 1;
    cs->emin = emin;
    cs->nmin = nmin;
    cs->emax = emax;
    cs->nmax = nmax;

    /* If it is not a projection coordinate system, then copy the
    	limits to the latitude and longitude limits.  Otherwise
    	compute the lat and long equivalents */

    if( cs->crdtype != CSTP_PROJECTION )
    {
        cs->lnmin = emin;
        cs->lnmax = emax;
        cs->ltmin = nmin;
        cs->ltmax = nmax;
    }

    else
    {
        proj_to_geog( cs->prj, emin, nmin, &cs->lnmin, &cs->ltmin );
        proj_to_geog( cs->prj, emax, nmax, &cs->lnmax, &cs->ltmax );
        while( cs->lnmax > cs->lnmin ) cs->lnmax -= TWOPI;
        while( cs->lnmax < cs->lnmin ) cs->lnmax += TWOPI;
    }
}


int check_coordsys_range( coordsys *cs, double xyz[3] )
{
    int sts;
    if( !cs->gotrange ) return OK;
    sts = OK;
    if( cs->crdtype == CSTP_GEODETIC )
    {
        double lon = xyz[CRD_LON];
        if( lon <= cs->lnmin || lon >= cs->lnmax )
        {
            sts = INCONSISTENT_DATA;
            while( lon < cs->lnmin ) lon += TWOPI;
            while( lon > cs->lnmax ) lon -= TWOPI;
            if( lon < cs->lnmin ) sts = INVALID_DATA;
        }
        if( xyz[CRD_LAT] < cs->ltmin || xyz[CRD_LAT] > cs->ltmax ) sts = INVALID_DATA;
        if( sts == INCONSISTENT_DATA ) xyz[CRD_LON] = lon;
    }
    else if( cs->crdtype == CSTP_PROJECTION )
    {
        if( xyz[CRD_EAST] < cs->emin || xyz[CRD_EAST] > cs->emax ||
                xyz[CRD_NORTH] < cs->nmin || xyz[CRD_NORTH] > cs->nmax ) sts = INVALID_DATA;
    }
    return sts;
}


int is_projection( coordsys *cs )
{
    return cs->crdtype == CSTP_PROJECTION;
}

int is_geodetic( coordsys *cs )
{
    return cs->crdtype == CSTP_GEODETIC;
}

int is_geocentric( coordsys *cs )
{
    return cs->crdtype == CSTP_CARTESIAN;
}

int has_deformation_model( coordsys *cs )
{
    return cs->rf->def ? 1 : 0;
}

double deformation_model_epoch( coordsys *cs )
{
    return cs->rf->defepoch;
}

int identical_coordinate_systems( coordsys *cs1, coordsys *cs2 )
{
    if( cs1->crdtype != cs2->crdtype ) return 0;
    if( cs1->crdtype == CSTP_PROJECTION &&
            !identical_projections( cs1->prj, cs2->prj ) ) return 0;
    if( !identical_ellipsoids( cs1->rf->el, cs2->rf->el ) ) return 0;
    if( !identical_ref_frame_axes( cs1->rf, cs2->rf ) ) return 0;
    return 1;
}

int related_coordinate_systems( coordsys *cs1, coordsys *cs2 )
{
    if( cs1->crdtype == CSTP_PROJECTION && cs2->crdtype == CSTP_PROJECTION &&
            !identical_projections( cs1->prj, cs2->prj )) return 0;
    if( !identical_ellipsoids( cs1->rf->el, cs2->rf->el ) ) return 0;
    if( !identical_ref_frame_axes( cs1->rf, cs2->rf ) ) return 0;
    return 1;
}

int coordsys_geoid_exu( coordsys *cs, double llh[3], double *height, double *exu )
{
    double llh1[3];
    if( ! cs->hrs )
    {
        if( height ) *height=0.0;
        if( exu ){ exu[0]=exu[1]=exu[2]=0.0; }
        return OK;
    }
    if( height || exu )
    {
        if( cs->crdtype == CSTP_PROJECTION )
        {
            proj_to_geog( cs->prj, llh[CRD_EAST], llh[CRD_NORTH], &(llh1[CRD_LON]), &(llh1[CRD_LAT]));
            llh1[CRD_HGT]=llh[CRD_HGT];
        }
        else if( cs->crdtype == CSTP_CARTESIAN )
        {
            xyz_to_llh( cs->rf->el, llh, llh1 );
        }
        else
        {
            llh1[CRD_LON]=llh[CRD_LON];
            llh1[CRD_LAT]=llh[CRD_LAT];
            llh1[CRD_HGT]=llh[CRD_HGT];
        }
    }
    return calc_vdatum_offset( cs->hrs, llh1, height, exu );
}
