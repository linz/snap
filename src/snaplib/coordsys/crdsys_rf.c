#include "snapconfig.h"
/* crdsysrf.c:  Reference frame management for coordinate system routines */

/*
   $Log: crdsysr0.c,v $
   Revision 1.2  2003/11/28 01:59:25  ccrook
   Updated to be able to use grid transformation for datum changes (ie to
   support official NZGD49-NZGD2000 conversion)

   Revision 1.1  1995/12/22 16:47:20  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "coordsys/coordsys.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/pi.h"



static char rcsid[]="$Id: crdsysr0.c,v 1.2 2003/11/28 01:59:25 ccrook Exp $";

/* Routine to calculate sines/cosines of rotations and scale difference
   as a ratio */

void init_ref_frame( ref_frame *rf )
{
    int i;
    for( i=0; i<3; i++ )
    {
        rf->csrot[i] = cos( rf->rxyz[i] * STOR );
        rf->snrot[i] = sin( rf->rxyz[i] * STOR );
    }
    rf->ratio = 1.0 + rf->scale * 1.0e-6;
}

ref_frame *create_ref_frame( const char *code, const char *name, ellipsoid *el,
                             const char *refcode, double txyz[3], double rxyz[3], double scale )
{

    ref_frame *rf;
    int i;

    rf = (ref_frame *) check_malloc( sizeof( ref_frame ) );

    rf->code = copy_string( code );
    _strupr( rf->code );
    rf->name = copy_string( name );
    rf->el = el;
    rf->refcode = copy_string( refcode );
    _strupr( rf->refcode );
    for( i=0; i<3; i++ )
    {
        rf->txyz[i] = txyz[i];
        rf->rxyz[i] = rxyz[i];
    }
    rf->scale = scale;
    rf->func = NULL;
    rf->def = NULL;
    init_ref_frame( rf );
    return rf;
}


ref_frame *copy_ref_frame( ref_frame *rf )
{
    ellipsoid *el;
    ref_frame *rf1;
    if( rf == NULL ) return NULL;
    el = copy_ellipsoid( rf->el );
    if( !el ) return NULL;
    rf1 = create_ref_frame( rf->code, rf->name, el, rf->refcode, rf->txyz,
                            rf->rxyz, rf->scale );
    if( !rf1 ) delete_ellipsoid( el );
    if( rf1 ) { rf1->func = copy_ref_frame_func( rf->func ); }
    if( rf1 ) { rf1->def = copy_ref_deformation( rf->def ); }
    return rf1;
}


void delete_ref_frame( ref_frame *rf )
{
    if( !rf ) return;
    if( rf->func ) delete_ref_frame_func( rf->func );
    rf->func = 0;
    if( rf->def ) delete_ref_deformation( rf->def);
    rf->def = 0;
    delete_ellipsoid( rf->el );
    check_free( rf->code );
    check_free( rf->name );
    check_free( rf->refcode );
    check_free( rf );
}

int identical_ref_frame_axes( ref_frame *rf1, ref_frame *rf2 )
{
    int i;
    if( strcmp(rf1->refcode,rf2->refcode) != 0 ) return 0;
    for( i=0; i<3; i++ )
    {
        if( rf1->txyz[i] != rf2->txyz[i] ) return 0;
        if( rf1->rxyz[i] != rf2->rxyz[i] ) return 0;
    }
    if( rf1->scale != rf2->scale ) return 0;
    if( ! identical_ref_frame_func(rf1->func,rf2->func)) return 0;
    if( ! identical_ref_deformation(rf1->def,rf2->def)) return 0;
    return 1;
}

