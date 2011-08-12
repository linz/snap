#ifndef WXMAPWINDOW_HPP
#define WXMAPWINDOW_HPP

#include "wx_includes.hpp"
#include "mapdefs.hpp"
#include "wxsimpleevent.hpp"
#include "wxmapscale.hpp"
#include "wxmap.hpp"

DECLARE_EVENT_TYPE( WX_MAPWINDOW_POSITION, -1)
DECLARE_EVENT_TYPE( WX_MAPWINDOW_CLICKED, -1)
DECLARE_EVENT_TYPE( WX_MAPWINDOW_REPAINTED, -1)
DECLARE_EVENT_TYPE( WX_MAPWINDOW_REDRAWMAP, -1)

class wxMapWindowEvent: public wxEvent
{
public:
    wxMapWindowEvent( wxEventType eventType, int id, const wxMouseEvent &me, MapPoint pt );
    wxMapWindowEvent( const wxMapWindowEvent &event );

    const MapPoint GetMapPoint() { return pt; }
    const wxMouseEvent &GetMouseEvent() { return mouseEvent; }

    // required for sending with wxPostEvent()
    virtual wxEvent* Clone() const { return new wxMapWindowEvent(*this); }


private:
    wxMouseEvent mouseEvent;
    MapPoint pt;
    DECLARE_NO_ASSIGN_CLASS( wxMapWindowEvent )
};

typedef void (wxEvtHandler::*wxMapWindowEventFunction)(wxMapWindowEvent&);

#define EVT_MAPWINDOW_EVENT( eventtype, fn) \
    DECLARE_EVENT_TABLE_ENTRY( eventtype, wxID_ANY, -1, \
    (wxObjectEventFunction) (wxEventFunction) \
    wxStaticCastEvent( wxMapWindowEventFunction, & fn ), (wxObject *) NULL ),

//---------------------------------------------------------------------------

// Modes for rubber band dragging

enum RBDragMode { rbmNone, rbmLine, rbmRect };
enum ZoomDragMode { zdmNone, zdmZoom, zdmPan };

// Options for rubber band dragging
enum
{
    rbLeftButtonCanDrag = 1,
    rbRightButtonCanDrag = 2,
    rbMiddleButtonCanDrag = 4,
    rbMoveOriginOnShift = 8,
    rbClearOnButtonDown = 16
};

class wxMapWindow;

class wxMapDragger
{
    friend class wxMapWindow;
public:
    wxMapDragger();
    ~wxMapDragger();

    virtual bool InitDrag( const wxMouseEvent &event );
    virtual void SetDragCursor();
    virtual void EndDrag();

    void SetMinDragLength( int length );
    void SetRBDragMode( RBDragMode newMode );
    void SetMoveOriginOnShift( bool newState );

protected:
    void SetMapCursor( const wxCursor &cursor );
    virtual void DrawDragger( wxDC &dc );

    wxMapWindow *mapWindow;
    wxPoint dragStartPoint;
    wxPoint dragEndPoint;

private:
    void AttachToMapWindow( wxMapWindow *newWindow );
    void ProcessMouseEvent( wxMouseEvent &event );
    void ProcessMouseCaptureLostEvent( wxMouseCaptureLostEvent &event );

    bool dragging;
    bool canDrag;  // Used to avoid repeated checks after a rejected drag start
    bool moveOriginOnShift;

    int minDragLength;

    RBDragMode dragMode;

    wxCursor savedCursor;
};

// Map scale dragger class

class wxMapScaleDragger : public wxMapDragger
{
public:
    wxMapScaleDragger();
    virtual bool InitDrag( const wxMouseEvent &event );
    virtual void SetDragCursor();
    virtual void EndDrag();
    void SetZoomDragMode( ZoomDragMode newMode );
    void SetAutoZoomPanMode( bool newMode = true );
private:
    void ZoomTo( const MapRect &newRect );
    ZoomDragMode zoomMode;
    bool autoZoomPanMode;
};

// Map window class

class wxMapWindow : public wxWindow
{
    friend wxMapDragger;
public:
    wxMapWindow();
    wxMapWindow( wxWindow *parent, wxWindowID id );
    ~wxMapWindow();

    wxMapScale &GetScale() { return scale; }
    Symbology *GetSymbology() { return symbology; }
    wxMap *GetMap() { return map; }
    const MapPoint &GetCursorPosition() const { return cursorPosition; }

    void SetSendPositionEvent( bool sendPosition = true );
    void SetDragger( wxMapDragger *dragger );
    void SetSymbology( Symbology *symbology );
    void SetMap( wxMap *map );

    void RedrawMap();

protected:
    void PaintMap( wxDC &dc );

private:
    void SetupMapWindow();
    void DoRedrawMap();
    void OnPaint( wxPaintEvent &event );
    void OnMouseEvent( wxMouseEvent &event );
    void OnMouseCaptureLost( wxMouseCaptureLostEvent &event );
    void OnSizeEvent( wxSizeEvent &event );
    void OnScaleChangeEvent( wxSimpleEvent &event );
    void OnRedrawMap( wxSimpleEvent &event );
    void ForwardMouseEvent(  wxEventType type, const wxMouseEvent &event );

    wxMap *map;
    wxMapScale scale;
    Symbology *symbology;
    wxMapDragger *dragger;
    MapPoint cursorPosition;
    bool sendPositionEvent;

    DECLARE_DYNAMIC_CLASS( wxMapWindow );
    DECLARE_EVENT_TABLE();

};


#endif

