#include "snapconfig.h"
#include <stdio.h>
#include <optional>
#include <string>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_grid.h"

#include "geoid/griddata.h"
#include "util/iostring.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "util/pi.h"

/// The opaque payload behind a VELGRID ref_deformation's data member.
/// File-private to this translation unit - never exposed via a header, so
/// it has no external callers. grid/status are the only fields ever
/// mutated after construction - every real construction starts unopened
/// (grid=nullptr) and valid (status=OK); grid is lazily opened later by
/// rf_grid_open_file.
struct ref_deformation_grid
{
    ref_deformation_grid( std::string filename, double refepoch ) :
        filename( std::move(filename) ), grid( nullptr ), refepoch( refepoch ), status( OK )
    {}
    ref_deformation_grid( const ref_deformation_grid& ) = delete;

    const std::string filename; ///< Path to the velocity grid file
    grid_def *grid;              ///< Loaded grid, or nullptr until rf_grid_open_file lazily opens it
    const double refepoch;      ///< Reference epoch the grid's velocities are relative to
    int status;                   ///< OK, or the error code from the lazy file open
};


static ref_deformation_grid *rf_grid_create( const std::string &filename, double refepoch )
{
    return new ref_deformation_grid( filename, refepoch );
}

static int rf_grid_open_file( ref_deformation_grid *gd )
{
    if( gd->grid ) return OK;
    if( gd->status != OK ) return gd->status;
    grd_open_grid_file( gd->filename,2,&(gd->grid));
    if( ! gd->grid )
    {
        gd->status = FILE_OPEN_ERROR;
    }
    return gd->status;
}

static void rf_grid_delete( void *pgd )
{
    ref_deformation_grid *gd = (ref_deformation_grid *) pgd;
    if( ! gd ) return;
    if( gd->grid ) grd_delete_grid( gd->grid );
    delete gd;
}

static void *rf_grid_copy( void *pgd )
{
    ref_deformation_grid *gd = (ref_deformation_grid *) pgd;
    return rf_grid_create( gd->filename, gd->refepoch );
}

static int rf_grid_identical( void *pgd1, void *pgd2 )
{
    ref_deformation_grid *gd1 = (ref_deformation_grid *) pgd1;
    ref_deformation_grid *gd2 = (ref_deformation_grid *) pgd2;
    if( gd1->refepoch != gd2->refepoch ) return 0;
    return gd1->filename == gd2->filename ? 1 : 0;
}

static int rf_grid_describe(  ref_frame *rf, output_string_def *os )
{
    int i;
    int sts;
    ref_deformation *def = rf->def;
    ref_deformation_grid *gd = (ref_deformation_grid *)(def->data);
    write_output_string(os,"Gridded velocity deformation model\n");
    sts = rf_grid_open_file( gd );
    if( sts != OK ) return sts;
    for( i = 1; i <= 3; i++ )
    {
        const std::optional<std::string> &text = gd->grid->title(i);
        if( text && !text->empty() )
        {
            write_output_string(os,"    ");
            write_output_string(os,*text);
            write_output_string(os,"\n");
        }
    }
    return OK;
}

static int rf_grid_calc( ref_frame *rf, double lon, double lat, double epoch, double denu[3])
{
    int sts;
    ref_deformation *def = rf->def;
    ref_deformation_grid *gd = (ref_deformation_grid *)(def->data);
    denu[0] = denu[1] = denu[2] = 0.0;
    sts = rf_grid_open_file( gd );
    if( sts != OK ) return sts;
    if( epoch == 0 ) return OK;
    epoch -= gd->refepoch;
    if( epoch == 0 ) return OK;
    sts = grd_calc_linear( gd->grid, lon*RTOD, lat*RTOD, denu );
    denu[0] *= epoch;
    denu[1] *= epoch;
    denu[2] *= epoch;
    return sts;
}

static int rf_grid_apply( ref_frame *rf,  double llh[3], double epochfrom, double epochto )
{
    double denu[3];
    int sts;
    int i;
    ref_deformation *def = rf->def;
    ref_deformation_grid *gd = (ref_deformation_grid *)(def->data);
    if( epochfrom == 0 ) epochfrom = gd->refepoch;
    if( epochto == 0 ) epochto = gd->refepoch;
    if( epochfrom == epochto ) return OK;
    sts = rf_grid_open_file( gd );
    if( sts != OK ) return sts;
    denu[0] = denu[1] = denu[2] = 0.0;
    sts = grd_calc_linear( gd->grid, llh[CRD_LON]*RTOD, llh[CRD_LAT]*RTOD, denu );
    if( sts != OK ) return sts;
    for( i = 0; i < 3; i++ ) denu[i] *= (epochto-epochfrom);
    return rf_apply_enu_deformation_to_llh( rf, llh, denu );
}

ref_deformation *rfdef_parse_griddef( input_string_def &is )
{
    double refepoch;
    std::optional<std::string> gridfile;
    char filename[MAX_FILENAME_LEN];
    int sts;

    sts = next_string_field( is.scanner, filename, MAX_FILENAME_LEN );
    if( sts != OK )
    {
        report_string_error( is, sts, "Missing filename for VELGRID deformation");
        return nullptr;
    }

    sts = double_from_string( is.scanner, &refepoch );
    if( sts != OK )
    {
        report_string_error( is, sts, "Missing reference epoch for VELGRID deformation");
        return nullptr;
    }

    gridfile = find_relative_file( is.sourcename, filename, ".grd" );
    if( ! gridfile )
    {
        std::string errmess = "Cannot open VELGRID deformation grid file " + std::string(filename);
        report_string_error(is, FILE_OPEN_ERROR, errmess );
        return nullptr;
    }

    return new ref_deformation( "VELGRID", rf_grid_create( *gridfile, refepoch ),
                                 rf_grid_delete, rf_grid_copy, rf_grid_identical,
                                 rf_grid_describe, rf_grid_calc, rf_grid_apply );
}
