#ifndef SNAPPLOT_DRAGGER_HPP
#define SNAPPLOT_DRAGGER_HPP

#include "wxmapwindow.hpp"

class SnapplotMapDragger : public wxMapScaleDragger
{
public:
    SnapplotMapDragger( int tolerance = 10 );
    ~SnapplotMapDragger();
    virtual bool InitDrag( const wxMouseEvent &event );
    // virtual void SetDragCursor();
    virtual void EndDrag();
    void SetPickTolerance( int tolerance ) { pickTolerance = tolerance; }

protected:
    virtual void DrawDragger( wxDC &dc );

private:
    bool shiftStation;
    void DeleteConnections();
    int pickTolerance;
    int radius;
    int istn;
    MapPoint from;
    int nConnections;
    MapPoint *connections;
};


#endif


