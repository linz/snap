#include "snapconfig.h"
// wxmapdc: Code related to the drawing a map, scaling etc.

#include "wxmapdc.hpp"
#include <math.h>


//---------------------------------------------------------------------------
// wxMapDC bit.. mainly inline functions, but include code here for
// MoveTo and LineTo using real world coordinates...

bool wxMapDC::DrawLine( const MapPoint &mptFrom, const MapPoint &mptTo )
{
    wxPoint ptFrom;
    wxPoint ptTo;
    bool start;
    bool result = false;
    scale.AddPoint( mptFrom, true );
    scale.AddPoint( mptTo, false );
    while( scale.GetPoint(ptFrom,start) && scale.GetPoint(ptTo,result) )
    {
        dc->DrawLine( ptFrom, ptTo );
        result = true;
    }
    return result;
}

//void wxMapDC::LineTo( const MapPoint &mpt ) {
//   wxPoint pt;
//   bool start;
//   scale.AddPoint( mpt, false );
//   while( scale.GetPoint(pt,start) ) {
//      if( start ) {
//         dc->MoveTo(pt.x, pt.y);
//         }
//      else {
//         dc->LineTo(pt.x, pt.y);
//         }
//      }
//   }
