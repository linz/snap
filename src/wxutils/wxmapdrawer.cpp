#include "snapconfig.h"
#include "wxmapdrawer.hpp"

// wxMapDrawer class ... used to interface the map with the symbology,
// scale, etc...

// TODO: Look at buffering bitmaps of symbols on a per symbol/layer id, if this is
// likely to improve performance...

wxMapDrawer::wxMapDrawer( wxDC &dc, wxMapScale &scale, Symbology &symbology ) :
    dc(dc),
    scale(scale),
    symbology(symbology)
{
    // Set a default font in the DC

    refSize = (int) ((dc.GetPPI().GetWidth()*6)/72);

    // TODO: work out how to set a reference size... maybe based upon text font size?
    // Is there a more direct conversion between, say, em units and pixels..

    isVisible = false;
    lineStarted =false;
    layerId = -1;
    symbolId = -1;
    textStyleId = -1;
    lineStyle = wxSOLID;
    // TODO: consider more dynamic maximum number of points in drawer..
    nPoints = 0;
    maxPoints = 50;
    points = new wxPoint[maxPoints];
    //
    currentColour =wxColour("BLACK");
    currentSymbol = 0;

    // TextStyle calculates the size based upon 10pt = refsize

    defaultTextStyle = new TextStyle("default",wxFont( 10, wxFONTFAMILY_SWISS,wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL ));
    currentTextStyle = 0;
}

wxMapDrawer::~wxMapDrawer()
{
    if( points ) delete [] points;
    delete defaultTextStyle;
}

void wxMapDrawer::FlushMap()
{
    EndLine();
}

void wxMapDrawer::Clear( const wxColour &colour )
{
    dc.SetBackground( wxBrush( colour ));
    dc.Clear();

}

void wxMapDrawer::ClearLine()
{
    DrawLine();
    lineStarted = false;
}

void wxMapDrawer::AddLinePoint( const wxPoint &pt )
{
    points[nPoints++] = pt;
    if( nPoints >= maxPoints )
    {
        DrawLine();
        points[nPoints++] = pt;
    }
}

void wxMapDrawer::DrawLine()
{
    if( nPoints > 1 )
    {
        dc.SetPen( wxPen(currentColour, 1, lineStyle) );
        dc.DrawLines( nPoints, points );
    }
    else if( nPoints == 1 && autoPlotSymbol )
    {
        DrawSymbol( points[0] );
    }
    nPoints = 0;
}

void wxMapDrawer::AddScalePoints()
{
    wxPoint pt;
    bool start;
    while( scale.GetPoint( pt, start ) )
    {
        if( start ) DrawLine();
        AddLinePoint( pt );
    }
}

void wxMapDrawer::MoveTo( const MapPoint &pt )
{
    if( ! isVisible ) { return; }
    ClearLine();
    lineStarted = true;
    scale.AddPoint( pt, true );
    AddScalePoints();
}

void wxMapDrawer::LineTo( const MapPoint &pt )
{
    if( ! isVisible ) { return; }
    if( ! lineStarted ) { return; }
    scale.AddPoint( pt, false );
    AddScalePoints();
}

void wxMapDrawer::EndLine()
{
    if( ! lineStarted ) { return; }
    DrawLine();
    lineStarted = false;
}

void wxMapDrawer::PlotLine( const MapPoint ptFrom, const MapPoint ptTo )
{
    if( ! isVisible ) return;
    MoveTo( ptFrom );
    LineTo( ptTo );
    EndLine();
}

void wxMapDrawer::PlotEllipse(const MapPoint &pt, double majorAxis, double minorAxis, double orientation)
{
    if( ! isVisible) return;

    // Calculate the number of points required to display the ellipse

    double pixelSize = scale.GetScale();
    int npt = (int) (1.6*sqrt((majorAxis+minorAxis)/(4.0*pixelSize)));
    if( npt < 2 )
    {
        npt = 2;
    }
    else if( npt > 18 )
    {
        npt = 18;
    }
    npt *= 4;
    double angle = atan(1.0)*8.0/npt;
    double cosa = cos(angle);
    double sina = sin(angle);
    double coso = cos(orientation);
    double sino = sin(orientation);
    double cs = 1.0;
    double sn = 0.0;

    MapPoint pt0 = pt.Shift( majorAxis * coso, majorAxis * sino );
    MoveTo( pt0 );

    for( int ipt = 1; ipt < npt; ipt++ )
    {
        double tmp = cs*cosa + sn*sina;
        sn = sn*cosa - cs*sina;
        cs = tmp;
        double ex = cs*majorAxis;
        double ey = sn*minorAxis;

        LineTo( pt.Shift( ex*coso + ey*sino, ex*sino - ey*coso ));

    }
    LineTo( pt0 );
    EndLine();
}

void wxMapDrawer::SetLayer( int layerId )
{
    ClearLine();
    this->layerId = layerId;
    isVisible = symbology.ShowLayer( layerId );
    currentColour = symbology.LayerColour( layerId );
}

void wxMapDrawer::SetSymbol( int symbolId )
{
    if( symbolId != this->symbolId )
    {
        this->symbolId = symbolId;
        currentSymbol = symbology.GetPointSymbol( symbolId );
    }
}

void wxMapDrawer::SetTextStyle( int textStyleId )
{
    if( textStyleId != this->textStyleId )
    {
        this->textStyleId = textStyleId;
        currentTextStyle = symbology.GetTextStyle( textStyleId );
    }
}

void wxMapDrawer::SetLineStyle( int lineStyle )
{
    this->lineStyle = lineStyle;
}

void wxMapDrawer::PlotSymbol( const MapPoint &pt )
{
    if( ! isVisible ) { return; }
    ClearLine();
    if( scale.GetWorldExtents().Contains(pt) )
    {
        wxPoint ppt;
        scale.WorldToPlot( pt, ppt );
        DrawSymbol( ppt );
    }
}

void wxMapDrawer::PlotText( const MapPoint &pt, TextAlign alignment, const wxString &text )
{
    if( ! isVisible ) { return; }
    ClearLine();
    wxPoint ppt;
    scale.WorldToPlot( pt, ppt );
    TextStyle *style = currentTextStyle == 0 ? defaultTextStyle : currentTextStyle;
    style->Render( dc, ppt, text,alignment,refSize,currentColour);
}

void wxMapDrawer::DrawSymbol( const wxPoint &pt )
{
    if( currentSymbol )
    {
        currentSymbol->Render( dc, pt, refSize, currentColour );
    }
    else
    {
        dc.SetPen( currentColour );
        int size = refSize / 2;
        dc.DrawLine( pt.x - size, pt.y, pt.x + size + 1, pt.y );
        dc.DrawLine( pt.x, pt.y - size, pt.x, pt.y + size + 1 );
    }
}

