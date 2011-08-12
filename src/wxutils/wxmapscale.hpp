#ifndef WXMAPSCALE_HPP
#define WXMAPSCALE_HPP

// Basics map types

#include "wx_includes.hpp"
#include "mapdefs.hpp"


DECLARE_EVENT_TYPE(WX_MAPWINDOW_SCALE_CHANGED, -1)

// Class for managing a stack of zoom scales.

class ZoomStack
{
public:
    ZoomStack( int size = 0 );
    ~ZoomStack();
    void SetStackSize( int nStack );
    void ClearStack();
    void PushZoomRect( const MapRect &rect );
    bool PopZoomRect( MapRect &rect );

private:
    MapRect *zoomStack;
    int stackSize;
    int stackTop;
    int stackBottom;
};

//---------------------------------------------------------------------------
// wxMapScale: Manages scaling between window and world coordinates (in a
// cartesian system) and also trimming lines to the plot rectangle

class wxMapScale
{
public:
    wxMapScale( int windowId = wxID_ANY );

    // Copy constructor and = operator.  These simply copy the extents into the
    // modified scale.  They do not set the zoom stack, or copy any other information.

    wxMapScale( const wxMapScale &scale );
    const wxMapScale& operator = (const wxMapScale & scale);

    // Setup extents..  Aspect ratio is xpixel_size/ypixel_size

    // Slightly confusing use of "World" and "Map".  World refers to the World Coordinates displayed in the
    // window, as compared with the Plot coordinates of the window, ie it is the requested extent of the
    // displayed data.  Map refers to the maximum extents of the data (ie zoom all)!!

    // Note: ZoomTo and SetWorldExtents perform virtually the same function, but SetWorldExtents doesn't
    // push the current extents onto the zoom stack.

    void SetWindowExtents( const wxRect &extents, double aspect = 1.0 );
    void SetWorldExtents( const MapRect &extents );
    void SetMapExtents( const MapRect &extents );
    wxRect GetWindowExtents() const { return plotRect; }
    MapRect GetWorldExtents() const { return mapWindow; }
    MapRect GetMapExtents() const { return mapSize; }

    // GetScale returns the scale factor by which map coordinates (double) are multiplied to
    // get to plot coordinates (integer).  This is an average of the values used in the x and y
    // directions.

    double GetScale();

    void ZoomAll();
    void ZoomTo( const MapRect &extents );
    void ZoomOut() { ZoomOut( zoomOutFactor ); }
    void ZoomOut( double factor );
    void ZoomPrev();
    void Pan( double xShift, double yShift );

    void SetZoomOutFactor( double newFactor ) { zoomOutFactor = newFactor; }

    void SetZoomStackSize( int nStack );

    // Coordinate conversion routines
    void WorldToPlot( const MapPoint &data, wxPoint &plot ) const;
    void PlotToWorld( const wxPoint &plot, MapPoint &data ) const;

    // Line trimming routines
    bool TrimLine( const MapPoint &mpfrom, const MapPoint &mpTo,
                   wxPoint &ptFrom, wxPoint &ptTo );
    bool AddPoint( const MapPoint &pt, bool start );
    bool GetPoint( wxPoint &pt, bool &start );

    //
    bool IsValid() { return scaleValid; }

    void SetNotifyWindowId( int id ) { windowId = id; }

private:
    void CalcScale();
    char PointFlags( const MapPoint &pt );
    char ShiftIntoRange( MapPoint &p0, char flag0, const MapPoint p1, char flag1 );
    bool NextPoint( wxPoint &pt, bool &start );
    void NotifyWindow();

    MapRect mapExtents;    // Requested extents
    MapRect mapWindow;  // Actual window plotted
    MapRect mapSize;     // Total size of the map
    wxRect plotRect;      // Window rectangle in window coords
    double aspectRatio;

    bool scaleValid;   // Scale cannot be used
    double wxorg, wdxscale;
    double wyorg, wdyscale;
    int    dxorg, dyorg;

    double zoomOutFactor;

    MapPoint pt0, pt1, ptnext;
    char flagnext;
    bool s0, s1;
    char npt;

    int windowId;

    ZoomStack zoomStack;
};

inline
bool wxMapScale::GetPoint( wxPoint &pt, bool &start )
{
    if( !npt ) return false;
    return NextPoint( pt, start );
}

#endif

