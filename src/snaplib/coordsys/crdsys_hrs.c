#include "snapconfig.h"
/* crdsyshrs.c:  Routines to manage height reference surfaces */

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


height_ref *create_height_ref( const char *code, const char *name, 
                               height_ref *basehrs, ref_frame *rf,
                               height_ref_func *hrf )
{
    height_ref *hrs;

    /* Need basehrs or rf, but not both */
    if( basehrs && rf ) return NULL;
    if( ! basehrs && ! rf ) return NULL;

    hrs = (height_ref *) check_malloc( sizeof( height_ref ) );
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

height_ref *geoid_height_ref( const char *geoidfile, ref_frame *rf )
{
    char hrs_name[128];
    int plen=path_len(geoidfile,0);
    char *end;
    strncpy(hrs_name,geoidfile+plen,120);
    hrs_name[120]=0;
    end=strchr(hrs_name,'.');
    if( end ) *end=0;
    strcat( hrs_name," geoid");
    
    height_ref_func *hrf=create_grid_height_ref_func( geoidfile, 1 );
    height_ref *hrs=create_height_ref( "geoid", hrs_name, nullptr, rf, hrf );
    return hrs;
}

height_ref *copy_height_ref( height_ref *hrs )
{
    if( ! hrs ) return nullptr;
    height_ref *hrs1;
    height_ref_func *hrf=NULL;
    ref_frame *rf=NULL;
    height_ref *basehrs=NULL;
    if( hrs == NULL ) return NULL;
    if( hrs->func )
    {
        hrf=copy_height_ref_func( hrs->func );
    }
    if( hrs->basehrs ) basehrs=copy_height_ref( hrs->basehrs );
    if( hrs->rf ) rf=copy_ref_frame( hrs->rf );

    hrs1 = create_height_ref( hrs->code, hrs->name, basehrs, rf, hrf );
    return hrs1;
}


void delete_height_ref( height_ref *hrs )
{
    if( ! hrs ) return;
    if( hrs->basehrs ) delete_height_ref( hrs->basehrs ); 
    hrs->basehrs=0;
    if( hrs->rf ) delete_ref_frame( hrs->rf ); 
    hrs->rf=0;
    if( hrs->func ) delete_height_ref_func( hrs->func );
    hrs->func = 0;
    check_free( hrs->code );
    check_free( hrs->name );
    if( hrs->source ) check_free( hrs->source );
    check_free( hrs );
}


int identical_height_ref( height_ref *hrs1, height_ref *hrs2 )
{
    if( ! identical_height_ref_func( hrs1->func, hrs2->func )) return 0;
    if( hrs1->rf && ! hrs2->rf ) return 0;
    if( ! hrs1->rf && hrs2->rf ) return 0;
    if( hrs1->basehrs && ! hrs2->basehrs ) return 0;
    if( ! hrs1->basehrs && hrs2->basehrs ) return 0;
    if( hrs1->rf && ! identical_datum( hrs1->rf, hrs2->rf )) return 0;
    if( hrs1->basehrs && ! identical_height_ref( hrs1->basehrs, hrs2->basehrs )) return 0;
    return 1;
}

int calc_height_ref_offset( height_ref *hrs, double llh[3], double *height, double *exu )
{
    int sts;
    if( height ) *height=0.0;
    if( exu ){ exu[0]=exu[1]=exu[2]=0.0; }
    if( ! hrs || ! hrs->func ) return INVALID_DATA;
    sts=calc_height_ref_func( hrs->func, llh, height, exu );
    if( sts != OK || ! hrs->basehrs ) return sts;
    while( (hrs=hrs->basehrs) )
    {
        double dh;
        double dexu[3];
        double *pexu=exu ? &(dexu[0]) : 0;
        if( ! hrs->func ) return INVALID_DATA;
        sts=calc_height_ref_func( hrs->func, llh, &dh, pexu );
        if( sts != OK ) return sts;
        if( height ) *height += dh;
        if( exu ){ exu[0] += dexu[0]; exu[1] += dexu[1]; exu[2] += dexu[2]; }
    }
    return INVALID_DATA;
}

height_ref *base_height_ref( height_ref *hrs )
{
    return hrs->basehrs;
}

ref_frame *height_ref_ref_frame( height_ref *hrs )
{
    while( hrs )
    {
        if( hrs->rf ) return hrs->rf;
        hrs=hrs->basehrs;
    }
    return nullptr;
}
