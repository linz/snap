#include "snapconfig.h"
/* crdsyshrs.c:  Routines to manage vertical datums */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <filesystem>
#include <string>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_hrs_func.h"
#include "geoid/griddata.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include <boost/algorithm/string/case_conv.hpp>

vdatum::vdatum( const std::string &code_, const std::string &name_,
                vdatum *basehrs_, vdatum_func *hrf_,
                std::optional<std::string> source_ ) :
    code( boost::algorithm::to_upper_copy(code_) ),
    name( name_ ),
    source( std::move(source_) ),
    basehrs( basehrs_ ),
    rf( nullptr ),
    func( hrf_ )
{
    func->hrs = this;
}

vdatum::vdatum( const std::string &code_, const std::string &name_,
                ref_frame *rf_, vdatum_func *hrf_,
                std::optional<std::string> source_ ) :
    code( boost::algorithm::to_upper_copy(code_) ),
    name( name_ ),
    source( std::move(source_) ),
    basehrs( nullptr ),
    rf( rf_ ),
    func( hrf_ )
{
    func->hrs = this;
}

vdatum::~vdatum()
{
    delete basehrs;
    delete rf;
    delete func;
}

vdatum *geoid_vdatum( const std::string &geoidfile, ref_frame *rf )
{
    std::string hrs_name = std::filesystem::path( native_path(geoidfile) ).filename().string().substr(0,120);
    if( const size_t dot = hrs_name.find('.'); dot != std::string::npos ) hrs_name.resize(dot);
    hrs_name += " geoid";

    vdatum_func *hrf=create_grid_vdatum_func( geoidfile, 1 );
    return new vdatum( "geoid", hrs_name, rf, hrf );
}

vdatum *copy_vdatum( vdatum *hrs )
{
    if( ! hrs ) return nullptr;
    vdatum_func *hrf = hrs->func ? copy_vdatum_func( hrs->func ) : nullptr;
    if( hrs->rf )
    {
        return new vdatum( hrs->code, hrs->name, copy_ref_frame( hrs->rf ), hrf );
    }
    vdatum *basehrs = hrs->basehrs ? copy_vdatum( hrs->basehrs ) : nullptr;
    return new vdatum( hrs->code, hrs->name, basehrs, hrf );
}


int identical_vdatum( vdatum *hrs1, vdatum *hrs2 )
{
    if( ! identical_vdatum_func( hrs1->func, hrs2->func )) return 0;
    if( hrs1->rf && ! hrs2->rf ) return 0;
    if( ! hrs1->rf && hrs2->rf ) return 0;
    if( hrs1->basehrs && ! hrs2->basehrs ) return 0;
    if( ! hrs1->basehrs && hrs2->basehrs ) return 0;
    if( hrs1->rf && ! identical_datum( hrs1->rf, hrs2->rf )) return 0;
    if( hrs1->basehrs && ! identical_vdatum( hrs1->basehrs, hrs2->basehrs )) return 0;
    return 1;
}

int calc_vdatum_offset( vdatum *hrs, double llh[3], double *height, double *exu )
{
    int sts;
    if( height ) *height=0.0;
    if( exu ){ exu[0]=exu[1]=exu[2]=0.0; }
    if( ! hrs || ! hrs->func ) return INVALID_DATA;
    sts=calc_vdatum_func( hrs->func, llh, height, exu );
    if( sts != OK || ! hrs->basehrs ) return sts;
    while( (hrs=hrs->basehrs) )
    {
        double dh;
        double dexu[3];
        double *pexu=exu ? &(dexu[0]) : 0;
        if( ! hrs->func ) return INVALID_DATA;
        sts=calc_vdatum_func( hrs->func, llh, &dh, pexu );
        if( sts != OK ) return sts;
        if( height ) *height += dh;
        if( exu ){ exu[0] += dexu[0]; exu[1] += dexu[1]; exu[2] += dexu[2]; }
    }
    return INVALID_DATA;
}

vdatum *base_vdatum( vdatum *hrs )
{
    return hrs->basehrs;
}

ref_frame *vdatum_ref_frame( vdatum *hrs )
{
    while( hrs )
    {
        if( hrs->rf ) return hrs->rf;
        hrs=hrs->basehrs;
    }
    return nullptr;
}
