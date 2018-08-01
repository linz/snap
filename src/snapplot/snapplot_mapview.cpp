#include "snapconfig.h"
#include "snapplot_mapview.hpp"
#include "snapplot_event.hpp"

//extern "C" {
#include "plotpens.h"
#include "plotstns.h"
#include "plotconn.h"
#include "snap/stnadj.h"
//}

DEFINE_EVENT_TYPE( WX_MAPVIEW_DISPLAY_COORDS)

BEGIN_EVENT_TABLE( SnapplotMapView, wxMapWindow )
    EVT_IDLE( SnapplotMapView::OnIdle )
    EVT_MAPWINDOW_EVENT( WX_MAPWINDOW_POSITION, SnapplotMapView::OnMapWindowPosition )
    EVT_MAPWINDOW_EVENT( WX_MAPWINDOW_CLICKED, SnapplotMapView::OnClickEvent )
    EVT_PAINT( SnapplotMapView::OnPaint )
END_EVENT_TABLE()

// TODO: Fix up code here to use real colour and XOR with background colour
// TODO: Fix up code here to calculate appropriate radius...

static int locatorInnerWidth = 7;
static int locatorOuterWidth = 10;
static int locatorLockedWidth = 13;

SnapplotMapView::SnapplotMapView( wxWindow *parent ) :
    wxMapWindow( parent, -1 )
{
    // Set up the crosshair cursor with a white outline..

    SetCursor( wxCursor("CUR_CROSSHAIR2"));

    // TODO: Work out a better pick tolerance ... maybe need to
    // base on symbol size...

    pickTolerance = 10;
    positionChanged = false;
    locatorLocked = false;
    weakLock = true;
    locatorFrom = 0;
    locatorTo = 0;

    mapDragger.SetMoveOriginOnShift( true );
    mapDragger.SetPickTolerance( pickTolerance );
    SetDragger( &mapDragger );

    SetSendPositionEvent();
}

SnapplotMapView::~SnapplotMapView()
{
}

void SnapplotMapView::OnClickEvent( wxMapWindowEvent &event )
{
    const wxMouseEvent &me = event.GetMouseEvent();
    if( me.ButtonUp( wxMOUSE_BTN_LEFT ) )
    {
        ShowInfo( GetCursorPosition(), me.ShiftDown() );
    }
    else if ( me.ButtonUp( wxMOUSE_BTN_RIGHT ) )
    {
        if( me.ControlDown() && me.ShiftDown() )
        {
            int istn = FindStation( event.GetMapPoint() );
            if( istn )
            {
                double dx, dy;
                get_station_offset( istn, &dx, &dy );
                if( dx != 0.0 || dy != 0.0 )
                {
                    set_station_offset( istn, 0.0, 0.0 );
                    RedrawMap();
                }
            }
        }
        else if( me.ShiftDown() )
        {
            GetScale().ZoomOut();
        }
        else if ( me.AltDown() )
        {
            GetScale().ZoomAll();
        }
        else
        {
            GetScale().ZoomPrev();
        }
    }
}

void SnapplotMapView::ZoomToStation( int istn )
{
    ZoomToLine( istn, 0 );
}

void SnapplotMapView::ZoomToLine( int from, int to )
{
    double x1, y1, x2, y2, xm, ym;
    if( !from ) return;
    get_station_coordinates( from, &x1, &y1 );

    // If zooming to a line then just get the extents of the line
    if( to && to != from )
    {
        get_station_coordinates( to, &x2, &y2 );
        double dx = fabs( x1 - x2 );
        double dy = fabs( y1 - y2 );
        if( dx > 0.0 || dy > 0.0 )
        {
            x1 = (x1+x2)/2.0;
            y1 = (y1+y2)/2.0;
            GetScale().ZoomTo(MapRect( x1 - dx, y1 - dy, x1 + dx, y1 + dy ) );
            return;
        }
    }

    // Otherwise get the extents of the connected stations
    int nconnect = get_connection_count( from );
    x2 = xm = x1; y2 = ym = y1;
    while( nconnect-- )
    {
        int cto = get_connected_station( from, nconnect, 0 );
        if( !cto ) continue;
        if( ! connection_observation_count( from, nconnect ) ) continue;
        double x, y;
        get_station_coordinates( cto, &x, &y );
        if( x < x1 ) x1 = x; else if( x > x2 ) x2 = x;
        if( y < y1 ) y1 = y; else if( y > y2 ) y2 = y;
    }

    // If the range is not null then adjust the range to
    // put the selected station in the middle, and zoom to it

    if( x2 > x1 || y2 > y1 )
    {
        x2 = x2 - xm; x1 = xm - x1; if( x1 > x2 ) x2 = x1;
        x2 *= 1.1;
        x1 = xm - x2; x2 = xm + x2;
        y2 = y2 - ym; y1 = ym - y1; if( y1 > y2 ) y2 = y1;
        y2 *= 1.1;
        y1 = ym - y2; y2 = ym + y2;
        GetScale().ZoomTo( MapRect( x1, y1, x2, y2 ));
    }

    // Otherwise zoom to extents centred on the station and
    // calculated based on the number and extents of the stations
    // in the job ...
    else
    {
        double emin, nmin, emax, nmax;
        get_station_range( &emin, &nmin, &emax, &nmax );
        int nstns = used_station_count();
        double stnarea = (emax-emin)*(nmax-nmin);
        if( nstns ) stnarea /= nstns;
        y2 = sqrt( stnarea ) * 0.7;
        if( y2 <= 0.0 ) y2 = 1000.0;
        GetScale().ZoomTo( MapRect( xm - y2, ym - y2, xm + y2, ym + y2 ) );
    }
}

int SnapplotMapView::FindStation( const MapPoint &pt )
{
    double tolerance = pickTolerance * GetScale().GetScale();
    return nearest_station( pt.x, pt.y, tolerance );
}

void SnapplotMapView::ShowInfo( const MapPoint &pt, bool zoom )
{
    wxMapScale &mscale = GetScale();
    if( ! mscale.IsValid() ) return;


    int istn = FindStation( pt );
    if( istn )
    {
        /*
        if( lockedStn == istn ) curLockedStn = 0;
        if( !lockedStn && istn == hltStn1 && ! hltStn2 ) curLockedStn = istn;
        if( lockedStn && istn == hltStn2 ) curLockedStn = istn;
        if( curLockedStn && istn != curLockedStn )
        {
          ccMessage::PostMessage( ccMessage(msgWriteConnection,lockedStn,istn));

        }
        else
        {
          ccMessage::PostMessage( ccMessage(msgWriteStation, istn ));

        }
        */
        if( locatorFrom == istn )
        {
            SetLocatorLocked( ! locatorLocked );
            SendShowStationEvent( istn, zoom );
        }
        else if( locatorLocked && locatorFrom != 0 )
        {
            SetLocator( locatorFrom, istn );
            SendShowLineEvent( locatorFrom, istn, zoom );
        }
        else
        {
            SetLocatorLocked( false );
            SetLocator( istn, 0 );
            SendShowStationEvent( istn, zoom );
        }
    }
    else if( weakLock || ! locatorLocked )
    {
        int from ,to;
        double tolerance = pickTolerance * mscale.GetScale();
        if( nearest_connection( pt.x, pt.y, tolerance, &from, &to ) )
        {
            SetLocatorLocked( false );
            SetLocator( from, to );
            SendShowLineEvent( from, to, zoom );
        }

        else
        {
            SetLocatorLocked( false );
            SetLocator( 0, 0 );
            SendShowTitleEvent();
        }

    }
}

void SnapplotMapView::SendShowStationEvent( int istn, bool zoom )
{
    wxSnapplotEvent event( wxEVT_SHOW_DETAILVIEW, GetId() );
    event.SetShowStation( istn );
    event.SetEventObject(this);
    AddPendingEvent( event );
    if( zoom ) ZoomToStation( istn );
}

void SnapplotMapView::SendShowLineEvent( int from, int to, bool zoom )
{
    wxSnapplotEvent event( wxEVT_SHOW_DETAILVIEW, GetId() );
    event.SetShowLine( from, to );
    event.SetEventObject(this);
    AddPendingEvent( event );
    if( zoom ) ZoomToLine( from, to );
}

void SnapplotMapView::SendShowTitleEvent()
{
    wxSnapplotEvent event( wxEVT_SHOW_DETAILVIEW, GetId() );
    event.SetShowTitle();
    event.SetEventObject(this);
    AddPendingEvent( event );
}

void SnapplotMapView::LocateLine( int from, int to )
{
    SetLocatorLocked( false );
    SetLocator( from, to );
}

void SnapplotMapView::LocateStation( int istn )
{
    SetLocatorLocked( false );
    SetLocator( istn, 0 );
}

void SnapplotMapView::SetLocator( int from, int to )
{
    if( from == locatorFrom && to == locatorTo ) return;
    ClearLocator();
    locatorFrom = from;
    locatorTo = to;
    PaintLocator();
}

void SnapplotMapView::SetLocatorLocked( bool locked )
{
    if( locatorFrom && (locked != locatorLocked ))
    {
        ClearLocator();
        locatorLocked = locked;
        PaintLocator();
    }
}

void SnapplotMapView::PaintLocator()
{
    wxClientDC dc(this);
    PaintLocator( dc );
}


void SnapplotMapView::ClearLocator()
{
    // Paint uses XOR pen, so paint again clears...
    PaintLocator();
}

// Using macro to shift circles a little as otherwise don't line up properly!

#define DC_CIRCLE( dc, pt, radius ) dc.DrawEllipse( (pt).x-(radius), (pt).y-(radius), (radius)*2+1, (radius)*2+1 )

void SnapplotMapView::PaintLocator( wxDC &dc )
{
    if( locatorFrom == 0 ) return;
    wxRasterOperationMode logFunction = dc.GetLogicalFunction();
    dc.SetLogicalFunction( wxXOR );

    wxColour locatorCol = GetSymbology()->LayerColour( get_pen( SELECTED_PEN ) );
    wxColour backCol = GetSymbology()->LayerColour( get_pen( BACKGROUND_PEN ) );
    locatorCol = wxColour (
                     locatorCol.Red() ^ backCol.Red(),
                     locatorCol.Green() ^ backCol.Green(),
                     locatorCol.Blue() ^ backCol.Blue());

    if( ! locatorTo )
    {
        if( station_in_view(locatorFrom) )
        {
            dc.SetPen( wxPen( locatorCol, 1, wxTRANSPARENT ));
            dc.SetBrush( wxBrush( locatorCol ));
            double e, n;
            get_station_coordinates( locatorFrom, &e, &n );
            wxPoint ptFrom;
            GetScale().WorldToPlot( MapPoint(e,n),ptFrom );
            DC_CIRCLE( dc, ptFrom, locatorInnerWidth );
            DC_CIRCLE( dc, ptFrom, locatorLocked ? locatorLockedWidth : locatorOuterWidth );
            dc.SetPen( wxNullPen );
            dc.SetBrush( wxNullBrush );
        }
    }
    else
    {
        if( station_in_view( locatorFrom ) || station_in_view(locatorTo) )
        {
            wxPen innerPen(locatorCol, locatorInnerWidth*2 );
            wxPen outerPen(locatorCol, locatorOuterWidth*2 );
            wxMapScale scale = GetScale();
            double e, n;
            wxPoint ptStart;
            wxPoint ptEnd;
            bool start;
            get_station_coordinates( locatorFrom, &e, &n );
            scale.AddPoint( MapPoint(e,n), true );
            bool gotFromPoint = scale.GetPoint( ptStart, start );
            get_station_coordinates( locatorTo, &e, &n );
            scale.AddPoint( MapPoint(e,n), false );

            if( ( gotFromPoint || scale.GetPoint( ptStart, start )) && scale.GetPoint( ptEnd, start ) )
            {
                dc.SetPen( innerPen );
                dc.DrawLine( ptStart, ptEnd );
                dc.SetPen( wxNullPen );
                dc.SetPen( outerPen );
                dc.DrawLine( ptStart, ptEnd );
                dc.SetPen( wxNullPen );
            }
            // gotFromPoint means that the from point is in the screen
            if( locatorLocked && gotFromPoint )
            {
                dc.SetPen( wxPen(locatorCol, 1, wxTRANSPARENT) );
                dc.SetBrush( wxBrush(locatorCol));
                DC_CIRCLE( dc, ptStart, locatorOuterWidth );
                DC_CIRCLE( dc, ptStart, locatorLockedWidth );
                dc.SetBrush( wxNullBrush );
                dc.SetPen( wxNullPen );
            }
            dc.SetPen( wxNullPen );
        }
    }

    dc.SetPen( wxNullPen );
    dc.SetLogicalFunction( logFunction );
}

void SnapplotMapView::OnMapWindowPosition( wxMapWindowEvent & WXUNUSED(event) )
{
    positionChanged = true;
}

void SnapplotMapView::OnIdle( wxIdleEvent & WXUNUSED(event)  )
{

    // If we get some idle time, then update the coordinates display

    wxMapScale &mscale = GetScale();
    if( ! mscale.IsValid() ) return;
    if( ! positionChanged ) return;
    positionChanged = false;

    char buf[128];
    MapPoint pt = GetCursorPosition();
    format_plot_coords( pt.x, pt.y, buf );

    coordString.Empty();
    coordString.Append(buf);

    double tolerance = pickTolerance * mscale.GetScale();
    int istn = nearest_station( pt.x, pt.y, tolerance );

    if( istn )
    {
        coordString.Append("\r\n");
        coordString.Append(stnptr(istn)->Code);
        coordString.Append("\r\n");
        coordString.Append(stnptr(istn)->Name);
        coordString.Replace("\t"," ");
    }

    wxCommandEvent crdevent( WX_MAPVIEW_DISPLAY_COORDS, GetId() );
    crdevent.SetString( coordString );
    AddPendingEvent( crdevent );
}

void SnapplotMapView::OnPaint( wxPaintEvent & WXUNUSED(event) )
{
    //#if wxUSE_GRAPHICS_CONTEXT
    //wxPaintDC pdc(this);
    //wxGCDC dc( pdc ) ;
    //#else
    //wxPaintDC dc(this);
    //#endif
    wxPaintDC dc(this);
    PaintMap(dc);
    PaintLocator(dc);
}

// Routine generates a bitmap image that is a copy of contents of the screen map,
// by replotting the map onto the image ..

wxBitmap SnapplotMapView::CreateImage()
{
    wxSize mapSize = GetClientSize();
    wxBitmap bmp( mapSize.GetWidth(), mapSize.GetHeight() );
    wxMemoryDC bmpDC( bmp );
    wxMapScale scale( GetScale() );
    scale.SetWindowExtents( wxRect( wxPoint(0,0), mapSize ) );
    wxMapDrawer drawer( bmpDC, scale, *GetSymbology() );
    GetMap()->DrawMap( drawer );
    bmpDC.SetPen( wxPen( *wxBLACK ) );
    bmpDC.SetBrush( wxBrush( *wxBLACK, wxTRANSPARENT ) );
    bmpDC.DrawRectangle( 0, 0, mapSize.GetWidth(), mapSize.GetHeight() );
    return bmp;
}
