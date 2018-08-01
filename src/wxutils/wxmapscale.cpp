#include "snapconfig.h"
// wxmapscale: Code related scaling from real world coordinates to plot coordinates

#include "wxmapscale.hpp"
#include "wxsimpleevent.hpp"
#include <math.h>

DEFINE_EVENT_TYPE(WX_MAPWINDOW_SCALE_CHANGED);

// ZoomStack: class for managing a stack of zoom rectangles

ZoomStack::ZoomStack( int size )
{
    zoomStack = 0;
    stackSize = 0;
    stackTop = stackBottom = 0;
    if( size > 0 )
    {
        SetStackSize( size );
    }
}

ZoomStack::~ZoomStack()
{
    if( zoomStack ) delete [] zoomStack;
}

void ZoomStack::SetStackSize( int nStack )
{
    if( zoomStack ) delete [] zoomStack;
    zoomStack = 0;
    if( nStack > 0 )
    {
        zoomStack = new MapRect[nStack+1];
    }
    stackSize = nStack;
    stackTop = 0;
    stackBottom = 0;
}

void ZoomStack::ClearStack()
{
    stackTop = stackBottom = 0;
}

void ZoomStack::PushZoomRect( const MapRect &rect )
{
    if( stackSize > 0 )
    {
        stackTop++;
        if( stackTop > stackSize ) stackTop = 0;
        if( stackTop == stackBottom ) stackBottom++;
        if( stackBottom > stackSize ) stackBottom = 0;

        zoomStack[stackTop] = rect;
    }
}

bool ZoomStack::PopZoomRect( MapRect &rect )
{
    bool canPop = stackSize > 0 && stackTop != stackBottom;
    if( canPop )
    {
        rect = zoomStack[stackTop];
        stackTop--;
        if( stackTop < 0 ) stackTop = stackSize;
    }
    return canPop;
}
//---------------------------------------------------------------------------
// Map Scale - manages scaling and line clipping
// Code for clipping to a rectangular region

#define LEFT 0x01
#define RIGHT 0x02
#define TOP  0x04
#define BOTTOM 0x08

static const double dblZoomAllFactor = 1.05;

wxMapScale::wxMapScale( int windowId ) : windowId( windowId )
{
    npt = 0;
    scaleValid = false;
    zoomOutFactor = 2.0;
}

wxMapScale::wxMapScale( const wxMapScale &scale )
{
    npt = 0;
    scaleValid = false;
    *this = scale;
}

const wxMapScale &wxMapScale::operator = ( const wxMapScale &scale )
{
    npt = 0;
    SetWindowExtents( scale.plotRect, scale.aspectRatio );
    SetMapExtents( scale.mapSize );
    SetWorldExtents( scale.mapWindow );
    return *this;
}

void wxMapScale::SetWindowExtents( const wxRect &extents, double aspect )
{
    plotRect = extents;
    aspectRatio = aspect;
    npt = 0;
    CalcScale();
}

void wxMapScale::SetWorldExtents( const MapRect &extents )
{
    mapExtents = extents;
    npt = 0;
    CalcScale();
}

void wxMapScale::SetMapExtents( const MapRect &extents )
{
    mapSize = extents;
    ZoomAll();
    zoomStack.ClearStack();
}

double wxMapScale::GetScale()
{
    if( ! scaleValid ) return 0;
    return 0.5*(1.0/fabs(wdxscale)+1.0/fabs(wdyscale));
}

void wxMapScale::ZoomTo( const MapRect &extents )
{
    zoomStack.PushZoomRect( mapExtents );
    SetWorldExtents( extents );
}

void wxMapScale::ZoomAll()
{
    ZoomTo( mapSize.ExpandBy(dblZoomAllFactor) );
}

void wxMapScale::ZoomOut( double factor )
{
    ZoomTo( mapExtents.ExpandBy( factor ) );
}

void wxMapScale::Pan( double xShift, double yShift )
{
    ZoomTo( mapExtents.Shift( xShift, yShift ) );
}

void wxMapScale::ZoomPrev()
{
    MapRect lastExtents;
    if( zoomStack.PopZoomRect( lastExtents ) )
    {
        SetWorldExtents( lastExtents );
    }
    else
    {
        ZoomAll();
        zoomStack.ClearStack();
    }
}

void wxMapScale::SetZoomStackSize( int nStack )
{
    zoomStack.SetStackSize( nStack );
}

void wxMapScale::CalcScale()
{
    double ddx, ddy, wdx, wdy;
    dxorg = plotRect.GetLeft();   dyorg = plotRect.GetBottom();
    ddx = plotRect.GetRight() - dxorg;
    ddy = plotRect.GetTop() - dyorg;

    wxorg = mapExtents.min.x;  wyorg = mapExtents.min.y;
    wdx = mapExtents.max.x - mapExtents.min.x;
    wdy = mapExtents.max.y - mapExtents.min.y;
    if( wdx < 0.0 ) { wxorg += wdx; wdx = -wdx; }
    if( wdy < 0.0 ) { wyorg += wdy; wdy = -wdy; }

    if( ddx == 0 || ddy == 0 || (wdx == 0 && wdy == 0))
    {
        scaleValid = false;
    }
    else
    {
        if( wdx != 0.0 )
        {
            wdxscale = aspectRatio*ddx / wdx;
        }
        if( wdy != 0.0 )
        {
            wdyscale = ddy/wdy;
        }
        else
        {
            wdyscale = wdxscale*2;
        }
        if( wdx == 0.0 )
        {
            wdxscale = wdyscale*2;
        }
        if( fabs(wdyscale) < fabs(wdxscale) )
        {
            wdxscale = fabs(wdyscale) / aspectRatio;
            if( ddx < 0 ) wdxscale = -wdxscale;
            wxorg -= (ddx/wdxscale-wdx)/2.0;
        }
        else
        {
            wdyscale = fabs(wdxscale);
            if( ddy < 0 ) wdyscale = -wdyscale;
            wdxscale /= aspectRatio;
            wyorg  -= (ddy/wdyscale-wdy)/2.0;
        }
        scaleValid = true;
        PlotToWorld( wxPoint(plotRect.GetLeft(),plotRect.GetBottom()), mapWindow.min );
        PlotToWorld( wxPoint(plotRect.GetRight(),plotRect.GetTop())  , mapWindow.max );
        NotifyWindow();
    }
}

void wxMapScale::WorldToPlot( const MapPoint &data, wxPoint &plot ) const
{
    if(!scaleValid ) return;
    plot.x = (data.x - wxorg)*wdxscale + dxorg + 0.5;
    plot.y = (data.y - wyorg)*wdyscale + dyorg + 0.5;
}

void wxMapScale::PlotToWorld( const wxPoint &plot, MapPoint &data ) const
{
    if( !scaleValid ) return;
    data.x = (plot.x - dxorg)/wdxscale + wxorg;
    data.y = (plot.y - dyorg)/wdyscale + wyorg;
}


char wxMapScale::PointFlags( const MapPoint &pt )
{
    char flags;
    if( pt.x < mapWindow.min.x ) flags = LEFT;
    else if( pt.x > mapWindow.max.x ) flags = RIGHT;
    else flags = 0;
    if( pt.y < mapWindow.min.y ) flags |= BOTTOM;
    else if( pt.y > mapWindow.max.y ) flags |= TOP;
    return flags;
}


/* Assume flag0 | flag1 == 0, flag0 != 0 */
char wxMapScale::ShiftIntoRange( MapPoint &p0, char flag0,
                                 const MapPoint p1, char flag1 )
{
    double xnew, ynew;
    if( flag0 & (LEFT | RIGHT) )
    {
        xnew = (flag0 & LEFT) ? mapWindow.min.x : mapWindow.max.x;
        p0.y = ((p1.x - xnew) * (p0.y) + (xnew - p0.x) * p1.y ) / ( p1.x - p0.x );
        p0.x = xnew;
        if( p0.y < mapWindow.min.y ) flag0 = BOTTOM;
        else if( p0.y > mapWindow.max.y ) flag0 = TOP;
        else flag0 = 0;
        if( !flag0 ) return 1;
        if( flag0 & flag1 ) return 0;
    }

    ynew = (flag0 & BOTTOM) ? mapWindow.min.y : mapWindow.max.y;
    p0.x = ((p1.y - ynew) * (p0.x) + (ynew - p0.y) * p1.x ) / ( p1.y - p0.y );
    p0.y = ynew;
    if( p0.x < mapWindow.min.x || p0.x > mapWindow.max.x ) return 0;
    return 1;
}


bool wxMapScale::TrimLine( const MapPoint &mpFrom, const MapPoint &mpTo,
                           wxPoint &ptFrom, wxPoint &ptTo )
{
    AddPoint( mpFrom, true );
    bool start;
    bool gotPoint = GetPoint( ptFrom, start );
    AddPoint( mpTo, false );
    bool gotLine = (gotPoint || GetPoint( ptFrom, start )) && GetPoint( ptTo, start );
    return gotLine;
}


bool wxMapScale::AddPoint( const MapPoint &pt, bool start )
{
    char flag;

    if( ! scaleValid ) return false;

    flag = PointFlags( pt );

    /* First point on feature - ignore previous */
    if( start )
    {
        if( flag )
        {
            npt = 0;
        }
        else
        {
            npt = 1;
            pt0 = pt;
            s0 = start;
        }
    }

    /* Both points in range */
    else if( !(flag | flagnext) )
    {
        pt0 = pt;
        s0 = false;
        npt = 1;
    }

    /* Trivial rejection */
    else if( flag & flagnext )
    {
        npt = 0;
    }

    /* Start point in range - end point not */
    else if( ! flagnext )
    {
        pt0 = pt;
        ShiftIntoRange( pt0, flag, ptnext, flagnext );
        s0 = false;
        npt = 1;
    }

    /* End point in range - start point not */
    else if( ! flag )
    {
        pt1 = ptnext;
        ShiftIntoRange( pt1, flagnext, pt, flag );
        s1 = true;
        pt0 = pt;
        s0 = false;
        npt = 2;
    }

    /* Both points out range */
    else if( ShiftIntoRange( ptnext, flagnext, pt, flag ))
    {
        pt1 = ptnext;
        s1 = true;
        pt0 = pt;
        s0 = false;
        ShiftIntoRange( pt0, flag, pt1, 0 );
        npt = 2;
    }

    else
    {
        npt = 0;
    }

    ptnext = pt;
    flagnext = flag;
    return npt > 0;
}


bool wxMapScale::NextPoint( wxPoint &pt, bool &start )
{
    if( ! scaleValid ) return false;
    switch( npt )
    {
    case 1: WorldToPlot(pt0,pt); start = s0; break;
    case 2: WorldToPlot(pt1,pt); start = s1; break;
    default: return false;
    }
    npt--;
    return 1;
}

void wxMapScale::NotifyWindow()
{
    if( windowId != wxID_ANY )
    {
        wxWindow *window = wxWindow::FindWindowById( windowId );
        if( window != 0 )
        {
            wxSimpleEvent evt( WX_MAPWINDOW_SCALE_CHANGED );
            window->GetEventHandler()->AddPendingEvent( evt );
        }

    }
}
