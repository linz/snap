#ifndef SNAPPLOT_STATIONLIST_HPP
#define SNAPPLOT_STATIONLIST_HPP

#include "wxtabbedtextgrid.hpp"

class SnapplotStationSource : public wxTabbedTextSource
{
public:
    SnapplotStationSource();
    ~SnapplotStationSource();
    virtual char *GetHeader();
    virtual int GetRowCount();
    virtual char *GetRow( int i );
};

class SnapplotStationList : public wxTabbedTextGrid
{
public:
    SnapplotStationList( wxWindow* parent, wxWindowID id );
    ~SnapplotStationList();
    void Reload();
private:
    void OnLabelLeftClick( wxGridEvent &event );
    void OnCellLeftDClick( wxGridEvent &event );
    void OnCellLeftClick( wxGridEvent &event );
    void OnRowSelected( wxCommandEvent &event );
    void OnSetFocus( wxFocusEvent &event );
    SnapplotStationSource source;
    DECLARE_EVENT_TABLE();
};

#endif
