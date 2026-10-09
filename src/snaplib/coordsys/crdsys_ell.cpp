#include "snapconfig.h"
/* crdsysel.c:  Ellipsoid management for coordinate system routines */

/*
   $Log: crdsyse0.c,v $
   Revision 1.1  1995/12/22 16:32:47  CHRIS
   Initial revision

*/

#include "coordsys/coordsys.h"
#include <boost/algorithm/string/case_conv.hpp>

ellipsoid::ellipsoid( const std::string &code_, const std::string &name_, const double a_, const double rf_ ) :
    code( boost::algorithm::to_upper_copy(code_) ),
    name( name_ ),
    a( a_ ),
    b( rf_==0.0 ? a_ : a_-a_/rf_ ),
    rf( rf_ ),
    a2( a_*a_ ),
    b2( b*b ),
    a2b2( a2-b2 )
{
}

int identical_ellipsoids( ellipsoid *el1, ellipsoid *el2 )
{
    if( el1->a == el2->a && el1->rf == el2->rf ) return 1;
    return 0;
}
