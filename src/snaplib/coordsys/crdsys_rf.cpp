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

#include <math.h>
#include "coordsys/coordsys.h"
#include "util/pi.h"
#include <boost/algorithm/string/case_conv.hpp>

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

namespace
{
bool compute_use_rates( const double dtxyz[3], const double drxyz[3], double dscale )
{
    for( int i=0; i<3; i++ )
    {
        if( dtxyz[i] != 0 || drxyz[i] != 0 ) return true;
    }
    return dscale != 0.0;
}
}

ref_frame::ref_frame( const std::string &code_, const std::string &name_, ellipsoid *el_,
                       std::optional<std::string> refcode_, double txyz_[3], double rxyz_[3], double scale_,
                       double refdate_, double dtxyz_[3], double drxyz_[3], double dscale_,
                       ref_frame_func *func_, ref_deformation *def_, int use_iersunits_ ) :
    code( boost::algorithm::to_upper_copy(code_) ),
    name( name_ ),
    el( el_ ),
    scale( scale_ ),
    dscale( dscale_ ),
    refdate( refdate_ ),
    use_rates( compute_use_rates(dtxyz_,drxyz_,dscale_) ),
    use_iersunits( use_iersunits_ ),
    refcode( refcode_ ? std::make_optional(boost::algorithm::to_upper_copy(*refcode_)) : std::nullopt ),
    refrf( nullptr ),
    func( func_ ),
    defepoch( 0.0 ),
    def( def_ )
{
    for( int i=0; i<3; i++ )
    {
        txyz[i] = txyz_[i];
        rxyz[i] = rxyz_[i];
        dtxyz[i] = dtxyz_[i];
        drxyz[i] = drxyz_[i];
    }
    init_ref_frame( this, refdate_ );
}

ref_frame::~ref_frame()
{
    delete func;
    if( def ) delete_ref_deformation( def );
    delete refrf;
    delete el;
}

ref_frame *copy_ref_frame( ref_frame *rf )
{
    if( rf == nullptr ) return nullptr;
    if( ! rf->el ) return nullptr;
    ellipsoid *el = new ellipsoid( *rf->el );
    ref_frame_func *func = copy_ref_frame_func( rf->func );
    ref_deformation *def = copy_ref_deformation( rf->def );
    ref_frame *rf1 = new ref_frame( rf->code, rf->name, el, rf->refcode, rf->txyz,
                                     rf->rxyz, rf->scale, rf->refdate,
                                     rf->dtxyz, rf->drxyz, rf->dscale,
                                     func, def, rf->use_iersunits );
    rf1->defepoch = rf->defepoch;
    rf1->refrf = copy_ref_frame( rf->refrf );
    return rf1;
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
        if( *rf1->refcode != *rf2->refcode ) return 0;
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
