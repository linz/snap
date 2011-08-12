#ifndef WXMAPDRAWER_HPP
#define WXMAPDRAWER_HPP

#include "wx_includes.hpp"
#include "wxmapscale.hpp"
#include "wxsymbology.hpp"

class wxMapDrawer
{
public:
    wxMapDrawer( wxDC &dc, wxMapScale &scale, Symbology &symbology );
    ~wxMapDrawer();

    void SetLayer( int layerId );
    void SetSymbol( int symbolId );
    void SetTextStyle( int textStyleId );
    void SetLineStyle( int lineStyle );

    const Symbology &GetSymbology() { return symbology; }
    const wxMapScale &GetScale() { return scale; }

    void Clear( const wxColour &colour );
    void MoveTo( const MapPoint &pt );
    void LineTo( const MapPoint &pt );
    void EndLine();
    void PlotLine( const MapPoint ptFrom, const MapPoint ptTo );
    void PlotEllipse( const MapPoint &pt, double majorAxis, double minorAxis, double orientation );

    void PlotSymbol( const MapPoint &pt );

    void PlotText( const MapPoint &pt, TextAlign alignment, const wxString &text );

    // Draw any pending items ..
    void FlushMap();

private:
    void AddScalePoints();
    void PlotLine();
    void AddLinePoint( const wxPoint &pt );
    void DrawLine();
    void ClearLine();
    void DrawSymbol( const wxPoint &pt );

    wxDC &dc;
    wxMapScale &scale;
    Symbology &symbology;

    int refSize;
    bool isVisible;
    bool lineStarted;
    bool autoPlotSymbol;
    int layerId;
    int symbolId;
    int textStyleId;
    int lineStyle;

    int maxPoints;
    int nPoints;
    wxColour currentColour;
    PointSymbology *currentSymbol;
    TextStyle *currentTextStyle;
    TextStyle *defaultTextStyle;

    wxPoint *points;
    TextAlign alignment;
};


#endif