#include "snapconfig.h"
/* crdsyshrs.c:  Routines to manage vertical datums */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_hrs_func.h"
#include "geoid/geoid.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/geodetic.h"
#include "util/pi.h"

/*========================================================================================*/
/* Offset vertical datum function routine                                               */

static void describe_offset_vdatum_func( vdatum_func *, output_string_def * )
{
    return;
}

static int identical_offset_vdatum_func( void *data1, void *data2 )
{
    return (*(double *)data1) == (*(double *)data2);
}

static void *copy_offset_vdatum_func_data( void *data )
{
    return new double( *(double *)data );
}

static void delete_offset_vdatum_func_data( void *data )
{
    delete (double *) data;
}

static int calc_offset_vdatum_func( vdatum_func *hrf, double[3], double *height, double *exu )
{
    if( height) *height = -(*(double *)(hrf->data));
    if( exu )
    {
        exu[CRD_LON]=0.0;
        exu[CRD_LAT]=0.0;
        exu[CRD_HGT]=-(*(double *)(hrf->data));
    }
    return OK;
}

vdatum_func *create_offset_vdatum_func( double offset )
{
    char description[80];
    sprintf(description,"Offset of %.3lf metres", offset );
    return new vdatum_func( "OFFSET", description, new double(offset),
                             delete_offset_vdatum_func_data, describe_offset_vdatum_func,
                             copy_offset_vdatum_func_data, identical_offset_vdatum_func,
                             calc_offset_vdatum_func );
}

/*========================================================================================*/
/* Grid based vertical datum function routine                                           */

/// The opaque payload behind a GRID/GEOID vdatum_func's data member.
/// File-private to this translation unit - never exposed via a header, so
/// it has no external callers, and is only ever destroyed through
/// delete_grid_vdatum_func_data (matching rf_grid_def/LinzDefModel/etc. -
/// no destructor of its own, since that free function is the only real
/// destroy path). gd/rfcs/rfconv/irfconv/loadsts are the only fields ever
/// mutated after construction - every real construction starts unloaded
/// (gd=nullptr, loadsts=OK), and load_grid_vdatum_func populates the rest
/// lazily, on first real use. rfconv/irfconv share a single
/// new coord_conversion[2] allocation - irfconv is just rfconv+1, freed
/// together via delete[].
struct grid_vdatum_func_data
{
    grid_vdatum_func_data( std::string filename, int isoffset ) :
        filename( std::move(filename) ), gd( nullptr ), rfcs( nullptr ),
        rfconv( nullptr ), irfconv( nullptr ), loadsts( OK ), isoffset( isoffset )
    {}
    grid_vdatum_func_data( const grid_vdatum_func_data& ) = delete;

    const std::string filename; ///< Path to the geoid/grid file
    geoid_def *gd;                ///< Loaded geoid grid, or nullptr until load_grid_vdatum_func lazily opens it
    coordsys *rfcs;                ///< Reference-frame-only coordinate system, lazily built alongside gd
    coord_conversion *rfconv;        ///< Conversion to the geoid's own coordsys - see irfconv
    coord_conversion *irfconv;         ///< The inverse conversion of rfconv
    int loadsts;                         ///< OK, or the error code from the lazy load
    const int isoffset;                    ///< Offset is offset to height coord, so negative of offset to surface
};

static void delete_grid_vdatum_func_data( void *data )
{
    grid_vdatum_func_data *ghrfd=(grid_vdatum_func_data *) data;
    if( ! ghrfd ) return;
    if( ghrfd->gd ) delete_geoid_grid( ghrfd->gd );
    if( ghrfd->rfcs ) delete ghrfd->rfcs;
    delete[] ghrfd->rfconv;
    delete ghrfd;
}

static int load_grid_vdatum_func( vdatum_func *hrf, grid_vdatum_func_data *ghrfd )
{
    if( ghrfd->gd || ghrfd->loadsts != OK ) 
    {
        return ghrfd->loadsts;
    }
    if( ! hrf->hrs )
    {
        handle_error(INTERNAL_ERROR,"load_grid_vdatum_func called before hrs set",NO_MESSAGE);
        return INTERNAL_ERROR;
    }
    ref_frame *rf=vdatum_ref_frame( hrf->hrs );
    if( ! rf )
    {
        handle_error(INTERNAL_ERROR,"load_grid_vdatum_func called before hrs set",NO_MESSAGE);
        return INTERNAL_ERROR;
    }
    /* Load the geoid */
    ghrfd->gd=create_geoid_grid( ghrfd->filename );
    if( ! ghrfd->gd )
    {
        ghrfd->loadsts=INVALID_DATA;
        return INVALID_DATA;
    }
    /* Create a coordinate system using but not owning the reference frame */
    coordsys *gcs=get_geoid_coordsys( ghrfd->gd );
    if( ! identical_datum(gcs->rf,rf) )
    {
        ghrfd->rfconv=new coord_conversion[2];
        ghrfd->irfconv=ghrfd->rfconv+1;
        ghrfd->rfcs=new coordsys( rf->code, rf->name, CSTP_GEODETIC, rf, nullptr );
        ghrfd->rfcs->ownsrf=0;
        *ghrfd->rfconv=coord_conversion( ghrfd->rfcs, get_geoid_coordsys( ghrfd->gd ), DEFAULT_CRDSYS_EPOCH );
        ghrfd->loadsts=ghrfd->rfconv->valid ? OK : INVALID_DATA;
        if( ghrfd->loadsts == OK )
        {
            *ghrfd->irfconv=coord_conversion( get_geoid_coordsys( ghrfd->gd ), ghrfd->rfcs, DEFAULT_CRDSYS_EPOCH );
            ghrfd->loadsts=ghrfd->irfconv->valid ? OK : INVALID_DATA;
        }
    }
    return ghrfd->loadsts;
}

static void describe_grid_vdatum_func( vdatum_func *hrf, output_string_def *os )
{
    grid_vdatum_func_data *ghrfd=(grid_vdatum_func_data *) hrf->data;
    int sts=load_grid_vdatum_func( hrf, ghrfd );
    if( sts == OK )
    {
        for( int i=1; i < 4; i++ )
        {
            const std::string_view title = geoid_title( ghrfd->gd, i );
            if( !title.empty() )
            {
                write_output_string( os, title );
                write_output_string( os, "\n" );
            }
        }
    }
    return;
}

static int identical_grid_vdatum_func( void *data1, void *data2 )
{
    return ((grid_vdatum_func_data *)data1)->filename ==
           ((grid_vdatum_func_data *)data2)->filename;
}

static void *copy_grid_vdatum_func_data( void *data )
{
    grid_vdatum_func_data *ghrfd=(grid_vdatum_func_data *) data;
    return new grid_vdatum_func_data( ghrfd->filename, ghrfd->isoffset );
}

static int calc_grid_vdatum_func( vdatum_func *hrf, double llh[3], double *height, double *exu )
{
    grid_vdatum_func_data *ghrfd=(grid_vdatum_func_data *) hrf->data;
    int sts=load_grid_vdatum_func( hrf, ghrfd );
    if( ! height && ! exu ) return sts;
    if( sts != OK )
    {
        if( height ) *height=0.0;
        if( exu ){ exu[0]=exu[1]=exu[2]=0.0; }
        return sts;
    }
    geoid_def *gd=ghrfd->gd;
    if( ! ghrfd->rfcs )
    {
        if( exu )
        {
            sts=calculate_geoid_exu(gd,llh[CRD_LAT],llh[CRD_LON],exu);
            if( height ) *height=exu[CRD_HGT];
        }
        else
        {
            sts=calculate_geoid_undulation(gd,llh[CRD_LAT],llh[CRD_LON],height);
        }
    }
    else
    {
        double llh1[3];
        double exu1[3];
        sts=convert_coords( ghrfd->rfconv, llh, llh1, 0, 0 );
        if( sts == OK ) sts=calculate_geoid_exu(gd,llh1[CRD_LAT],llh1[CRD_LON],exu1);
        if( sts == OK ) sts=convert_coords( ghrfd->irfconv, llh1, llh1, exu1, exu1 );
        if( sts == OK )
        {
           if( height ) *height=exu1[CRD_HGT]; 
           if( exu ){ exu[CRD_LON]=exu1[CRD_LON]; exu[CRD_LAT]=exu1[CRD_LAT]; exu[CRD_HGT]=exu1[CRD_HGT]; }
        }
    }
    if( ghrfd->isoffset )
    {
        if( height ) { *height=-*height; }
        if( exu ) { exu[0]=-exu[0]; exu[1]=-exu[1]; exu[2]=-exu[2]; }
    }
    return sts;
}

vdatum_func *create_grid_vdatum_func( const std::string &grid_file, int isgeoid )
{
    const std::string description = std::string( isgeoid ? "Geoid" : "Grid offset" ) +
                                     " defined in " + std::filesystem::path( grid_file ).filename().string().substr( 0, 150 );
    return new vdatum_func( isgeoid ? "GEOID" : "GRID", description,
                             new grid_vdatum_func_data( grid_file, ! isgeoid ),
                             delete_grid_vdatum_func_data, describe_grid_vdatum_func,
                             copy_grid_vdatum_func_data, identical_grid_vdatum_func,
                             calc_grid_vdatum_func );
}

/*========================================================================================*/
/* Generic vertical datum function routines                                             */

vdatum_func::vdatum_func( std::string type_, std::string description_, void *data_,
                           void (*delete_data_)(void *data),
                           void (*describe_func_)(vdatum_func *hrf, output_string_def *os),
                           void *(*copy_data_)(void *data),
                           int (*identical_)(void *data1, void *data2),
                           int (*calc_height_)( vdatum_func *hrf, double llh[3], double *height, double *exu ) ) :
    type( std::move(type_) ), description( std::move(description_) ),
    hrs( nullptr ), data( data_ ),
    delete_data( delete_data_ ), describe_func( describe_func_ ),
    copy_data( copy_data_ ), identical( identical_ ), calc_height( calc_height_ )
{
}

vdatum_func::~vdatum_func()
{
    delete_data( data );
}

vdatum_func *copy_vdatum_func( vdatum_func *hrf )
{
    return new vdatum_func( hrf->type, hrf->description, hrf->copy_data( hrf->data ),
                             hrf->delete_data, hrf->describe_func, hrf->copy_data,
                             hrf->identical, hrf->calc_height );
}

int identical_vdatum_func( vdatum_func *hrf1, vdatum_func *hrf2 )
{
    if( hrf1->type != hrf2->type ) return 0;
    return hrf1->identical( hrf1->data, hrf2->data );
}

int calc_vdatum_func( vdatum_func *hrf, double llh[3], double *height, double *exu )
{
    if( height ) *height=0;
    if( ! hrf ) return INVALID_DATA;
    return hrf->calc_height( hrf, llh, height, exu );
}
