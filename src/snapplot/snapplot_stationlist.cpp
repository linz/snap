#include "snapconfig.h"
#include "snapplot_stationlist.hpp"
#include "snapplot_event.hpp"

//extern "C" {
#include "plotstns.h"
#include "infowin.h"
//}

SnapplotStationSource::SnapplotStationSource()
{
}

SnapplotStationSource::~SnapplotStationSource()
{
}

char *SnapplotStationSource::GetHeader()
{
    return station_list_header();
}

int SnapplotStationSource::GetRowCount()
{
    return used_station_count();
}

char *SnapplotStationSource::GetRow( int i )
{
    return station_list_item(i);
}


BEGIN_EVENT_TABLE( SnapplotStationList, wxTabbedTextGrid )
    EVT_GRID_LABEL_LEFT_CLICK( SnapplotStationList::OnLabelLeftClick )
    EVT_GRID_CELL_LEFT_CLICK( SnapplotStationList::OnCellLeftClick )
    EVT_GRID_CELL_LEFT_DCLICK( SnapplotStationList::OnCellLeftDClick )
    EVT_COMMAND( wxID_ANY, WX_TTGRID_ROW_SELECTED, SnapplotStationList::OnRowSelected )
    EVT_SET_FOCUS( SnapplotStationList::OnSetFocus )
    EVT_KILL_FOCUS( SnapplotStationList::OnSetFocus )
END_EVENT_TABLE()

SnapplotStationList::SnapplotStationList( wxWindow* parent, wxWindowID id )
    : wxTabbedTextGrid( parent, id )
{
}

SnapplotStationList::~SnapplotStationList()
{
}

void SnapplotStationList::Reload()
{
    init_station_list();
    SetTabbedTextSource( &source );
}

void SnapplotStationList::OnLabelLeftClick( wxGridEvent &event )
{
    int colid = event.GetCol();
    sort_station_list_col( colid );
    Refresh();
}


void SnapplotStationList::OnCellLeftClick( wxGridEvent &event )
{
    int row = event.GetRow();
    SelectRow(row);

    if( event.ShiftDown() )
    {
        wxSnapplotEvent spevent( wxEVT_ZOOM_MAPVIEW, GetId() );
        spevent.SetEventObject(this);
        PutTextInfo jump;
        station_item_info( row, &jump );
        spevent.SetInfo( jump );
        AddPendingEvent( spevent );
    }

}


void SnapplotStationList::OnCellLeftDClick( wxGridEvent & WXUNUSED(event) )
{
    wxCommandEvent evt( wxEVT_COMMAND_MENU_SELECTED, CMD_VIEW_DETAILS );
    AddPendingEvent( evt );
}

void SnapplotStationList::OnRowSelected( wxCommandEvent &event )
{
    int row = event.GetInt();
    PutTextInfo jump;
    station_item_info( row, &jump );

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

void SnapplotStationList::OnSetFocus( wxFocusEvent & WXUNUSED(event) )
{
    ClearSelection();
}