#ifndef SNAPPLOT_OBSLIST_HPP
#define SNAPPLOT_OBSLIST_HPP

#include "wxtabbedtextgrid.hpp"

class SnapplotObsSource : public wxTabbedTextSource
{
public:
    SnapplotObsSource();
    ~SnapplotObsSource();
    virtual char *GetHeader();
    virtual int GetRowCount();
    virtual char *GetRow( int i );
};

class SnapplotObsList : public wxTabbedTextGrid
{
public:
    SnapplotObsList( wxWindow* parent, wxWindowID id );
    ~SnapplotObsList();
    void Reload();
private:
    void OnCellLeftClick( wxGridEvent &event );
    void OnCellLeftDClick( wxGridEvent &event );
    void OnLabelLeftClick( wxGridEvent &event );
    void OnLabelRightClick( wxGridEvent &event );
    void OnRowSelected( wxCommandEvent &event );
    void OnSetFocus( wxFocusEvent &event );
    SnapplotObsSource source;
    DECLARE_EVENT_TABLE();
};

#endif
