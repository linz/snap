#ifndef SNAPPLOT_DRAGGER_HPP
#define SNAPPLOT_DRAGGER_HPP

#include "wxmapwindow.hpp"

class SnapplotMapDragger : public wxMapScaleDragger {
 public:
    SnapplotMapDragger(int tolerance = 10);
    ~SnapplotMapDragger();
    bool InitDrag(const wxMouseEvent& event) override;
    // virtual void SetDragCursor();
    void EndDrag() override;
    void SetPickTolerance(int tolerance) {
        pickTolerance = tolerance;
    }

 protected:
    void DrawDragger(wxDC& dc) override;

 private:
    bool shiftStation;
    void DeleteConnections();
    int pickTolerance;
    int radius;
    int istn;
    MapPoint from;
    int nConnections;
    MapPoint* connections;
};

#endif
