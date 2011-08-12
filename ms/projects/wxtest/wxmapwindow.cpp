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

bool wxMapDragger::InitDrag( const wxMouseEvent &event )
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
		wxLogMessage("Starting dragging");

		// Record the start of the event ...

		if( event.ButtonDown() )
		{
			dragStartPoint = event.GetPosition();
			canDrag = true;
		wxLogMessage("Event is button down");
		}

		// If it is a button up event, then record this as a non-drag mouse event

		if( event.ButtonUp() )
		{
			if( mapWindow ) 
			{
				mapWindow->ForwardMouseEvent( WX_MAPWINDOW_CLICKED, event  );
			}
		wxLogMessage("Event is button up");
		}

		// If this is a dragging event, then confirm that this is a valid
		// state for starting dragging.

		if( event.Dragging() )
		{
			wxPoint dragOffset = event.GetPosition() - dragStartPoint;
			wxLogMessage("Event is drag %d %d",(int)(dragOffset.x),(int)(dragOffset.y));
			if( abs(dragOffset.x) + abs(dragOffset.y) >= minDragLength )
			{
				wxLogMessage("Can drag: %s",canDrag ? "true" : "false");
				if( canDrag && InitDrag( event ) )
				{
					wxLogMessage("Ok to drag");
  					dragEndPoint = event.GetPosition();
					wxClientDC dc( mapWindow );
					dc.SetLogicalFunction( wxINVERT );
					savedCursor = mapWindow->GetCursor();
					SetDragCursor();
					DrawDragger( dc );
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
		wxLogMessage("Continuing dragging");
		wxClientDC dc( mapWindow );
		dc.SetLogicalFunction( wxINVERT );
		DrawDragger( dc );

		// Button down events effectively cancel dragging, so can hit 
		// second mouse button to cancel operation.

		// TODO Need to handle the corresponding mouse up event so that it is not treated as a click
		// event ... may need to make the whole thing smarter in terms of mouse button states and 
		// events.  May also need to include timing, as mouse up event long after mouse down isn't 
		// really of interest, as down may have occured in a different window when not capturing mouse.

		if( event.ButtonUp() || event.ButtonDown() ) 
		{
			mapWindow->ReleaseMouse();
			dragging = false;
			mapWindow->SetCursor( savedCursor );
			if( event.ButtonUp() ) EndDrag();
		}

		if( dragging ) 
		{
			wxPoint newEndPoint = event.GetPosition();

			if( moveOriginOnShift && event.ShiftDown() )
			{
				int dx = newEndPoint.x - dragEndPoint.x;
				int dy = newEndPoint.y - dragEndPoint.y;

				dragStartPoint = wxPoint( dragStartPoint.x + dx, dragStartPoint.y + dy );
			}
			dragEndPoint = newEndPoint;
			DrawDragger( dc );
		}
	}
}

void wxMapDragger::ProcessMouseCaptureLostEvent(wxMouseCaptureLostEvent &event)
{
	if( ! mapWindow ) return;

	wxClientDC dc( mapWindow );
	dc.SetLogicalFunction( wxINVERT );
	DrawDragger( dc );

	dragging = false;
	mapWindow->SetCursor( savedCursor );
}

void wxMapDragger::DrawDragger( wxDC &dc )
{
	if( dragMode == rbmLine ) 
	{
		dc.DrawLine( dragStartPoint, dragEndPoint );
	}
	else if( dragMode == rbmRect )
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


// Map scale dragger class

wxMapScaleDragger::wxMapScaleDragger()
{
	zoomMode = zdmZoom;
	autoZoomPanMode = false;
}

bool wxMapScaleDragger::InitDrag( const wxMouseEvent &event )
{
	wxLogMessage("In wxMapScaleDragger::InitDrag");
	wxLogMessage("Zoom mode = %d",(int) zoomMode );
	if( zoomMode == zdmNone ) return false;
	if( ! mapWindow->GetScale().IsValid() ) return false;
	wxLogMessage("Map scale valid");

	if( ! event.LeftIsDown() ) return false;

wxLogMessage("Button is down");
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
		SetRBDragMode( rbmLine );
	}
	return true;
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
			if( ptStart.x > ptEnd.x ){ double tmp = ptStart.x; ptStart.x = ptEnd.x; ptEnd.x = tmp; }
			if( ptStart.y > ptEnd.y ){ double tmp = ptStart.y; ptStart.y = ptEnd.y; ptEnd.y = tmp; }
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

void wxMapWindow::OnSizeEvent( wxSizeEvent &event )
{
	GetScale().SetWindowExtents( GetClientRect() );
}

void wxMapWindow::OnScaleChangeEvent( wxSimpleEvent &event )
{
	DoRedrawMap();
}

void wxMapWindow::OnRedrawMap( wxSimpleEvent &event )
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
}

void wxMapWindow::OnPaint( wxPaintEvent &event ) 
{
    #if wxUSE_GRAPHICS_CONTEXT
    wxPaintDC pdc(this);
    wxGCDC dc( pdc ) ;
    #else
    wxPaintDC dc(this);
    #endif
    PaintMap(dc);
}



