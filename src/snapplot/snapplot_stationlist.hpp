#ifndef SNAPPLOT_STATIONLIST_HPP
#define SNAPPLOT_STATIONLIST_HPP

#include <string>

#include "wxtabbedtextgrid.hpp"

class SnapplotStationSource : public wxTabbedTextSource
{
public:
    SnapplotStationSource();
    ~SnapplotStationSource();
    virtual std::string GetHeader();
    virtual int GetRowCount();
    virtual std::string GetRow( int i );
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
