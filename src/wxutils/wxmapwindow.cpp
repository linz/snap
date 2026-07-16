#include "snapconfig.h"
// wxmapwindow: Code related to the drawing a map, scaling etc.

#include "wxmapwindow.hpp"

DEFINE_EVENT_TYPE( WX_MAPWINDOW_POSITION );
DEFINE_EVENT_TYPE( WX_MAPWINDOW_CLICKED );
DEFINE_EVENT_TYPE( WX_MAPWINDOW_REPAINTED);
DEFINE_EVENT_TYPE( WX_MAPWINDOW_REDRAWMAP);


// wxMapWindowClicked definition

wxMapWindowEvent::wxMapWindowEvent( wxEventType eventType, int id, const wxMouseEvent &me, MapPoint pt ) :
    wxEvent( id, eventType ),
    mouseEvent(me),
    pt(pt)
{
    SetEventType( eventType );
    SetId(id);
}

wxMapWindowEvent::wxMapWindowEvent( const wxMapWindowEvent &event ) :
    wxEvent( event ),
    mouseEvent( event.mouseEvent ),
    pt(event.pt)
{
}

// wxMapDragger definition

wxMapDragger::wxMapDragger()
{
    mapWindow = 0;
    dragging = false;
    canDrag = false;
    moveOriginOnShift = false;
    minDragLength = 10;
    dragMode = rbmRect;
    dragCancelButton = wxMOUSE_BTN_NONE;
    overlayResetPending = false;
}

wxMapDragger::~wxMapDragger()
{
    if( mapWindow ) mapWindow->SetDragger(0);
}

void wxMapDragger::AttachToMapWindow( wxMapWindow *newWindow )
{
    if( mapWindow != newWindow )
    {
        if( mapWindow ) mapWindow->SetDragger(0);
        mapWindow = newWindow;
    }
}

void wxMapDragger::SetMinDragLength( int length )
{
    minDragLength = length;
}

void wxMapDragger::SetRBDragMode( RBDragMode newMode )
{
    dragMode = newMode;
}

void wxMapDragger::SetMoveOriginOnShift( bool newState )
{
    moveOriginOnShift = newState;
}

bool wxMapDragger::InitDrag( const wxMouseEvent & WXUNUSED(event) )
{
    return true;
}

void wxMapDragger::EndDrag()
{
}

void wxMapDragger::SetDragCursor()
{
    SetMapCursor( wxCursor( wxCURSOR_CROSS ) );
}

void wxMapDragger::SetMapCursor( const wxCursor & cursor )
{
    mapWindow->SetCursor( cursor );
}

void wxMapDragger::ProcessMouseEvent( wxMouseEvent &event )
{
    if( ! mapWindow ) return;

    if( ! dragging )
    {
        // Record the start of the event ...

        if( event.ButtonDown() )
        {
            dragStartPoint = event.GetPosition();
            canDrag = true;
        }

        // If it is a button up event, then record this as a non-drag mouse event

        if( event.ButtonUp() )
        {
            if( event.GetButton() == dragCancelButton ) {
                dragCancelButton = wxMOUSE_BTN_NONE;
            }
            else if( mapWindow ) {
                mapWindow->ForwardMouseEvent( WX_MAPWINDOW_CLICKED, event  );
            }
        }

        // If this is a dragging event, then confirm that this is a valid
        // state for starting dragging.

        if( event.Dragging() )
        {
            wxPoint dragOffset = event.GetPosition() - dragStartPoint;
            if( abs(dragOffset.x) + abs(dragOffset.y) >= minDragLength )
            {
                if( canDrag && InitDrag( event ) )
                {
                    dragEndPoint = event.GetPosition();
                    savedCursor = mapWindow->GetCursor();
                    SetDragCursor();
                    {
                        wxClientDC dc( mapWindow );
                        wxDCOverlay dcOverlay( overlay, &dc );
                        dcOverlay.Clear();
                        DrawDragger( dc );
                    }
                    mapWindow->CaptureMouse();
                    dragging = true;
                }
                else
                {
                    canDrag = false;
                }
            }
        }
    }
    else
    {
        // Button down events effectively cancel dragging, so can hit
        // second mouse button to cancel operation.

        // TODO Need to handle the corresponding mouse up event so that it is not treated as a click
        // event ... may need to make the whole thing smarter in terms of mouse button states and
        // events.  May also need to include timing, as mouse up event long after mouse down isn't
        // really of interest, as down may have occurred in a different window when not capturing mouse.

        if( event.ButtonUp() || event.ButtonDown() )
        {
            mapWindow->ReleaseMouse();
            dragging = false;
            canDrag = false;
            mapWindow->SetCursor( savedCursor );
            if( event.ButtonUp() ) {
                dragCancelButton = wxMOUSE_BTN_NONE;
                EndDrag();
                // EndDrag() may have changed the view - wait for the next
                // real repaint (see CheckPendingOverlayReset) before hiding
                // the drag preview, rather than reveal stale content now.
                overlayResetPending = true;
            }
            else {
                dragCancelButton = event.GetButton();
                // Nothing changed - safe to hide the drag preview right away.
                overlay.Reset();
            }
        }
        else
        {
            wxPoint newEndPoint = event.GetPosition();

            if( moveOriginOnShift && event.ShiftDown() )
            {
                int dx = newEndPoint.x - dragEndPoint.x;
                int dy = newEndPoint.y - dragEndPoint.y;

                dragStartPoint = wxPoint( dragStartPoint.x + dx, dragStartPoint.y + dy );
            }
            dragEndPoint = newEndPoint;

            wxClientDC dc( mapWindow );
            wxDCOverlay dcOverlay( overlay, &dc );
            dcOverlay.Clear();
            DrawDragger( dc );
        }
    }
}

void wxMapDragger::ProcessMouseCaptureLostEvent(wxMouseCaptureLostEvent & WXUNUSED(event))
{
    if( ! mapWindow ) return;

    // No EndDrag() here - the view never changed, so unlike the end/cancel
    // path in ProcessMouseEvent there's no pending repaint to wait for.
    overlay.Reset();

    dragging = false;
    mapWindow->SetCursor( savedCursor );
}

void wxMapDragger::CheckPendingOverlayReset()
{
    if( overlayResetPending )
    {
        overlayResetPending = false;
        // The real repaint has been issued, but may not be actually
        // presented on screen yet - start a settle timer instead of
        // resetting immediately, giving the compositor a margin to catch up.
        // A compositor frame is typically ~16ms at 60Hz, so this should be
        // several frames of margin - designed to reliably eliminate any
        // repaint gap to the main window while still feeling responsive.
        mapWindow->StartOverlayResetTimer( 50 );
    }
}

void wxMapDragger::ResetOverlayNow()
{
    overlay.Reset();
}

void wxMapDragger::DrawDragger( wxDC &dc )
{
    if( dragMode == rbmRect )
    {
        if( dragStartPoint == dragEndPoint )
        {
            dc.DrawLine( dragStartPoint.x-10, dragStartPoint.y, dragStartPoint.x+11, dragStartPoint.y );
            dc.DrawLine( dragStartPoint.x, dragStartPoint.y-10, dragStartPoint.x, dragStartPoint.y+11 );
        }
        else
        {
            dc.DrawLine( dragStartPoint.x, dragStartPoint.y, dragStartPoint.x, dragEndPoint.y );
            dc.DrawLine( dragStartPoint.x, dragEndPoint.y, dragEndPoint.x, dragEndPoint.y );
            if( dragEndPoint.x != dragStartPoint.x && dragEndPoint.y != dragStartPoint.y )
            {
                dc.DrawLine( dragEndPoint.x, dragEndPoint.y, dragEndPoint.x, dragStartPoint.y );
                dc.DrawLine( dragEndPoint.x, dragStartPoint.y, dragStartPoint.x, dragStartPoint.y );
            }
        }
    }
}

void wxMapDragger::PaintMapOnto( wxDC &dc )
{
    dc.SetBackground( wxBrush( mapWindow->GetBackgroundColour() ) );
    dc.Clear();
    mapWindow->PaintMap( dc );
}

wxBitmap wxMapDragger::CaptureMapBitmap()
{
    wxSize size = mapWindow->GetClientSize();
    wxBitmap bitmap( size.GetWidth(), size.GetHeight() );
    wxMemoryDC memDC( bitmap );
    PaintMapOnto( memDC );
    return bitmap;
}


// Map scale dragger class

wxMapScaleDragger::wxMapScaleDragger()
{
    zoomMode = zdmZoom;
    autoZoomPanMode = false;
}

bool wxMapScaleDragger::InitDrag( const wxMouseEvent &event )
{
    if( zoomMode == zdmNone ) return false;
    if( ! mapWindow->GetScale().IsValid() ) return false;

    if( ! event.LeftIsDown() ) return false;

    if( autoZoomPanMode )
    {
        zoomMode = event.ShiftDown() ? zdmPan : zdmZoom;
    }

    if( zoomMode == zdmZoom )
    {
        SetMoveOriginOnShift( true );
        SetRBDragMode( rbmRect );
    }
    else
    {
        SetMoveOriginOnShift( false );
        panBitmap = CaptureMapBitmap();
    }
    return true;
}

void wxMapScaleDragger::DrawDragger( wxDC &dc )
{
    if( zoomMode == zdmPan )
    {
        // The shifted bitmap doesn't cover the whole window once dragged far
        // enough - fill with the background colour first so the revealed
        // edge shows plain background rather than the leftover, unshifted
        // view.
        dc.SetBackground( wxBrush( mapWindow->GetBackgroundColour() ) );
        dc.Clear();
        if( panBitmap.IsOk() )
        {
            wxPoint offset = dragEndPoint - dragStartPoint;
            dc.DrawBitmap( panBitmap, offset.x, offset.y );
        }
    }
    else
    {
        wxMapDragger::DrawDragger( dc );
    }
}

void wxMapScaleDragger::SetDragCursor()
{
    if( zoomMode == zdmZoom )
    {
        SetMapCursor( wxCursor( wxCURSOR_MAGNIFIER ) );
    }
    else if( zoomMode == zdmPan )
    {
        SetMapCursor( wxCursor( wxCURSOR_HAND ) );
    }
    else
    {
        wxMapDragger::SetDragCursor();
    }
}

void wxMapScaleDragger::EndDrag()
{
    if( zoomMode == zdmZoom || zoomMode == zdmPan )
    {
        wxMapScale &scale = mapWindow->GetScale();
        MapPoint ptStart;
        MapPoint ptEnd;
        scale.PlotToWorld( dragStartPoint, ptStart );
        scale.PlotToWorld( dragEndPoint, ptEnd );
        MapRect newWindow;
        if( zoomMode == zdmZoom )
        {
            if( ptStart.x > ptEnd.x ) { double tmp = ptStart.x; ptStart.x = ptEnd.x; ptEnd.x = tmp; }
            if( ptStart.y > ptEnd.y ) { double tmp = ptStart.y; ptStart.y = ptEnd.y; ptEnd.y = tmp; }
            newWindow = MapRect( ptStart, ptEnd );
        }
        else
        {
            double dx = ptStart.x - ptEnd.x;
            double dy = ptStart.y - ptEnd.y;
            newWindow = scale.GetWorldExtents().Shift(dx,dy);
        }
        scale.ZoomTo( newWindow );
    }
    panBitmap = wxBitmap();
}

void wxMapScaleDragger::SetZoomDragMode( ZoomDragMode newMode )
{
    zoomMode = newMode;
}

void wxMapScaleDragger::SetAutoZoomPanMode( bool newMode )
{
    autoZoomPanMode = newMode;
}

// Map window class


// wxMapWindow class definition

wxMapWindow::wxMapWindow()
{
    SetupMapWindow();
}

wxMapWindow::wxMapWindow ( wxWindow *parent, wxWindowID id ):
    wxWindow( parent, id, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER )
{
    SetupMapWindow();
}

void wxMapWindow::SetupMapWindow()
{
    scale.SetNotifyWindowId( GetId() );
    SetBackgroundColour( *wxWHITE );
    SetCursor( wxCursor(wxCURSOR_CROSS) );
    dragger = 0;
    map = 0;
    symbology = 0;
    sendPositionEvent = false;
    overlayResetTimer.SetOwner( this );
}

void wxMapWindow::StartOverlayResetTimer( int delayMilliseconds )
{
    overlayResetTimer.StartOnce( delayMilliseconds );
}

void wxMapWindow::OnOverlayResetTimer( wxTimerEvent & WXUNUSED(event) )
{
    if( dragger ) {
        dragger->ResetOverlayNow();
    }
}


wxMapWindow::~wxMapWindow()
{
    SetDragger( 0 );
}

IMPLEMENT_DYNAMIC_CLASS( wxMapWindow, wxWindow );

BEGIN_EVENT_TABLE(wxMapWindow, wxWindow)
    EVT_PAINT( wxMapWindow::OnPaint )
    EVT_MOUSE_EVENTS( wxMapWindow::OnMouseEvent )
    EVT_MOUSE_CAPTURE_LOST( wxMapWindow::OnMouseCaptureLost )
    EVT_SIZE( wxMapWindow::OnSizeEvent )
    EVT_SIMPLE_EVENT( WX_MAPWINDOW_SCALE_CHANGED, wxMapWindow::OnScaleChangeEvent )
    EVT_SIMPLE_EVENT( WX_MAPWINDOW_REDRAWMAP, wxMapWindow::OnRedrawMap )
    EVT_TIMER( wxID_ANY, wxMapWindow::OnOverlayResetTimer )
END_EVENT_TABLE()

void wxMapWindow::SetDragger( wxMapDragger *newDragger )
{
    wxMapDragger *oldDragger = dragger;
    dragger = newDragger;
    if( oldDragger ) oldDragger->AttachToMapWindow( 0 );
    if( dragger ) dragger->AttachToMapWindow( this );
}

void wxMapWindow::SetSendPositionEvent( bool sendPosition )
{
    sendPositionEvent = sendPosition;
}

void wxMapWindow::SetSymbology( Symbology *symbology )
{
    this->symbology = symbology;
    Refresh();
}

void wxMapWindow::SetMap( wxMap *map )
{
    this->map = map;
    if( map )
    {
        MapRect mapExtents;
        map->GetMapExtents( mapExtents );
        scale.SetMapExtents( mapExtents );
    }
    Refresh();
}

void wxMapWindow::OnMouseEvent( wxMouseEvent &event )
{
    if( scale.IsValid() )
    {
        wxPoint mousePoint( event.GetX(), event.GetY() );
        scale.PlotToWorld( mousePoint, cursorPosition );
        if( sendPositionEvent ) ForwardMouseEvent( WX_MAPWINDOW_POSITION, event );
        if( dragger )
        {
            dragger->ProcessMouseEvent( event );
        }
        else if( event.ButtonUp() )
        {
            ForwardMouseEvent( WX_MAPWINDOW_CLICKED, event );
        }
    }
}

void wxMapWindow::OnMouseCaptureLost(wxMouseCaptureLostEvent &event)
{
    if( dragger ) dragger->ProcessMouseCaptureLostEvent( event );
}

void wxMapWindow::ForwardMouseEvent( wxEventType type, const wxMouseEvent &event )
{
    wxMapWindowEvent mwevent( type, GetId(), event, cursorPosition );
    mwevent.SetEventObject( this );
    AddPendingEvent( mwevent );
}

void wxMapWindow::OnSizeEvent( wxSizeEvent & WXUNUSED(event) )
{
    GetScale().SetWindowExtents( GetClientRect() );
}

void wxMapWindow::OnScaleChangeEvent( wxSimpleEvent & WXUNUSED(event) )
{
    RefreshCursorPosition();
    DoRedrawMap();
}

void wxMapWindow::RefreshCursorPosition()
{
    // A zoom/pan changes the scale without the mouse moving, so the position
    // readout (only ever recomputed from a real mouse event) would otherwise
    // keep showing the coordinate under the old scale until the next move.
    if( ! scale.IsValid() ) return;

    wxPoint mousePoint = ScreenToClient( wxGetMousePosition() );
    scale.PlotToWorld( mousePoint, cursorPosition );
    if( sendPositionEvent )
    {
        wxMouseEvent mouseEvent( wxEVT_MOTION );
        mouseEvent.SetPosition( mousePoint );
        ForwardMouseEvent( WX_MAPWINDOW_POSITION, mouseEvent );
    }
}

void wxMapWindow::OnRedrawMap( wxSimpleEvent & WXUNUSED(event) )
{
    DoRedrawMap();
}

void wxMapWindow::DoRedrawMap()
{
    // For the moment just refresh.  Could implement differently if
    // buffering the map...
    Refresh();
}

void wxMapWindow::RedrawMap()
{
    wxSimpleEvent event(WX_MAPWINDOW_REDRAWMAP);
    AddPendingEvent( event );
}

void wxMapWindow::PaintMap(  wxDC &dc )
{
    if( map && symbology )
    {
        wxCursor saved = GetCursor();
        SetCursor( *wxHOURGLASS_CURSOR );
        wxMapDrawer drawer( dc, scale, *symbology );
        map->DrawMap( drawer );
        drawer.FlushMap();
        SetCursor( saved );
    }
    if( dragger ) {
        dragger->CheckPendingOverlayReset();
    }
}

void wxMapWindow::OnPaint( wxPaintEvent & WXUNUSED(event) )
{
    #if wxUSE_GRAPHICS_CONTEXT
    wxPaintDC pdc(this);
    wxGCDC dc( pdc ) ;
    #else
    wxPaintDC dc(this);
    #endif

    PaintMap(dc);
}



