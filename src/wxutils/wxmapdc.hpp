#ifndef WXMAPDC_HPP
#define WXMAPDC_HPP

// Basics map types

#include "wx_includes.hpp"
#include "wxmapscale.hpp"


//---------------------------------------------------------------------------
//  MapCanvas: Simple combination of a scale with a canvas to facilitate
// plotting of objects on the canvas using real world coordinates

class wxMapDC
{
public:
    wxMapDC( wxDC *dc, wxMapScale &scale ) : dc(dc), scale(scale) {;}
    wxDC *DC() { return dc; }
    wxMapScale &Scale() { return scale; }
    bool DrawLine( const MapPoint &mptFrom, const MapPoint &mptTo );
    // void LineTo( const MapPoint &mpt );
private:
    wxDC *dc;
    wxMapScale &scale;
};


#endif