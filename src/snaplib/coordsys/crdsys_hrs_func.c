#include "snapconfig.h"
/* crdsyshrs.c:  Routines to manage vertical datums */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_hrs_func.h"
#include "geoid/geoid.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/geodetic.h"
#include "util/pi.h"

/*========================================================================================*/
/* Generic vertical datum function routine                                              */

static vdatum_func *create_vdatum_func( const char *type, const char *description )
{
    int hrfsize=sizeof(vdatum_func_s)+strlen(type)+strlen(description)+2;
    vdatum_func *hrf = (vdatum_func * ) check_malloc( hrfsize ); 
    char *ptr=((char *)(void *) hrf) + sizeof(vdatum_func);
    hrf->type=ptr;
    strcpy(ptr,type);
    ptr += strlen(type)+1;
    hrf->description=ptr;
    strcpy(ptr,description);
    hrf->hrs=nullptr;
    hrf->data=nullptr;
    hrf->delete_func=nullptr;
    hrf->copy_func=nullptr;
    hrf->identical=nullptr;
    hrf->calc_height=nullptr;
    return hrf;
}

/*========================================================================================*/
/* Offset vertical datum function routine                                               */

static void describe_offset_vdatum_func( vdatum_func *hrf, output_string_def *os )
{
    return;
}

static int identical_offset_vdatum_func( void *data1, void *data2 )
{
    return (*(double *)data1) == (*(double *)data2);
}

static void *copy_offset_vdatum_func_data( void *data )
{
    void *copy=check_malloc( sizeof(double));
    memcpy(copy,data,sizeof(double));
    return copy;
}

static int calc_offset_vdatum_func( vdatum_func *hrf, double llh[3], double *height, double *exu )
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
    vdatum_func *hrf=create_vdatum_func( "OFFSET", description );
    hrf->data=check_malloc(sizeof(double));
    *(double *)(hrf->data)=offset;
    hrf->delete_func=check_free;
    hrf->describe_func=describe_offset_vdatum_func;
    hrf->copy_func=copy_offset_vdatum_func_data;
    hrf->identical=identical_offset_vdatum_func;
    hrf->calc_height=calc_offset_vdatum_func;
    return hrf;
}

/*========================================================================================*/
/* Grid based vertical datum function routine                                           */

typedef struct 
{
    char *filename;
    geoid_def *gd;
    coordsys *rfcs;
    coord_conversion *rfconv;
    coord_conversion *irfconv;
    int loadsts;
    int isoffset;  /* Offset is offset to height coord, so negative of offset to surface */
} grid_vdatum_func_data;

static grid_vdatum_func_data *create_grid_vdatum_func_data( const char *filename, int isoffset )
{
    grid_vdatum_func_data *ghrfd=(grid_vdatum_func_data *)
        check_malloc(sizeof(grid_vdatum_func_data)+strlen(filename)+1);
    char *gfilename=((char *)(void *)ghrfd)+sizeof(grid_vdatum_func_data);
    ghrfd->filename=gfilename;
    strcpy(gfilename,filename);
    ghrfd->gd=nullptr;
    ghrfd->rfcs=nullptr;
    ghrfd->rfconv=nullptr;
    ghrfd->irfconv=nullptr;
    ghrfd->loadsts=OK;
    ghrfd->isoffset=isoffset;
    return ghrfd;
}

static void delete_grid_vdatum_func_data( void *data )
{
    if( ! data ) return;
    grid_vdatum_func_data *ghrfd=(grid_vdatum_func_data *) data;
    if( ghrfd->gd ) { delete_geoid_grid( ghrfd->gd ); ghrfd->gd=nullptr; }
    if( ghrfd->rfcs ) { delete_coordsys( ghrfd->rfcs ); ghrfd->rfcs=nullptr; }
    if( ghrfd->rfconv ) { check_free( ghrfd->rfconv ); }
    check_free( data );
}

static int load_grid_vdatum_func( vdatum_func *hrf, grid_vdatum_func_data *ghrfd )
{
    if( ghrfd->gd || ghrfd->loadsts != OK ) 
    {
        return ghrfd->loadsts;
    }
    if( ! hrf->hrs )
    {
        handle_error(INTERNAL_ERROR,"load_grid_vdatum_func called before hrs set",nullptr);
        return INTERNAL_ERROR;
    }
    ref_frame *rf=vdatum_ref_frame( hrf->hrs );
    if( ! rf )
    {
        handle_error(INTERNAL_ERROR,"load_grid_vdatum_func called before hrs set",nullptr);
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
        ghrfd->rfconv=(coord_conversion *) check_malloc( sizeof(coord_conversion)*2 );
        ghrfd->irfconv=ghrfd->rfconv+1;
        ghrfd->rfcs=create_coordsys( rf->code, rf->name, CSTP_GEODETIC, rf, nullptr );
        ghrfd->rfcs->ownsrf=0;
        ghrfd->loadsts=define_coord_conversion_epoch( ghrfd->rfconv, 
                ghrfd->rfcs, get_geoid_coordsys( ghrfd->gd ),
                DEFAULT_CRDSYS_EPOCH );
        if( ghrfd->loadsts == OK )
        {
            ghrfd->loadsts=define_coord_conversion_epoch( ghrfd->irfconv, 
                get_geoid_coordsys( ghrfd->gd ), ghrfd->rfcs, 
                DEFAULT_CRDSYS_EPOCH );
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
            const char *title=geoid_title( ghrfd->gd, i );
            if( title && strlen(title) > 0 )
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
    return strcmp(
            ((grid_vdatum_func_data *)data1)->filename,
            ((grid_vdatum_func_data *)data2)->filename
            ) == 0;
}

static void *copy_grid_vdatum_func_data( void *data )
{
    grid_vdatum_func_data *ghrfd=(grid_vdatum_func_data *) data;
    return create_grid_vdatum_func_data(ghrfd->filename,ghrfd->isoffset);
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

vdatum_func *create_grid_vdatum_func( const char *grid_file, int isgeoid )
{
    char description[256];
    sprintf(description,"%s defined in %.150s", 
            isgeoid ? "Geoid" : "Grid offset",
            grid_file+path_len(grid_file,0) );
    vdatum_func *hrf=create_vdatum_func( isgeoid ? "GEOID" : "GRID", description );
    hrf->data=create_grid_vdatum_func_data( grid_file, ! isgeoid );
    hrf->delete_func=delete_grid_vdatum_func_data;
    hrf->describe_func=describe_grid_vdatum_func;
    hrf->copy_func=copy_grid_vdatum_func_data;
    hrf->identical=identical_grid_vdatum_func;
    hrf->calc_height=calc_grid_vdatum_func;
    return hrf;
}

/*========================================================================================*/
/* Generic vertical datum function routines                                             */

void delete_vdatum_func( vdatum_func *hrf )
{
    if( hrf->data && hrf->delete_func ) hrf->delete_func( hrf->data );
    hrf->data=0;
    check_free( hrf );
}

vdatum_func *copy_vdatum_func( vdatum_func *hrf )
{
    vdatum_func *newhrf=create_vdatum_func( hrf->type, hrf->description );
    newhrf->delete_func=hrf->delete_func;
    newhrf->copy_func=hrf->copy_func;
    newhrf->identical=hrf->identical;
    newhrf->calc_height=hrf->calc_height;
    if( newhrf->copy_func ) newhrf->data=hrf->copy_func( hrf->data );
    return newhrf;
}

int identical_vdatum_func( vdatum_func *hrf1, vdatum_func *hrf2 )
{
    if( strcmp(hrf1->type,hrf2->type) != 0 ) return 0;
    if( hrf1->identical && ! hrf1->identical(hrf1->data,hrf2->data)) return 0;
    return 1;
}

int calc_vdatum_func( vdatum_func *hrf, double llh[3], double *height, double *exu )
{
    if( height ) *height=0;
    if( ! hrf || ! hrf->calc_height ) return INVALID_DATA;
    return hrf->calc_height( hrf, llh, height, exu );
}
