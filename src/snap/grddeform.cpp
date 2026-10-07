
#include <stdio.h>
#include <math.h>
#include <string>

#include "snap/snapglob.h"
#include "snap/deform.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/chkalloc.h"
#include "geoid/griddata.h"
#include "coordsys/coordsys.h"
#include "snap/stnadj.h"

#include "grddeform.h"

#ifndef M_PI
#include "util/pi.h"
#define M_PI PI
#endif

static double epoch;
static grid_def *velgrid;
static std::string model;
static std::string modelfile;
static int veldimension;


struct velocity
{
    double dxyz[3];
};

static velocity *stn_velocities = nullptr;

static std::string desc1;
static std::string desc2;
static std::string desc3;

/* Called when the configuration file includes a deformation command - the
   command is passed to define_deformation as the string model */

// #pragma warning (disable : 4100)

static int init_grid_deformation(  const std::string &pmodel, double pepoch )
{
    epoch = pepoch;
    model = pmodel;
    auto grdfile = find_coordsys_data_file( model, ".grd" );
    if( !grdfile ) return INVALID_DATA;
    modelfile = *grdfile;
    if(  grd_open_grid_file( modelfile, 2, &velgrid ) == OK )
    {
        veldimension = 2;
    }
    else if ( grd_open_grid_file( modelfile, 3, &velgrid ) == OK )
    {
        veldimension = 3;
    }
    else
    {
        return INVALID_DATA;
    }
    desc1 = velgrid->title( 1 ).value_or( "" );
    desc2 = velgrid->title( 2 ).value_or( "" );
    desc3 = velgrid->title( 3 ).value_or( "" );
    return OK;
}

/* Called before the first adjustment iteration to perform any necessary
   preparation  - for a velocity model we use this to calculate and cache
   the velocities of all stations.. */

static int init_griddef( void * )
{
    coord_conversion tovcs;
    char buf[128];

    if( ! velgrid ) return INVALID_DATA;
    const std::string vcsdef = velgrid->crdsys.value_or( "" );
    coordsys * const vcs = load_coordsys( vcsdef );
    if( !vcs )
    {
        sprintf( buf,"Cannot load velocity model coordinate system %-20s",vcsdef.c_str());
        handle_error(WARNING_ERROR,buf,NO_MESSAGE);
        return INVALID_DATA;
    }
    if( define_coord_conversion( &tovcs, net->geosys, vcs ) != OK )
    {
        sprintf(buf,"Cannot convert station coordinates to coordinate system %-20s of velocity model",
                vcs->code.c_str());
        handle_error(WARNING_ERROR,buf,NO_MESSAGE);
        return INVALID_DATA;
    }
    const double factor = vcs->crdtype == CSTP_GEODETIC ? 180/M_PI  : 1.0;

    /* Allocate space for a set of deformation parameters.. */

    const int nstns = number_of_stations( net );
    stn_velocities = static_cast<velocity *>( check_malloc( sizeof(velocity) * (nstns+1) ) );

    /* For each station calculate the velocity */

    for( int istn = 1; istn <= nstns; istn++ )
    {
        double xyz[3];
        const station * const st = station_ptr( net, istn );
        xyz[CRD_LAT] = st->ELat;
        xyz[CRD_LON] = st->ELon;
        xyz[CRD_HGT] = st->OHgt + st->GUnd;
        if( convert_coords( &tovcs, xyz, nullptr, xyz, nullptr ) != OK )
        {
            sprintf(buf,"Cannot convert coordinates of %-20s to velocity coordinate system %-20s",
                    st->Code.c_str(), vcs->code.c_str());
            handle_error(WARNING_ERROR,buf,NO_MESSAGE);
            return INVALID_DATA;
        }
        grd_calc_linear( velgrid, xyz[CRD_EAST]*factor, xyz[CRD_NORTH]*factor,
                         stn_velocities[istn].dxyz );
    }

    /* Release resource held by grid now that we have all values from it! */
    delete vcs;
    grd_delete_grid( velgrid );
    velgrid = 0;

    return OK;
}

/* Called for each observation to determine the east, north, and vertical
   offset that the model predicts for a specific time */

static int calc_griddef( void *, station *st, double date, double denu[3] )
{
    int stnid;
    double year;
    stnid = find_station( net, st->Code );
    year = date_as_year(date)-epoch;

    denu[0] = stn_velocities[stnid].dxyz[0] * year;
    denu[1] = stn_velocities[stnid].dxyz[1] * year;
    denu[2] = veldimension == 3 ? stn_velocities[stnid].dxyz[2] * year : 0.0;
    return OK;
}

/* Describe the deformation model in an output file */

static int print_griddef_model( void *, FILE *out, const char *prefix )
{
    fprintf(out,"%sModel type: velocity\n",prefix );
    fprintf(out,"%sModel name: %s\n", prefix,model.c_str() );
    if( !desc1.empty() ) {fprintf(out,"%s%s\n",prefix,desc1.c_str());}
    if( !desc2.empty() ) {fprintf(out,"%s%s\n",prefix,desc2.c_str());}
    if( !desc3.empty() ) {fprintf(out,"%s%s\n",prefix,desc3.c_str());}
    fprintf(out,"%sReference epoch: %.1lf\n",prefix,epoch);
    return OK;
}

static int delete_griddef( void * )
{
    return OK;
}

int create_grid_deformation( deformation_model **model, const std::string &pmodel, double pepoch )
{
    int sts;
    sts = init_grid_deformation( pmodel, pepoch );
    if( sts == OK )
    {
        (*model) = create_deformation_model(
                       NULL,
                       init_griddef,
                       calc_griddef,
                       print_griddef_model,
                       delete_griddef);
    }
    return sts;
}

