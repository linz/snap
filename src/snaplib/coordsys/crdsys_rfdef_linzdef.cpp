#include <stdio.h>
#include <math.h>
#include <optional>
#include <string_view>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_linzdef.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "util/pi.h"

#include "dbl4/snap_dbl4_interface.h"
#include "dbl4_utl_binsrc.h"
#include "dbl4_utl_blob.h"
#include "dbl4_utl_lnzdef.h"
#include "dbl4_utl_error.h"

#define VERSIONLEN 8

/// The opaque payload behind a LINZDEF ref_deformation's data member.
/// File-private to this translation unit - never exposed via a header, so
/// it has no external callers. version is truncated to VERSIONLEN
/// characters at construction, mirroring the linzdef binary format's own
/// fixed-width version field (dbl4_utl_lnzdef.cpp's own VERSIONLEN=8/
/// Version typedef), which is compared byte-for-byte against version
/// strings stored in a loaded model - the truncation is load-bearing, not
/// an arbitrary buffer limit. loaded/loadsts/blob/binsrc/linzdef are the
/// only fields ever mutated after construction - every real construction
/// starts unloaded, and rf_linzdef_load populates the rest lazily, on
/// first real use.
struct LinzDefModel
{
    LinzDefModel( std::string ldeffile, const std::string &version ) :
        ldeffile( std::move(ldeffile) ), version( version.substr(0,VERSIONLEN) ),
        loaded(0), loadsts(OK), blob(nullptr), binsrc(nullptr), linzdef(nullptr)
    {}
    LinzDefModel( const LinzDefModel& ) = delete;

    const std::string ldeffile; ///< Path to the linzdef model file
    const std::string version;   ///< The model version to select, truncated to VERSIONLEN characters, or empty for the default
    int loaded;                    ///< Whether rf_linzdef_load has run yet
    int loadsts;                    ///< OK, or the error code from the lazy load
    hBlob blob;                      ///< The open model file, or nullptr until loaded
    hBinSrc binsrc;                   ///< The model's binary source, or nullptr until loaded
    hLinzDefModel linzdef;              ///< The parsed model, or nullptr until loaded
};

/* Called when the configuration file includes a deformation command - the
   command is passed to define_deformation as the string model */

// #pragma warning (disable : 4100)

static void rf_linzdef_delete( void *data )
{
    LinzDefModel *model = (LinzDefModel *) data;
    if( model == NULL ) return;
    if( model->linzdef ) { utlReleaseLinzDef(model->linzdef); model->linzdef = NULL; }
    if( model->binsrc ) { utlReleaseBinSrc(model->binsrc); model->binsrc = NULL; }
    if( model->blob ) { utlBlobClose(model->blob); model->blob = NULL; }
    delete model;
}

static LinzDefModel *rf_linzdef_create( const std::string &ldeffile, const std::string &version )
{
    return new LinzDefModel( ldeffile, version );
}

static int rf_linzdef_load( LinzDefModel *model )
{
    if( ! model->loaded)
    {
        int sts;
        sts = utlCreateReadonlyFileBlob( model->ldeffile, &(model->blob) );
        if( sts == STS_OK ) sts = utlCreateBinSrc( model->blob, &(model->binsrc) );
        if( sts == STS_OK ) sts = utlCreateLinzDef( model->binsrc, &(model->linzdef) );
        if( sts == STS_OK && ! model->version.empty() )
        {
            sts=utlSetLinzDefVersion(model->linzdef,model->version.c_str());
        }
        model->loaded = 1;
        model->loadsts = sts == STS_OK ? OK : INVALID_DATA;
    }
    return model->loadsts;
}

static void *rf_linzdef_copy( void *src )
{
    if( ! src ) return nullptr;
    LinzDefModel *model=(LinzDefModel *) src;
    return rf_linzdef_create( model->ldeffile, model->version );
}


static int rf_linzdef_identical( void *pld1, void *pld2 )
{
    LinzDefModel *ld1 = (LinzDefModel *) pld1;
    LinzDefModel *ld2 = (LinzDefModel *) pld2;
    return ld1->ldeffile == ld2->ldeffile ? 1 : 0;
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
    std::optional<std::string_view> title;
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
        std::string buffer;
        sts = utlLinzDefTitle( model->linzdef, 1, title );
        if( sts == STS_OK && title )
        {
            buffer = std::string(title->substr(0,80));
        }
        sts = utlLinzDefTitle( model->linzdef, 3, title );
        if( sts == STS_OK && title && ! title->empty() )
        {
            if( ! buffer.empty() )
            {
                if( buffer.find(*title) == std::string::npos )
                {
                    buffer += " (" + std::string(title->substr(0,20)) + ")";
                }
            }
            else
            {
                buffer = "Version " + std::string(title->substr(0,20));
            }
        }
        write_output_string2(os,buffer,OSW_TRIMR | OSW_SKIPBLANK,"    ");
        /* Description */
        sts = utlLinzDefTitle( model->linzdef, 2, title );
        if( sts == STS_OK && title && ! title->empty() )
        {
            write_output_string2(os,*title,OSW_TRIMR | OSW_SKIPBLANK,"    ");
        }
    }
    return OK;
}

ref_deformation *rfdef_parse_linzdef( input_string_def &is )
{
    std::optional<std::string> ldeffile;
    std::string filename;
    std::string version;

    int sts = next_string_field( is.scanner, filename, MAX_FILENAME_LEN );
    if( sts != OK )
    {
        report_string_error( is, sts, "Missing filename for LINZDEF deformation");
        return nullptr;
    }

    sts = next_string_field( is.scanner, version, VERSIONLEN );
    if( sts != OK ) { version.clear(); }

    ldeffile = find_relative_file( is.sourcename, filename, ".grd" );
    if( ! ldeffile )
    {
        std::string errmess = "Cannot open LINZDEF deformation grid file " + filename;
        report_string_error(is, FILE_OPEN_ERROR, errmess );
        return nullptr;
    }

    return new ref_deformation( "LINZDEF", rf_linzdef_create( *ldeffile, version ),
                                 rf_linzdef_delete, rf_linzdef_copy, rf_linzdef_identical,
                                 rf_linzdef_describe, rf_linzdef_calc, nullptr );
}
