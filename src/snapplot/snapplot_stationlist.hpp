#ifndef SNAPPLOT_STATIONLIST_HPP
#define SNAPPLOT_STATIONLIST_HPP

#include <string>

#include "wxtabbedtextgrid.hpp"

class SnapplotStationSource : public wxTabbedTextSource
{
public:
    SnapplotStationSource();
    ~SnapplotStationSource() override;
    std::string GetHeader() override;
    int GetRowCount() override;
    std::string GetRow( int i ) override;
};

class SnapplotStationList : public wxTabbedTextGrid
{
public:
    SnapplotStationList( wxWindow* parent, wxWindowID id );
    ~SnapplotStationList() override;
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
