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

enum RBDragMode { rbmNone, rbmRect };
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

    // Fills dc with the background colour, then draws a fresh copy of the
    // map onto it - rather than reading back what's currently on screen
    // (e.g. via wxClientDC::Blit), which is unreliable on wxGTK, and isn't
    // meaningful at all under wx's native overlay backend (selected on
    // Wayland sessions), which draws to a separate transparent layer
    // instead of the real window.
    void PaintMapOnto( wxDC &dc );

    // As PaintMapOnto, but into an owned bitmap sized to the window.
    wxBitmap CaptureMapBitmap();

    wxMapWindow *mapWindow;
    wxPoint dragStartPoint;
    wxPoint dragEndPoint;

private:
    void AttachToMapWindow( wxMapWindow *newWindow );
    void ProcessMouseEvent( wxMouseEvent &event );
    void ProcessMouseCaptureLostEvent( wxMouseCaptureLostEvent &event );

    // Called by wxMapWindow::PaintMap() (a friend) right after it draws -
    // if a reset was requested, starts a settle timer rather than resetting
    // immediately, since the repaint may not be actually presented on
    // screen yet even though it's just been issued.
    void CheckPendingOverlayReset();

    // Called by wxMapWindow once the settle timer fires - actually hides
    // the drag preview.
    void ResetOverlayNow();

    bool dragging;
    bool canDrag;  // Used to avoid repeated checks after a rejected drag start
    bool moveOriginOnShift;
    bool overlayResetPending;

    // Button whose up event should be swallowed rather than forwarded as a
    // fresh click, because it only cancelled an in-progress drag
    // (wxMOUSE_BTN_NONE if no such swallow is pending).
    int dragCancelButton;

    int minDragLength;

    RBDragMode dragMode;

    wxCursor savedCursor;
    wxOverlay overlay;
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
protected:
    // protected, not private: a subclass may need to call this base
    // implementation directly (qualified, non-virtually) from within its
    // own override, which requires at least protected access.
    virtual void DrawDragger( wxDC &dc );
private:
    void ZoomTo( const MapRect &newRect );
    ZoomDragMode zoomMode;
    bool autoZoomPanMode;

    // Snapshot of the view taken when a pan drag starts, blitted at the
    // current drag offset each frame for a live-scrolling preview.
    wxBitmap panBitmap;
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
    void RefreshCursorPosition();
    void OnOverlayResetTimer( wxTimerEvent &event );

    // Starts a one-shot timer that calls dragger->ResetOverlayNow() after
    // delayMilliseconds - the repaint that made this necessary may have
    // been issued but not yet actually presented on screen, so this gives
    // it a margin to catch up before the drag preview is hidden.
    void StartOverlayResetTimer( int delayMilliseconds );

    wxMap *map;
    wxMapScale scale;
    Symbology *symbology;
    wxMapDragger *dragger;
    MapPoint cursorPosition;
    bool sendPositionEvent;
    wxTimer overlayResetTimer;

    DECLARE_DYNAMIC_CLASS( wxMapWindow );
    DECLARE_EVENT_TABLE();

};


#endif

