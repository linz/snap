#include "snapconfig.h"
#include "wx_includes.hpp"
#include "ctrl_station_selector.hpp"

#include <stdlib.h>
#include <search.h>

//extern "C" {
#include "plotstns.h"
#include "snap/stnadj.h"
//}
void wxStationSelectorTable::Setup()
{
    id = 0;
    selected = 0;
    nrows = used_station_count();
    if( nrows <= 0 ) return;

    id = new int[nrows];
    int nstns = number_of_stations(net);
    selected = new bool[nstns+1];

    for( int i = 0; i < nrows; i++ )
    {
        int istn = sorted_station_number(i);
        id[i] = istn;
        selected[istn] = false;
    }

    SortByCode();

    // TODO: Sort out these colours
    selectedAttr = new wxGridCellAttr();
    selectedAttr->SetBackgroundColour( wxColour("LIGHT BLUE") );
    unselectedAttr = new wxGridCellAttr();
    unselectedAttr->SetBackgroundColour( wxColour("WHITE") );
}

void wxStationSelectorTable::CleanUp()
{
    if( id ) delete [] id;
    if( selected ) delete [] selected;
    id = 0;
    selected = 0;
    selectedAttr->DecRef();
    unselectedAttr->DecRef();
}

wxString wxStationSelectorTable::GetColLabelValue( int col )
{
    wxString label;
    if( col == 0 )
    {
        label = wxString("Code");
    }
    else
    {
        label = wxString("Name");
    }
    return label;
}

int wxStationSelectorTable::GetNumberRows() { return nrows; }

int wxStationSelectorTable::GetNumberCols() { return 2; }

bool wxStationSelectorTable::IsEmptyCell( int WXUNUSED(row), int WXUNUSED(col) ) { return false; }

wxString wxStationSelectorTable::GetValue( int row, int col )
{
    int istn = id[row];
    station *stn = stnptr(istn);
    return col == 0 ? stn->Code : stn->Name;
}

void wxStationSelectorTable::SetValue( int WXUNUSED(row), int WXUNUSED(col), const wxString & WXUNUSED(value)  ) {}

static int cmp_station_code( const void *istn1, const void *istn2 )
{
    return stncodecmp( stnptr(*(int *)istn1)->Code, stnptr(*(int *)istn2)->Code );
}

wxGridCellAttr *wxStationSelectorTable::GetAttr( int row, int WXUNUSED(col),  wxGridCellAttr::wxAttrKind  WXUNUSED(kind)  )
{
    wxGridCellAttr *attr = IsSelected(row) ? selectedAttr : unselectedAttr;
    attr->IncRef();
    return attr;
}

static int cmp_station_name( const void *istn1, const void *istn2 )
{
    return _stricmp( stnptr(*(int *)istn1)->Name, stnptr(*(int *)istn2)->Name );
}

void wxStationSelectorTable::SortByCode()
{
    qsort( id, nrows, sizeof(int), cmp_station_code );
}

void wxStationSelectorTable::SortByName()
{
    qsort( id, nrows, sizeof(int), cmp_station_name );
}

void wxStationSelectorTable::Select( int row, bool select  )
{
    selected[id[row]] = select;
}

bool wxStationSelectorTable::IsSelected( int row )
{
    return selected[id[row]];
}

int wxStationSelectorTable::StationNumber( int row )
{
    return id[row];
}

//////////////////////////////////////////////////////////////////////////////
// Interrupting this event seems to screw up selection!
//	EVT_GRID_CELL_LEFT_CLICK( wxStationSelectorGrid::OnGridCellLeftClick )
// Range select called anyway, so cancels this out?
//	EVT_GRID_SELECT_CELL( wxStationSelectorGrid::OnGridSelectCell )

BEGIN_EVENT_TABLE( wxStationSelectorGrid, wxGrid )
    EVT_GRID_RANGE_SELECT( wxStationSelectorGrid::OnGridRangeSelect )
    EVT_GRID_LABEL_LEFT_CLICK( wxStationSelectorGrid::OnGridLabelLeftClick )
END_EVENT_TABLE()

wxStationSelectorGrid::wxStationSelectorGrid(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size ) :
    wxGrid( parent, id, pos, size )
{
    table = new wxStationSelectorTable();
    SetTable( table, false, wxGrid::wxGridSelectRows );

    // Trying to get this sized right???
    SetSize( wxSize( GetCharWidth()*40, GetCharHeight()*20) );
    SetMinSize( wxSize( GetCharWidth()*40, GetCharHeight()*10) );

    SetGridLineColour( wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE) );
    SetSelectionBackground( wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT) );
    SetSelectionForeground( wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT) );
    SetCellHighlightPenWidth(0);
    // EnableGridLines( false );
    EnableEditing( false );
    EnableDragColSize();
    DisableDragRowSize();

    // SetSelectionMode(wxGrid::wxGridSelectRows);
    SetRowLabelSize(0);
    SetColLabelSize( (GetCharHeight()*3)/2 );

    SetColSize( 0, GetCharWidth()*12 );
    SetColSize( 1, GetCharWidth()*50 );
    //SetColLabelSize(0);
}

wxStationSelectorGrid::~wxStationSelectorGrid()
{
    SetTable( 0 );
    delete table;
}

void wxStationSelectorGrid::SortByCode()
{
    table->SortByCode();
    // SetTable( table, false, wxGrid::wxGridSelectRows );
    Refresh();
}

void wxStationSelectorGrid::SortByName()
{
    table->SortByName();
    // SetTable( table, false, wxGrid::wxGridSelectRows );
    Refresh();
}

void wxStationSelectorGrid::OnGridRangeSelect( wxGridRangeSelectEvent &event )
{
    if( event.Selecting() )
    {
        ClearSelection();
        int rowMin = event.GetTopRow();
        int rowMax = event.GetBottomRow();
        for( int row = rowMin; row <= rowMax; row++ )
        {
            table->Select( row, ! table->IsSelected(row) );
        }
        // RefreshRect( BlockToDeviceRect(wxGridCellCoords(rowMin,0) , wxGridCellCoords(rowMax,1)) );
        Refresh();
    }
}

void wxStationSelectorGrid::OnGridLabelLeftClick( wxGridEvent &event )
{
    int col = event.GetCol();
    if( col == 0 ) SortByCode();
    if( col == 1 ) SortByName();
}
