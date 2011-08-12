
#include <stdio.h>
#include <math.h>

#include "snap/snapglob.h"
#include "snap/deform.h"
#include "snap/stnadj.h"
#include "coordsys/coordsys.h"
#include "util/chkalloc.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "util/pi.h"
#include "lnzdeform.h"

#include "dbl4/snap_dbl4_interface.h"
#include "dbl4_utl_binsrc.h"
#include "dbl4_utl_blob.h"
#include "dbl4_utl_lnzdef.h"
#include "dbl4_utl_error.h"

typedef struct
{
    double x, y;
    double y0def[3];
    double year;
    double def[3];
} StationDeformation;


typedef struct
{
    hBlob blob;
    hBinSrc binsrc;
    hLinzDefModel linzdef;
    double epoch;
    StationDeformation *stdefs;
} LinzDefModel;

/* Called when the configuration file includes a deformation command - the
   command is passed to define_deformation as the string model */

#pragma warning (disable : 4100)

static void delete_linzdefmodel( LinzDefModel *model )
{
    if( model == NULL ) return;
    if( model->linzdef ) { utlReleaseLinzDef(model->linzdef); model->linzdef = NULL; }
    if( model->binsrc ) { utlReleaseBinSrc(model->binsrc); model->binsrc = NULL; }
    if( model->linzdef ) { utlBlobClose(model->blob); model->blob = NULL; }
    if( model->stdefs ) { check_free(model->stdefs); model->stdefs = NULL; }
    check_free(model);
}

static LinzDefModel *init_linzdefmodel( char *pmodel, double pepoch )
{
    char *deffile;
    LinzDefModel *model;
    int sts;

    model = NULL;

    set_find_file_directories( program_file_name, cmd_dir, user_dir );
    deffile = find_file( pmodel, ".ldm", FF_TRYPROGDIR | FF_TRYHOMEDIR | FF_TRYBASEDIR );
    if( !deffile ) return NULL;

    model = (LinzDefModel *) check_malloc( sizeof(LinzDefModel));
    model->blob = NULL;
    model->binsrc = NULL;
    model->linzdef = NULL;
    model->epoch = pepoch;
    model->stdefs = NULL;

    sts = utlCreateReadonlyFileBlob( deffile, &(model->blob) );
    if( sts == STS_OK ) sts = utlCreateBinSrc( model->blob, &(model->binsrc) );
    if( sts == STS_OK ) sts = utlCreateLinzDef( model->binsrc, &(model->linzdef) );

    if( sts != STS_OK ) { delete_linzdefmodel( model ); model = NULL; }

    return model;
}

/* Called before the first adjustment iteration to perform any necessary
   preparation  - cache converted coordinates and epoch 0 deformations. */

static int init_linzdef_deformation( void *deformation )
{
    char *vcsdef;
    coordsys *vcs;
    coord_conversion tovcs;
    double factor;
    int nstns, istn;
    StatusType sts;
    StationDeformation *stdefs;
    char buf[128];

    LinzDefModel *model = (LinzDefModel *) deformation;

    if( ! model ) return OK;

    sts = utlLinzDefCoordSysDef(model->linzdef,&vcsdef);
    if( sts != STS_OK ) return INVALID_DATA;

    vcs = load_coordsys( vcsdef );
    if( !vcs )
    {
        sprintf( buf,"Cannot load deformation model coordinate system %-20s",vcsdef);
        handle_error(WARNING_ERROR,buf,NO_MESSAGE);
        return INVALID_DATA;
    }
    if( define_coord_conversion( &tovcs, net->geosys, vcs ) != OK )
    {
        sprintf(buf,"Cannot convert station coordinates to coordinate system %-20s of deformation model",
                vcs->code);
        handle_error(WARNING_ERROR,buf,NO_MESSAGE);
        return INVALID_DATA;
    }
    factor = vcs->crdtype == CSTP_GEODETIC ? RTOD  : 1.0;

    /* Allocate space for a set of deformation parameters.. */

    nstns = number_of_stations( net );
    stdefs = (StationDeformation *) check_malloc( sizeof(StationDeformation) * (nstns+1) );
    model->stdefs = stdefs;

    /* For each station calculate the velocity */

    for( istn = 1; istn <= nstns; istn++ )
    {
        double xyz[3];
        station *st;
        StationDeformation *std;
        st = station_ptr( net, istn );
        std = stdefs+istn;

        xyz[CRD_LAT] = st->ELat;
        xyz[CRD_LON] = st->ELon;
        xyz[CRD_HGT] = st->OHgt + st->GUnd;
        if( convert_coords( &tovcs, xyz, NULL, xyz, NULL ) != OK )
        {
            sprintf(buf,"Cannot convert coordinates of %-20s to deformation model coordinate system %-20s",
                    st->Code, vcs->code);
            handle_error(WARNING_ERROR,buf,NO_MESSAGE);
            return INVALID_DATA;
        }

        std->x = xyz[0]*factor;
        std->y = xyz[1]*factor;
        std->year = 0.0;
        std->y0def[0] = std->y0def[1] = std->y0def[2] = 0.0;

        if( model->epoch != 0 )
        {
            sts = utlCalcLinzDef( model->linzdef, model->epoch, std->x, std->y, std->y0def );
            if( sts != STS_OK )
            {
                sprintf(buf,"Cannot calculate deformation of %-20s at date %.1lf",
                        st->Code, model->epoch);
                handle_error(WARNING_ERROR,buf,NO_MESSAGE);
                return INVALID_DATA;
            }
        }

    }

    /* Release resource held by grid now that we have all values from it! */
    delete_coordsys( vcs );
    return OK;
}

/* Called for each observation to determine the east, north, and vertical
   offset that the model predicts for a specific time */

static int calc_linzdef_deformation( void *deformation, station *st, double date, double denu[3] )
{
    int stnid;
    StationDeformation *std;
    int sts;
    char buf[128];
    double year;

    LinzDefModel *model = (LinzDefModel *) deformation;

    stnid = find_station( net, st->Code );
    std = &(model->stdefs[stnid]);

    year = date_as_year( date );

    if( std->year == 0.0 || std->year != date )
    {
        sts = utlCalcLinzDef( model->linzdef, year, std->x, std->y, std->def );
        if( sts != STS_OK )
        {
            sprintf(buf,"Cannot calculate deformation of %-20s at %.1lf",
                    st->Code,year);
            handle_error(WARNING_ERROR,buf,NO_MESSAGE);
            return INVALID_DATA;
        }
        std->year = year;
        std->def[0] -= std->y0def[0];
        std->def[1] -= std->y0def[1];
        std->def[2] -= std->y0def[2];
    }

    denu[0] = std->def[0];
    denu[1] = std->def[1];
    denu[2] = std->def[2];

    return OK;
}

/* Describe the deformation model in an output file */

static int print_linzdef( void *deformation, FILE *out, char *prefix )
{
    char *title;
    int i;
    int sts;
    LinzDefModel *model = (LinzDefModel *) deformation;
    if( ! model ) return OK;

    for( i = 1; i <= 3; i++ )
    {
        sts = utlLinzDefTitle( model->linzdef, i, &title );
        if( sts == STS_OK && title )
        {
            fputs(prefix,out);
            fputs(title,out);
            fputs("\n",out);
        }
    }
    return OK;
}

static int delete_linzdef( void *deformation )
{
    LinzDefModel *model = (LinzDefModel *) deformation;
    if( model ) delete_linzdefmodel( model );
    return OK;
}

int create_linzdef_deformation( deformation_model **model, char *pmodel, double pepoch )
{
    int sts;
    LinzDefModel *ldm;

    (*model) = NULL;
    sts = INVALID_DATA;
    ldm = init_linzdefmodel( pmodel, pepoch );
    if( ldm )
    {
        (*model) = create_deformation_model(
                       ldm,
                       init_linzdef_deformation,
                       calc_linzdef_deformation,
                       print_linzdef,
                       delete_linzdef);
        sts = OK;
    }
    return sts;
}

