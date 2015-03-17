
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
#include "csdeform.h"

typedef struct
{
    double x, y;
    double y0def[3];
    double year;
    double def[3];
} StationDeformation;


typedef struct
{
    StationDeformation *stdefs;
} CrdsysDefModel;

/* Called when the configuration file includes a deformation command - the
   command is passed to define_deformation as the string model */

// #pragma warning (disable : 4100)

static void delete_csdefmodel( CrdsysDefModel *model )
{
    if( model == NULL ) return;
    if( model->stdefs ) { check_free(model->stdefs); model->stdefs = NULL; }
    check_free(model);
}

static CrdsysDefModel *init_csdefmodel()
{
    CrdsysDefModel *model;
    if( ! has_deformation_model( net->crdsys ) ) return NULL;
    model = (CrdsysDefModel *) check_malloc( sizeof(CrdsysDefModel));
    model->stdefs = NULL;
    return model;
}

/* Called before the first adjustment iteration to perform any necessary
   preparation  - cache converted coordinates and epoch 0 deformations. */

static int init_csdef_deformation( void *deformation )
{
    double csepoch;
    int nstns, istn;
    StationDeformation *stdefs;
    char buf[128];
    CrdsysDefModel *model = (CrdsysDefModel *) deformation;

    if( ! model ) return 0;

    /* Allocate space for a set of station initial values  */

    nstns = number_of_stations( net );
    stdefs = (StationDeformation *) check_malloc( sizeof(StationDeformation) * (nstns+1) );
    model->stdefs = stdefs;

    /* For each station calculate the coordsys epoch coords */

    csepoch = deformation_model_epoch(net->crdsys);
    for( istn = 1; istn <= nstns; istn++ )
    {
        double xyz[3];
        station *st;
        StationDeformation *std;
        st = station_ptr( net, istn );
        std = stdefs+station_id( net, st );

        xyz[CRD_LAT] = st->ELat;
        xyz[CRD_LON] = st->ELon;
        xyz[CRD_HGT] = st->OHgt + st->GUnd;

        std->year = 0.0;
        std->y0def[0] = std->y0def[1] = std->y0def[2] = 0.0;

        if( csepoch != 0 )
        {
            int sts;
            sts = ref_deformation_at_epoch( net->crdsys->rf, xyz,
                                            csepoch, std->y0def );
            if( sts != OK )
            {
                sprintf(buf,"Cannot calculate deformation of %-20s at date %.1lf",
                        st->Code, csepoch);
                handle_error(WARNING_ERROR,buf,NO_MESSAGE);
                return INVALID_DATA;
            }
        }
    }

    return OK;
}

/* Called for each observation to determine the east, north, and vertical
   offset that the model predicts for a specific time */

static int calc_csdef_deformation( void *deformation, station *st, double date, double denu[3] )
{
    int stnid;
    StationDeformation *std;
    int sts;
    char buf[128];
    double year;
    CrdsysDefModel *model = (CrdsysDefModel *) deformation;

    denu[0] = denu[1] = denu[2] = 0.0;
    if( ! model ) return OK;

    stnid = station_id( net, st );
    std = model->stdefs+stnid;

    year = date_as_year( date );

    if( std->year == 0.0 || std->year != date )
    {
        double xyz[3];
        xyz[CRD_LAT] = st->ELat;
        xyz[CRD_LON] = st->ELon;
        xyz[CRD_HGT] = st->OHgt + st->GUnd;
        sts = ref_deformation_at_epoch( net->crdsys->rf, xyz,
                                        year, std->def );
        if( sts != OK )
        {
            sprintf(buf,"Cannot calculate deformation of %-20s at %.1lf",
                    st->Code, year);
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

static int print_csdef( void *deformation, FILE *out, const char *prefix )
{
    if( ! deformation ) return OK;
    fputs(prefix,out);
    fputs("Applying coordinate system deformation model\n",out);
    output_string_def os;
    output_string_to_file( &os, out );
    describe_deformation_model( &os, net->crdsys->rf );
    return OK;
}

static int delete_csdef( void *deformation )
{
    CrdsysDefModel *model = (CrdsysDefModel *) deformation;
    if( model ) delete_csdefmodel( model );
    return OK;
}

int create_crdsys_deformation( deformation_model **model )
{
    int sts;
    CrdsysDefModel *ldm;

    (*model) = NULL;
    sts = OK;
    ldm = init_csdefmodel( );
    if( ldm )
    {
        (*model) = create_deformation_model(
                       ldm,
                       init_csdef_deformation,
                       calc_csdef_deformation,
                       print_csdef,
                       delete_csdef);
    }
    return sts;
}

