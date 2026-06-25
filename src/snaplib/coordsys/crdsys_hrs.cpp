#include "snapconfig.h"
/* crdsyshrs.c:  Routines to manage vertical datums */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_hrs_func.h"
#include "geoid/griddata.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/fileutil.h"
#include "util/errdef.h"


vdatum *create_vdatum( const char *code, const char *name, 
                               vdatum *basehrs, ref_frame *rf,
                               vdatum_func *hrf )
{
    vdatum *hrs;

    /* Need basehrs or rf, but not both */
    if( basehrs && rf ) return NULL;
    if( ! basehrs && ! rf ) return NULL;

    hrs = (vdatum *) check_malloc( sizeof( vdatum ) );
    hrs->code = copy_string( code );
    _strupr( hrs->code );
    hrs->name = copy_string( name );
    hrs->basehrs = basehrs;
    hrs->rf = rf;
    hrs->func = hrf;
    hrs->source = nullptr;
    hrf->hrs=hrs;
    return hrs;
}

vdatum *geoid_vdatum( const char *geoidfile, ref_frame *rf )
{
    char hrs_name[128];
    int plen=path_len(geoidfile,0);
    char *end;
    strncpy(hrs_name,geoidfile+plen,120);
    hrs_name[120]=0;
    end=strchr(hrs_name,'.');
    if( end ) *end=0;
    strcat( hrs_name," geoid");
    
    vdatum_func *hrf=create_grid_vdatum_func( geoidfile, 1 );
    vdatum *hrs=create_vdatum( "geoid", hrs_name, nullptr, rf, hrf );
    return hrs;
}

vdatum *copy_vdatum( vdatum *hrs )
{
    if( ! hrs ) return nullptr;
    vdatum *hrs1;
    vdatum_func *hrf=NULL;
    ref_frame *rf=NULL;
    vdatum *basehrs=NULL;
    if( hrs == NULL ) return NULL;
    if( hrs->func )
    {
        hrf=copy_vdatum_func( hrs->func );
    }
    if( hrs->basehrs ) basehrs=copy_vdatum( hrs->basehrs );
    if( hrs->rf ) rf=copy_ref_frame( hrs->rf );

    hrs1 = create_vdatum( hrs->code, hrs->name, basehrs, rf, hrf );
    return hrs1;
}


void delete_vdatum( vdatum *hrs )
{
    if( ! hrs ) return;
    if( hrs->basehrs ) delete_vdatum( hrs->basehrs ); 
    hrs->basehrs=0;
    if( hrs->rf ) delete_ref_frame( hrs->rf ); 
    hrs->rf=0;
    if( hrs->func ) delete_vdatum_func( hrs->func );
    hrs->func = 0;
    check_free( hrs->code );
    check_free( hrs->name );
    if( hrs->source ) check_free( hrs->source );
    check_free( hrs );
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
