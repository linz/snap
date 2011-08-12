#ifndef SNAPPLOT_MAP_HPP
#define SNAPPLOT_MAP_HPP

// snapplot_map is a class interfacing between the old SNAPPLOT C code and
// the wxMap class..

#include "wxmap.hpp"

//extern "C" {
#include "plotfunc.h"
//}

class SnapplotMap : public wxMap
{
public:
    SnapplotMap();
    virtual ~SnapplotMap();
    virtual void GetMapExtents( MapRect &extents );
    virtual void GetLayer( Symbology &symbology );
    virtual void *StartDrawing();
    virtual bool DrawSome( wxMapDrawer &drawer, void *state );
    virtual void EndDrawing( void *state );
private:

    enum
    {
        smBackground,
        smObs,
        smStations,
        smCovariances,
        smHgtErrors,
        smAdjustments,
        smStnNames,
        smHltStns,
        smHltObs,
        smDrawScale,
        smRedrawObs,
        smMapDone,
        smFirstSection = smBackground
    };

    int section;
    int item;
    int pconn_option;
    double data_offset;
    bool startDrawing;

    map_plotter plotter;

};

#endif