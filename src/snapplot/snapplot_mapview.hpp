#ifndef SNAPPLOT_MAPVIEW_HPP
#define SNAPPLOT_MAPVIEW_HPP

#include "snapplot_event.hpp"
#include "snapplot_dragger.hpp"

#include "wxmapwindow.hpp"

DECLARE_EVENT_TYPE( WX_MAPVIEW_DISPLAY_COORDS, -1 )

class SnapplotMapView : public wxMapWindow
{
public:

    SnapplotMapView( wxWindow *parent );
    ~SnapplotMapView();
    void ZoomToStation( int istn );
    void ZoomToLine( int from, int to );
    void LocateStation( int istn );
    void LocateLine( int from, int to );
    wxBitmap CreateImage();
    void SetWeakLock( bool newWeakLock ) { weakLock = newWeakLock; }
    bool WeakLock() { return weakLock; }

private:
    void OnIdleEvent( wxIdleEvent &event );
    void TestOnShowDetails( wxSnapplotEvent &event );
    void OnMapWindowPosition( wxMapWindowEvent &event );
    void OnClickEvent( wxMapWindowEvent &event );
    void OnIdle( wxIdleEvent &event  );
    void OnPaint( wxPaintEvent &event );
    void ShowInfo( const MapPoint &pt, bool zoom );

    void SendShowStationEvent( int istn, bool zoom );
    void SendShowLineEvent( int from, int to, bool zoom );
    void SendShowTitleEvent();

    void SetLocator( int from, int to );
    void SetLocatorLocked( bool locked );
    void PaintLocator();
    void PaintLocator( wxDC &dc );
    void ClearLocator();

    int FindStation( const MapPoint &pt );

    int pickTolerance;
    bool positionChanged;
    int locatorFrom;
    int locatorTo;
    bool locatorLocked; // If the from station is locked ...
    bool weakLock; // If true, then clicking other than a station will release a lock
    wxString coordString;

    // Object used to manage dragging on the map
    SnapplotMapDragger mapDragger;

    DECLARE_EVENT_TABLE();
};


#endif
