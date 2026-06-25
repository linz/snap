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
#include <string.h>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rffunc_grid.h"
#include "geoid/griddata.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "util/pi.h"

enum {NULL_GRID, SNAP2D_GRID};

typedef struct
{
    char *filename;
    grid_def *grid;
    int type;
    int status;
} rf_grid_def;


static rf_grid_def *rf_grid_create( const char *filename, int type )
{
    rf_grid_def *gd;
    if( type != SNAP2D_GRID ) return NULL;
    gd = (rf_grid_def *) check_malloc( sizeof(rf_grid_def) );
    gd->filename = copy_string( filename );
    gd->grid = NULL;
    gd->status = OK;
    gd->type = type;
    return gd;
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
    if( gd->filename ) check_free( gd->filename );
    gd->filename = NULL;
    if( gd->grid ) grd_delete_grid( gd->grid );
    gd->grid = NULL;
    gd->type =  NULL_GRID;
    check_free( gd );
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
    return strcmp(gd1->filename, gd2->filename) == 0 ? 1 : 0;
}

static int rf_grid_describe( ref_frame *rf, output_string_def *os )
{
    ref_frame_func *rff = rf->func;
    rf_grid_def *gd = (rf_grid_def *)(rff->data);
    write_output_string(os,"Transformation uses ");
    if( rff->description )
    {
        write_output_string(os,rff->description);
    }
    else
    {
        write_output_string(os,"transformation grid from file ");
        write_output_string(os,gd->filename);
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

ref_frame_func *create_rf_grid_func( const char *type, const char *filename, char *description )
{
    int gridtype = NULL_GRID;
    void *pgd;
    ref_frame_func *rff;
    if( _stricmp(type,"SNAP2D") == 0 ) gridtype = SNAP2D_GRID;
    pgd = rf_grid_create( filename, gridtype );
    if( ! pgd ) return NULL;
    rff = (ref_frame_func *) check_malloc( sizeof(ref_frame_func) );
    rff->description = NULL;
    if( description && strlen(description) > 0 )
        rff->description = copy_string( description );
    rff->data = pgd;
    rff->delete_func = rf_grid_delete;
    rff->describe_func = rf_grid_describe;
    rff->copy_func = rf_grid_copy;
    rff->identical = rf_grid_identical;
    rff->xyz_to_std_func = rf_grid_xyz_to_std;
    rff->std_to_xyz_func = rf_grid_std_to_xyz;
    return rff;
}

