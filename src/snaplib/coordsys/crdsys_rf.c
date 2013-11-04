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
   as a ratio, at a specific epoch */

void init_ref_frame( ref_frame *rf, double convepoch )
{
    int i;
    double dfactor=convepoch-rf->refdate;
    for( i=0; i < 3; i++ )
    {
        double rot;
        rf->trans[i] = rf->txyz[i]+dfactor*rf->dtxyz[i];
        rot=(rf->rxyz[i]+dfactor*rf->drxyz[i])*STOR;
        rf->csrot[i]=cos(rot);
        rf->snrot[i]=sin(rot);
    }
    rf->sclfct=1.0+(rf->scale+dfactor*rf->dscale)*0.000001;
    rf->calcdate=convepoch;
}

ref_frame *create_ref_frame( const char *code, const char *name, ellipsoid *el,
                             const char *refcode, double txyz[3], double rxyz[3], double scale,
                             double refdate, double dtxyz[3], double drxyz[3], double dscale
        )
{
    ref_frame *rf;
    int i;
    int use_rates;

    rf = (ref_frame *) check_malloc( sizeof( ref_frame ) );

    rf->code = copy_string( code );
    _strupr( rf->code );
    rf->name = copy_string( name );
    rf->el = el;
    rf->refcode = copy_string( refcode );
    if( rf->refcode ) _strupr( rf->refcode );
    rf->refrf = 0;
    rf->refdate = refdate;
    use_rates=0;
    for( i=0; i<3; i++ )
    {
        rf->txyz[i] = txyz[i];
        rf->rxyz[i] = rxyz[i];
        rf->dtxyz[i] = dtxyz[i];
        rf->drxyz[i] = drxyz[i];
        if(dtxyz[i] != 0 || drxyz[i] !=0 ) use_rates=1;
    }
    rf->scale = scale;
    rf->dscale = dscale;
    if( dscale != 0.0 ) use_rates=1;
    rf->use_rates = use_rates;
    rf->use_iersunits = 0;
    rf->func = NULL;
    rf->def = NULL;
    rf->defepoch = 0.0;
    rf->calcdate=0;
    init_ref_frame( rf, refdate );
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
                            rf->rxyz, rf->scale, rf->refdate,
                            rf->dtxyz, rf->drxyz, rf->dscale );
    if( rf1 )
    {
        rf1->use_iersunits=rf->use_iersunits;
    }
    if( rf1 ) 
    { 
        rf1->func = copy_ref_frame_func( rf->func );
        rf1->def = copy_ref_deformation( rf->def );
        rf1->defepoch = rf->defepoch;
        rf1->refrf = copy_ref_frame( rf1->refrf );
    }
    else
    {
        delete_ellipsoid( el );
    }
    return rf1;
}


void delete_ref_frame( ref_frame *rf )
{
    if( !rf ) return;
    if( rf->func ) delete_ref_frame_func( rf->func );
    rf->func = 0;
    if( rf->def ) delete_ref_deformation( rf->def);
    rf->def = 0;
    if( rf->refrf ) delete_ref_frame( rf->refrf );
    rf->refrf = 0;
    delete_ellipsoid( rf->el );
    check_free( rf->code );
    check_free( rf->name );
    if( rf->refcode ) { check_free( rf->refcode ); rf->refcode=0; }
    check_free( rf );
}


int identical_ref_frame_axes( ref_frame *rf1, ref_frame *rf2 )
{
    if( ! identical_datum( rf1, rf2 )  ) return 0;
    if( ! identical_ref_deformation(rf1->def,rf2->def)) return 0;
    if( rf1->def && rf1->defepoch != rf2->defepoch ) return 0;
    return 1;
}

int identical_datum( ref_frame *rf1, ref_frame *rf2 )
{
    int i;
    if( rf1->refcode && ! rf2->refcode ) return 0;
    if( ! rf1->refcode && rf2->refcode ) return 0;
    /* If no reference frame code then transformations are meaningless */
    if( rf1->refcode )
    {
        if( strcmp(rf1->refcode,rf2->refcode) != 0 ) return 0;
        if( rf1->refdate != rf2->refdate ) return 0;
        for( i=0; i<3; i++ )
        {
            if( rf1->txyz[i] != rf2->txyz[i] ) return 0;
            if( rf1->rxyz[i] != rf2->rxyz[i] ) return 0;
            if( rf1->dtxyz[i] != rf2->dtxyz[i] ) return 0;
            if( rf1->drxyz[i] != rf2->drxyz[i] ) return 0;
        }
        if( rf1->scale != rf2->scale ) return 0;
        if( rf1->dscale != rf2->dscale ) return 0;
        if( ! identical_ref_frame_func(rf1->func,rf2->func)) return 0;
    }
    return 1;
}

