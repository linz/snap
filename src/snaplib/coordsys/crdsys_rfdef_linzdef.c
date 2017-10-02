#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_linzdef.h"
#include "util/chkalloc.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/dstring.h"
#include "util/errdef.h"
#include "util/pi.h"

#include "dbl4/snap_dbl4_interface.h"
#include "dbl4_utl_binsrc.h"
#include "dbl4_utl_blob.h"
#include "dbl4_utl_lnzdef.h"
#include "dbl4_utl_error.h"

typedef struct
{
    char *ldeffile;
    int loaded;
    int loadsts;
    hBlob blob;
    hBinSrc binsrc;
    hLinzDefModel linzdef;
} LinzDefModel;

/* Called when the configuration file includes a deformation command - the
   command is passed to define_deformation as the string model */

// #pragma warning (disable : 4100)

static void rf_linzdef_delete( void *data )
{
    LinzDefModel *model = (LinzDefModel *) data;
    if( model == NULL ) return;
    if( model->ldeffile ) check_free( model->ldeffile );
    if( model->linzdef ) { utlReleaseLinzDef(model->linzdef); model->linzdef = NULL; }
    if( model->binsrc ) { utlReleaseBinSrc(model->binsrc); model->binsrc = NULL; }
    if( model->linzdef ) { utlBlobClose(model->blob); model->blob = NULL; }
    check_free(model);
    return;
}

static void *rf_linzdef_create( const char *ldeffile )
{
    LinzDefModel *model;
    model = (LinzDefModel *) check_malloc( sizeof(LinzDefModel));
    model->ldeffile = copy_string(ldeffile);
    model->blob = NULL;
    model->binsrc = NULL;
    model->linzdef = NULL;
    model->loaded = 0;
    model->loadsts = OK;
    return model;
}

static int rf_linzdef_load( LinzDefModel *model )
{
    if( ! model->loaded)
    {
        int sts;
        sts = utlCreateReadonlyFileBlob( model->ldeffile, &(model->blob) );
        if( sts == STS_OK ) sts = utlCreateBinSrc( model->blob, &(model->binsrc) );
        if( sts == STS_OK ) sts = utlCreateLinzDef( model->binsrc, &(model->linzdef) );
        model->loaded = 1;
        model->loadsts = sts == STS_OK ? OK : INVALID_DATA;
    }
    return model->loadsts;
}

static void *rf_linzdef_copy( void *src )
{
    if( ! src ) return 0;
    return rf_linzdef_create( ((LinzDefModel *)src)->ldeffile );
}


static int rf_linzdef_identical( void *pld1, void *pld2 )
{
    LinzDefModel *ld1 = (LinzDefModel *) pld1;
    LinzDefModel *ld2 = (LinzDefModel *) pld2;
    return strcmp(ld1->ldeffile, ld2->ldeffile) == 0 ? 1 : 0;
}

/* Called for each observation to determine the east, north, and vertical
   offset that the model predicts for a specific time */

static int rf_linzdef_calc( ref_frame *rf, double lon, double lat, double epoch, double denu[3])
{
    int sts;
    ref_deformation *def = rf->def;
    LinzDefModel *model = (LinzDefModel *) (def->data);
    denu[0] = denu[1] = denu[2] = 0.0;
    if( epoch == 0 ) return OK;

    sts = rf_linzdef_load( model );
    if( sts == OK )
    {
        sts = utlCalcLinzDef( model->linzdef, epoch, lon*RTOD, lat*RTOD, denu ) == STS_OK ?
              OK : INVALID_DATA;
    }
    return sts;
}

/* Describe the deformation model */

static int rf_linzdef_describe( ref_frame *rf, output_string_def *os )
{
    char *title;
    char buffer[128];
    int i;
    int sts;
    ref_deformation *def = rf->def;
    LinzDefModel *model = (LinzDefModel *) (def->data);
    if( ! model ) return OK;

    write_output_string(os,"LINZ deformation model\n");
    sts = rf_linzdef_load( model );
    if( sts != OK )
    {
        write_output_string(os,"   Cannot load from file ");
        write_output_string(os,model->ldeffile);
        write_output_string(os,"\n");
    }
    else
    {
        /* Name and version */
        buffer[0]=0;
        sts = utlLinzDefTitle( model->linzdef, 1, &title );
        if( sts == STS_OK && title )
        {
            sprintf(buffer,"%.80s",title);
        }
        sts = utlLinzDefTitle( model->linzdef, 3, &title );
        if( sts == STS_OK && title && title[0] )
        {
            if( buffer[0] )
            {
                if( ! strstr(buffer,title) )
                {
                    sprintf(buffer+strlen(buffer)," (%.20s)",title);
                }
            }
            else
            {
                sprintf(buffer,"Version %.20s",title);
            }
        }
        write_output_string2(os,buffer,OSW_TRIMR | OSW_SKIPBLANK,"    ");
        /* Description */
        sts = utlLinzDefTitle( model->linzdef, 2, &title );
        if( sts == STS_OK && title && title[0])
        {
            write_output_string2(os,title,OSW_TRIMR | OSW_SKIPBLANK,"    ");
        }
    }
    return OK;
}

int rfdef_parse_linzdef( ref_deformation *def, input_string_def *is )
{
    const char *ldeffile;
    char filename[MAX_FILENAME_LEN];
    int sts;

    sts = next_string_field( is, filename, MAX_FILENAME_LEN );
    if( sts != OK )
    {
        report_string_error( is, sts, "Missing filename for LINZDEF deformation");
        return sts;
    }

    ldeffile = find_relative_file( is->sourcename, filename, ".grd" );
    if( ! ldeffile )
    {
        char errmess[80+MAX_FILENAME_LEN];
        sts = FILE_OPEN_ERROR;
        sprintf(errmess,"Cannot open LINZDEF deformation grid file %s",filename);
        report_string_error(is, sts, errmess );
        return sts;
    }

    def->data = rf_linzdef_create( ldeffile );
    def->delete_func = rf_linzdef_delete;
    def->copy_func = rf_linzdef_copy;
    def->identical = rf_linzdef_identical;
    def->describe_func = rf_linzdef_describe;
    def->calc_denu = rf_linzdef_calc;
    def->apply_llh = 0;
    return sts;
}
