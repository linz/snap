#ifndef CTRL_STATION_SELECTOR_HPP
#define CTRL_STATION_SELECTOR_HPP

#include "wx_includes.hpp"

class wxStationSelectorTable : public wxGridTableBase
{
public:
    wxStationSelectorTable() { Setup(); }
    ~wxStationSelectorTable() override { CleanUp(); }

    int GetNumberRows() override;
    int GetNumberCols() override;
    bool IsEmptyCell( int row, int col ) override;
    wxString GetValue( int row, int col ) override;
    wxString GetColLabelValue( int col ) override;
    void SetValue( int row, int col, const wxString &value  ) override;
    wxGridCellAttr *GetAttr( int row, int col,  wxGridCellAttr::wxAttrKind  kind  ) override;

    void Select( int row, bool select = true );
    bool IsSelected( int row );
    int StationNumber( int row );

    void SortByCode();
    void SortByName();

private:

    void Setup();
    void CleanUp();

    int nrows;
    int *id;
    bool *selected;
    wxGridCellAttr * selectedAttr;
    wxGridCellAttr * unselectedAttr;

};

class wxStationSelectorGrid : public wxGrid
{
public:
    wxStationSelectorGrid(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
    ~wxStationSelectorGrid() override;

    void Select( int row, bool select = true ) { table->Select(row, select ); }
    bool IsSelected( int row ) { return table->IsSelected(row); }
    int StationNumber( int row ) { return table->StationNumber(row); }
    int StationCount() { return table->GetNumberRows(); }

    void SortByCode();
    void SortByName();

private:
    void OnGridLabelLeftClick( wxGridEvent &event );
    void OnGridRangeSelect( wxGridRangeSelectEvent &event );

    wxStationSelectorTable *table;

    DECLARE_EVENT_TABLE()
};



#endif