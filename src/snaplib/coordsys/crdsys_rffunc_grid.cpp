#include "snapconfig.h"

/* crdsysr5.c:  Reference frame function definition for 2d lat/lon
   conversion - still uses 7 param transformation for height conversion.

   Uses the griddata functions for handling the grid based conversion.

   $Log: crdsysr5.c,v $
   Revision 1.3  2004/04/22 02:34:21  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  2004/02/02 03:25:43  ccrook
   Fixed up copying of reference frame functions in copy_coordsys function

   Revision 1.1  2003/11/28 01:59:26  ccrook
   Updated to be able to use grid transformation for datum changes (ie to
   support official NZGD49-NZGD2000 conversion)


*/

#include <stdio.h>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rffunc_grid.h"
#include "geoid/griddata.h"
#include "util/errdef.h"
#include "util/pi.h"
#include <boost/algorithm/string/predicate.hpp>

enum {NULL_GRID, SNAP2D_GRID};

/// The opaque payload behind a grid-based ref_frame_func's data member.
/// File-private to this translation unit - never exposed via a header, so
/// it has no external callers. grid/status are never caller-supplied -
/// every real construction starts unopened (grid=nullptr) and valid
/// (status=OK); grid is lazily opened later by rf_grid_open_file.
struct rf_grid_def
{
    rf_grid_def( std::string filename, int type ) :
        filename( std::move(filename) ), grid( nullptr ), type( type ), status( OK )
    {}
    rf_grid_def( const rf_grid_def& ) = delete;

    std::string filename; ///< Path to the transformation grid file
    grid_def *grid;       ///< Loaded grid, or nullptr until rf_grid_open_file lazily opens it
    int type;              ///< NULL_GRID or SNAP2D_GRID
    int status;             ///< OK, or the error code from the lazy file open
};


static rf_grid_def *rf_grid_create( const std::string &filename, int type )
{
    if( type != SNAP2D_GRID ) return nullptr;
    return new rf_grid_def( filename, type );
}

static void rf_grid_open_file( rf_grid_def *gd )
{
    grid_def *grid = NULL;
    if( gd->status != OK ) return;
    grd_open_grid_file(gd->filename,2,&grid);
    if( grid )
    {
        gd->grid = grid;
    }
    else
    {
        gd->status = FILE_OPEN_ERROR;
    }
}

static void rf_grid_delete( void *pgd )
{
    rf_grid_def *gd = (rf_grid_def *) pgd;
    if( ! gd ) return;
    if( gd->grid ) grd_delete_grid( gd->grid );
    delete gd;
}

static void *rf_grid_copy( void *pgd )
{
    rf_grid_def *gd = (rf_grid_def *) pgd;
    return rf_grid_create( gd->filename, gd->type );
}

static int rf_grid_identical( void *pgd1, void *pgd2 )
{
    rf_grid_def *gd1 = (rf_grid_def *) pgd1;
    rf_grid_def *gd2 = (rf_grid_def *) pgd2;
    return gd1->filename == gd2->filename ? 1 : 0;
}

static int rf_grid_describe( ref_frame *rf, output_string_def *os )
{
    ref_frame_func *rff = rf->func;
    rf_grid_def *gd = (rf_grid_def *)(rff->data);
    write_output_string(os,"Transformation uses ");
    if( rff->description )
    {
        write_output_string(os,rff->description->c_str());
    }
    else
    {
        write_output_string(os,"transformation grid from file ");
        write_output_string(os,gd->filename.c_str());
    }
    write_output_string(os,"\n");
    return OK;
}

static int rf_grid_xyz_to_std( ref_frame *rf, double xyz[3], double date )
{
    rf_grid_def *gd;
    double llh[3];
    double dllh[3];
    int sts;
    ref_frame_func *rff;
    if( ! rf->func ) return INVALID_DATA;
    gd = (rf_grid_def *) rf->func->data;
    if( ! gd ) return INVALID_DATA;
    if( gd->status == OK && ! gd->grid ) rf_grid_open_file( gd );
    if( gd->status != OK ) return gd->status;
    xyz_to_llh( rf->el, xyz, llh );
    sts = grd_calc_linear( gd->grid, llh[CRD_LON]*RTOD, llh[CRD_LAT]*RTOD, dllh );
    if( sts == OK )
    {
        llh[CRD_LON] += dllh[CRD_LON]*DTOR;
        llh[CRD_LAT] += dllh[CRD_LAT]*DTOR;
    }

    llh_to_xyz( rf->el, llh, xyz, NULL, NULL );
    rff = rf->func;
    rf->func = NULL;
    xyz_to_std( rf, xyz, date );
    rf->func = rff;
    return sts;
}

static int rf_grid_std_to_xyz( ref_frame *rf, double xyz[3], double date )
{
    rf_grid_def *gd;
    double llh[3];
    double dllh[3];
    double lat, lon;
    int sts;
    ref_frame_func *rff;
    if( ! rf->func ) return INVALID_DATA;
    gd = (rf_grid_def *) rf->func->data;
    if( ! gd ) return INVALID_DATA;
    if( gd->status == OK && ! gd->grid ) rf_grid_open_file( gd );
    if( gd->status != OK ) return gd->status;
    rff = rf->func;
    rf->func = NULL;
    std_to_xyz( rf, xyz, date );
    rf->func = rff;
    xyz_to_llh( rf->el, xyz, llh );
    lon = llh[CRD_LON]*RTOD;
    lat = llh[CRD_LAT]*RTOD;
    sts = grd_calc_linear( gd->grid, lon, lat, dllh );
    if( sts != OK ) return sts;
    lon -= dllh[CRD_LON];
    lat -= dllh[CRD_LAT];
    sts = grd_calc_linear( gd->grid, lon, lat, dllh );
    if( sts != OK ) return sts;
    llh[CRD_LON] -= dllh[CRD_LON]*DTOR;
    llh[CRD_LAT] -= dllh[CRD_LAT]*DTOR;
    llh_to_xyz( rf->el, llh, xyz, NULL, NULL );
    return OK;
}

ref_frame_func *create_rf_grid_func( const std::string &type, const std::string &filename, const std::string &description )
{
    int gridtype = NULL_GRID;
    if( boost::algorithm::iequals(type,"SNAP2D") ) gridtype = SNAP2D_GRID;
    void *pgd = rf_grid_create( filename, gridtype );
    if( ! pgd ) return nullptr;
    std::optional<std::string> desc;
    if( ! description.empty() ) desc = description;
    return new ref_frame_func( "GRID", desc, pgd, rf_grid_delete, rf_grid_describe,
                                rf_grid_copy, rf_grid_identical, rf_grid_xyz_to_std, rf_grid_std_to_xyz );
}

