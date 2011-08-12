#include "snapconfig.h"
#include "snapplot_dragger.hpp"

//extern "C" {
#include "plotstns.h"
#include "plotconn.h"
//}

SnapplotMapDragger::SnapplotMapDragger( int pickTolerance )
    : pickTolerance(pickTolerance)
{
    connections = 0;
    radius = 10;
    shiftStation = false;
    SetAutoZoomPanMode();

}

SnapplotMapDragger::~SnapplotMapDragger()
{
    DeleteConnections();
}

bool SnapplotMapDragger::InitDrag( const wxMouseEvent &event )
{
    shiftStation = event.ShiftDown() && event.ControlDown();
    if( ! shiftStation ) return wxMapScaleDragger::InitDrag( event );

    if( ! mapWindow ) return false;
    wxMapScale &scale = mapWindow->GetScale();
    if( ! scale.IsValid() ) return false;

    MapPoint refpt;
    scale.PlotToWorld( dragStartPoint, refpt );

    double tolerance = pickTolerance * scale.GetScale();

    istn = nearest_station( refpt.x, refpt.y, tolerance );
    if( ! istn ) return false;

    // Ok, we have liftoff!!!

    // Make a list of connected points

    int nc = get_connection_count( istn );
    nConnections = 0;
    if( nc )
    {
        DeleteConnections();
        connections = new MapPoint[nc];
        while( nc-- )
        {
            char visible;
            int to = get_connected_station( istn, nc, &visible );
            if( !to || !visible ) continue;
            MapPoint &pt = connections[nConnections++];
            get_station_coordinates( to, &pt.x, &pt.y );
        }
    }
    return true;
}

void SnapplotMapDragger::EndDrag()
{
    if( ! shiftStation ) { wxMapScaleDragger::EndDrag(); return; }

    DeleteConnections();
    MapPoint newLoc;
    mapWindow->GetScale().PlotToWorld( dragEndPoint, newLoc );
    double x, y;
    get_station_coordinates( istn, &x, &y );
    x = newLoc.x - x;
    y = newLoc.y - y;
    if( using_station_offsets() )
    {
        double dx, dy;
        get_station_offset( istn, &dx, &dy );
        x += dx;
        y += dy;
    }
    set_station_offset( istn, x, y );
    use_station_offsets( 1 );
    mapWindow->RedrawMap();
}

void SnapplotMapDragger::DrawDragger( wxDC &dc )
{
    if( ! shiftStation ) { wxMapScaleDragger::DrawDragger(dc); return; }

    dc.DrawCircle( dragEndPoint, radius );
    MapPoint pt;
    mapWindow->GetScale().PlotToWorld( dragEndPoint, pt );
    for( int i = 0; i < nConnections; i++ )
    {
        wxPoint from;
        wxPoint to;
        if( mapWindow->GetScale().TrimLine( pt, connections[i], from, to ) )
        {
            dc.DrawLine( from, to );
        }
    }
}

void SnapplotMapDragger::DeleteConnections()
{
    if( connections != 0 ) delete [] connections;
    connections = 0;
    nConnections = 0;
}
