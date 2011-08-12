#ifndef WXMAP_HPP
#define WXMAP_HPP

// Abstract base class for a map.  Requires functions to
// get the symbology, get the map extents, and draw the map
// using a map drawer.
//
// The drawing is potentially broken up into chunks by providing
// a StartDrawing function, which returns a status, a DrawSome
// function, which draws some of the map, returning true if there
// is more to draw, and an EndDrawing function.  The purpose of the
// state is to allow more than one drawing task to run in parallel.
//
// NOTE: DrawSome function must return false on completion, otherwise
// the program may end up drawing forever!!!

#include "wxsymbology.hpp"
#include "wxmapdrawer.hpp"

class wxMap
{
public:
    wxMap();
    virtual ~wxMap();
    virtual void GetMapExtents( MapRect &extents ) = 0;
    virtual void GetLayer( Symbology &symbology ) = 0;
    void DrawMap( wxMapDrawer &drawer );
    virtual void *StartDrawing() = 0;
    virtual bool DrawSome( wxMapDrawer &drawer, void *state ) = 0;
    virtual void EndDrawing( void *state ) = 0;
private:
};



#endif
