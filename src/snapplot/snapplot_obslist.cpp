#include "snapconfig.h"
#include "snapplot_obslist.hpp"
#include "snapplot_event.hpp"

//extern "C" {
#include "plotconn.h"
//}

SnapplotObsSource::SnapplotObsSource()
{
}

SnapplotObsSource::~SnapplotObsSource()
{
}

char *SnapplotObsSource::GetHeader()
{
    return sres_list_header();
}

int SnapplotObsSource::GetRowCount()
{
    return sres_index_count();
}

char *SnapplotObsSource::GetRow( int i )
{
    return sres_item_description( i );
}


BEGIN_EVENT_TABLE( SnapplotObsList, wxTabbedTextGrid )
    EVT_GRID_LABEL_LEFT_CLICK( SnapplotObsList::OnLabelLeftClick )
    EVT_GRID_LABEL_RIGHT_CLICK( SnapplotObsList::OnLabelRightClick )
    EVT_GRID_CELL_LEFT_CLICK( SnapplotObsList::OnCellLeftClick )
    EVT_GRID_CELL_LEFT_DCLICK( SnapplotObsList::OnCellLeftDClick )
    EVT_COMMAND( wxID_ANY, WX_TTGRID_ROW_SELECTED, SnapplotObsList::OnRowSelected )
    EVT_SET_FOCUS( SnapplotObsList::OnSetFocus )
    EVT_KILL_FOCUS( SnapplotObsList::OnSetFocus )
END_EVENT_TABLE()

SnapplotObsList::SnapplotObsList( wxWindow* parent, wxWindowID id )
    : wxTabbedTextGrid( parent, id )
{
}

SnapplotObsList::~SnapplotObsList()
{
}


void SnapplotObsList::Reload()
{
    // Force the grid to reload the source...
    SetTabbedTextSource( &source );
}


void SnapplotObsList::OnCellLeftClick( wxGridEvent &event )
{
    int row = event.GetRow();
    SelectRow(row);

    if( event.ShiftDown() )
    {
        PutTextInfo jump;
        sres_item_info( row, &jump );
        wxSnapplotEvent spevent( wxEVT_ZOOM_MAPVIEW, GetId() );
        spevent.SetInfo( jump );
        spevent.SetEventObject(this);
        AddPendingEvent( spevent );
    }
}

void SnapplotObsList::OnCellLeftDClick( wxGridEvent & WXUNUSED(event) )
{
    wxCommandEvent evt( wxEVT_COMMAND_MENU_SELECTED, CMD_VIEW_DETAILS );
    AddPendingEvent( evt );
}

void SnapplotObsList::OnRowSelected( wxCommandEvent &event )
{
    int row = event.GetInt();
    PutTextInfo jump;
    sres_item_info( row, &jump );

    wxSnapplotEvent spevent( wxEVT_SET_DETAILVIEW, GetId() );
    spevent.SetInfo( jump );
    spevent.SetEventObject(this);
    AddPendingEvent( spevent );

    /*
    if( event.GetExtraLong() & WX_TTGRID_SHIFTDOWN )
    {
    	spevent.SetEventType( wxEVT_ZOOM_MAPVIEW );
    	AddPendingEvent( spevent );
    }
    */

    spevent.SetEventType( wxEVT_LOCATE_MAPVIEW );
    AddPendingEvent( spevent );

}

void SnapplotObsList::OnLabelLeftClick( wxGridEvent &event )
{
    int colid = event.GetCol();
    set_sres_sort_col( colid );
    Refresh();
}

void SnapplotObsList::OnLabelRightClick( wxGridEvent & WXUNUSED(event) )
{
    // Send a display options command message ...

    wxCommandEvent evt( wxEVT_COMMAND_MENU_SELECTED, CMD_DATA_LISTOPTIONS);
    AddPendingEvent( evt );
}

void SnapplotObsList::OnSetFocus( wxFocusEvent & WXUNUSED(event) )
{
    ClearSelection();
}