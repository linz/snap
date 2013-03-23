#include "snapconfig.h"
#include <stdio.h>
#include <string.h>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_grid.h"

#include "geoid/griddata.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/iostring.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "util/pi.h"

typedef struct
{
    char *filename;
    grid_def *grid;
    double refepoch;
    int status;
} ref_deformation_grid;


static ref_deformation_grid *rf_grid_create( char *filename, double refepoch )
{
    ref_deformation_grid *gd;
    gd = (ref_deformation_grid *) check_malloc( sizeof(ref_deformation_grid) );
    gd->filename = copy_string( filename );
    gd->grid = NULL;
    gd->refepoch = refepoch;
    gd->status = OK;
    return gd;
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
    if( gd->filename ) check_free( gd->filename );
    gd->filename = NULL;
    if( gd->grid ) grd_delete_grid( gd->grid );
    gd->grid = NULL;
    check_free( gd );
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
    return strcmp(gd1->filename, gd2->filename) == 0 ? 1 : 0;
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
        const char *text = grd_title(gd->grid,i);
        if( text && text[0] )
        {
            write_output_string(os,"    ");
            write_output_string(os,text);
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

int rfdef_parse_griddef( ref_deformation *def, input_string_def *is )
{
    double refepoch;
    char *gridfile;
    char filename[MAX_FILENAME_LEN];
    int sts;

    sts = next_string_field( is, filename, MAX_FILENAME_LEN );
    if( sts != OK )
    {
        report_string_error( is, sts, "Missing filename for VELGRID deformation");
        return sts;
    }

    sts = double_from_string( is, &refepoch );
    if( sts != OK )
    {
        report_string_error( is, sts, "Missing reference epoch for VELGRID deformation");
        return sts;
    }

    gridfile = find_file_from_base( is->sourcename, filename, ".grd" );
    if( ! gridfile )
    {
        char errmess[80+MAX_FILENAME_LEN];
        sts = FILE_OPEN_ERROR;
        sprintf(errmess,"Cannot open VELGRID deformation grid file %s",filename);
        report_string_error(is, sts, errmess );
        return sts;
    }

    def->delete_func = rf_grid_delete;
    def->copy_func = rf_grid_copy;
    def->describe_func = rf_grid_describe;
    def->data = rf_grid_create( gridfile, refepoch );
    def->calc_denu = rf_grid_calc;
    def->apply_llh = rf_grid_apply;
    return sts;
}
