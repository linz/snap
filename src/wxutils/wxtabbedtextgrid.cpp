#include "snapconfig.h"
// wxTabbedTextGrid: A wxGrid for displaying a symbology list
//
// Requires a Symbology defining the symbologies to display, "tickon" and "tickoff" icons for
// show/hide display.
//

#include <boost/numeric/conversion/cast.hpp>

#include "wx_includes.hpp"
#include "wxtabbedtextgrid.hpp"

wxString wxTabbedTextSource::GetText()
{
    int nrow = GetRowCount();
    wxString text( GetHeader() );

    // Guesstimate an allocation amount
    int nchar = (int) ((text.Len()+1) * (nrow+1) * 1.1);
    text.Alloc( nchar );
    text.Trim( true );
    text.Trim( false );

    wxRegEx re( " *\t *");
    re.ReplaceAll( &text, "\t" );

    text.Append('\n');
    for( int i = 0; i < nrow; i++ )
    {
        text.Append( wxString( GetRow(i) ) );
        text.Append('\n');
    }
    return text;
}

wxTabbedTextTable::wxTabbedTextTable()
{
    nrow = 0;
    ncol = 0;
    currow = -1;
    colWidth = 0;
    colName = 0;
    rightJustify = 0;

}

wxTabbedTextTable::~wxTabbedTextTable()
{
    ClearSource();
}

int wxTabbedTextTable::GetNumberRows()
{
    return nrow;
}

int wxTabbedTextTable::GetNumberCols()
{
    return ncol;
}

int wxTabbedTextTable::GetColWidth( int col )
{
    return colWidth[col];
}

bool wxTabbedTextTable::GetColRightJustify( int col )
{
    return rightJustify[col];
}

wxString wxTabbedTextTable::GetColLabelValue( int col )
{
    if( colName == 0 ) return "";
    return colName[ col ];
}

wxString wxTabbedTextTable::GetValue( int row, int col )
{
    if( ttsource == 0 ) return "";
    if( row != currow ) GetRow( row );
    wxString value( rowData[col] );
    if( rightJustify[col] ) value.Append("  ");
    return value;
}

wxString wxTabbedTextTable::GetText()
{
    wxString text;
    if( ttsource ) text = ttsource->GetText();
    return text;
}

void wxTabbedTextTable::SetValue( int WXUNUSED(row), int WXUNUSED(col), const wxString& WXUNUSED(str)  )
{
    // Do nothing ...
}

bool wxTabbedTextTable::IsEmptyCell( int WXUNUSED(row) , int WXUNUSED(col) )
{
    return false;
}

std::vector<std::string_view> wxTabbedTextTable::_splitColumns( const std::string_view line )
{
    std::vector<std::string_view> columns;
    size_t start = 0;
    while( true )
    {
        const size_t tab = line.find( '\t', start );
        if( tab == std::string_view::npos )
        {
            columns.push_back( line.substr( start ) );
            break;
        }
        columns.push_back( line.substr( start, tab - start ) );
        start = tab + 1;
    }
    return columns;
}

std::string_view wxTabbedTextTable::_trimBlanks( std::string_view column )
{
    const size_t first = column.find_first_not_of( ' ' );
    if( first == std::string_view::npos ) return std::string_view();
    column.remove_prefix( first );
    column.remove_suffix( column.size() - 1 - column.find_last_not_of( ' ' ) );
    return column;
}

void wxTabbedTextTable::ClearSource()
{
    ttsource = 0;
    rowData.clear();
    if( colWidth != 0 ) { delete [] colWidth; colWidth = 0; }
    if( colName != 0 ) { delete [] colName; colName = 0; }
    if( rightJustify != 0 ) { delete [] rightJustify; rightJustify = 0; }
    currow = -1;
}

void wxTabbedTextTable::SetSource( wxTabbedTextSource *source )
{
    ClearSource();
    ttsource = source;
    if( ttsource != 0 )
    {
        nrow = ttsource->GetRowCount();

        // Get the header rows
        const std::string header = ttsource->GetHeader();
        const std::vector<std::string_view> columns = _splitColumns( header );

        // Allocate the columns
        ncol = boost::numeric_cast<int>( columns.size() );
        colWidth = new int[ncol];
        colName = new wxString[ncol];
        rowData.assign( columns.size(), std::string() );
        rightJustify = new bool[ncol];

        // Parse the column names and alignments

        for( int i = 0; i < ncol; i++ )
        {
            std::string_view column = columns[i];

            // If first character is blank, then right justify
            rightJustify[i] = ! column.empty() && column.front() == ' ';
            if( rightJustify[i] ) column.remove_prefix( 1 );

            // The width includes any leading blanks, the name does not
            colWidth[i] = boost::numeric_cast<int>( column.size() );
            colName[i] = wxString( std::string( _trimBlanks( column ) ) );
        }
    }
}

void wxTabbedTextTable::GetRow( int nrow )
{
    if( ! ttsource ) return;
    if( currow == nrow ) return;

    const std::string row = ttsource->GetRow(nrow);
    const std::vector<std::string_view> columns = _splitColumns( row );

    // A row with too few columns leaves the remaining ones empty, and extra columns are ignored
    for( size_t i = 0; i < rowData.size(); i++ )
    {
        rowData[i] = i < columns.size() ? std::string( _trimBlanks( columns[i] ) ) : std::string();
    }

    currow = nrow;
}



DEFINE_EVENT_TYPE( WX_TTGRID_ROW_SELECTED )

IMPLEMENT_DYNAMIC_CLASS( wxTabbedTextGrid, wxGrid );

BEGIN_EVENT_TABLE(wxTabbedTextGrid, wxGrid)
    EVT_GRID_RANGE_SELECT(wxTabbedTextGrid::OnRangeSelect)
END_EVENT_TABLE()

wxTabbedTextGrid::wxTabbedTextGrid()
{
}

wxTabbedTextGrid::wxTabbedTextGrid(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size ) :
    wxGrid( parent, id, pos, size )
{
    table = new wxTabbedTextTable();
    SetTable( table, false, wxGrid::wxGridSelectRows );

    SetGridLineColour( wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE) );
    SetSelectionBackground( wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT) );
    SetSelectionForeground( wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT) );
    SetCellHighlightPenWidth(0);
    // EnableGridLines( false );
    EnableEditing( false );
    EnableDragColSize();
    DisableDragRowSize();

    SetSelectionMode(wxGrid::wxGridSelectRows);
    SetRowLabelSize(0);
    //SetColLabelSize(0);
}

wxTabbedTextGrid::~wxTabbedTextGrid()
{
    SetTable( 0, false, wxGrid::wxGridSelectRows );
    delete table;
}

void wxTabbedTextGrid::SetTabbedTextSource( wxTabbedTextSource *source )
{

    table->SetSource( source );

    // Hopefully this will reset row/column counts etc??

    BeginBatch();

    // Call SetDefaultColSize to clear the column size arrays, otherwise
    // get assert errors when number of columns is changed.
    //
    // Patch applied to 2.8.0 version of wxGrid to fix this .. but may
    // need to reinstate this code if building against unpatched or
    // different version.

    // SetDefaultColSize( GetDefaultColSize(), true );
    // SetDefaultRowSize( GetDefaultRowSize(), true );

    SetTable( table, false, wxGrid::wxGridSelectRows );
    //InitColWidths();

    // Set up column attributes

    for( int col = 0; col < table->GetNumberCols(); col++ )
    {
        bool right = table->GetColRightJustify( col );
        wxGridCellAttr *attr = new wxGridCellAttr();
        attr->SetAlignment( right ? wxALIGN_RIGHT : wxALIGN_LEFT, wxALIGN_TOP );
        SetColAttr( col, attr );
        int nchar = table->GetColWidth( col );
        SetColSize( col, (nchar+2)*GetCharWidth() );
    }

    EndBatch();

    ForceRefresh();
}

wxString wxTabbedTextGrid::GetText()
{
    wxString text;
    if( table ) text = table->GetText();
    return text;
}

void wxTabbedTextGrid::OnCellSelect( wxGridEvent &event )
{
    // Code added trying to fix issue with OnRangeSelect not being triggered.
    // Issue appears to have resolved itself?

}

void wxTabbedTextGrid::OnRangeSelect( wxGridRangeSelectEvent &event )
{
    if( ! event.Selecting() ) return;
    int row = event.GetBottomRow();
    if( row != event.GetTopRow() )
    {
        SelectRow( event.GetBottomRow() );
        return;
    }
    long keystate = 0;
    if( event.ShiftDown() ) keystate |= WX_TTGRID_SHIFTDOWN;
    if( event.ControlDown() ) keystate |= WX_TTGRID_CTRLDOWN;
    if( event.AltDown() ) keystate |= WX_TTGRID_ALTDOWN;
    SendRowSelectedEvent(row, keystate );
}

void wxTabbedTextGrid::SendRowSelectedEvent( int row, long keystate )
{
    wxCommandEvent evt( WX_TTGRID_ROW_SELECTED, GetId() );
    evt.SetEventObject( this );
    evt.SetInt( row );
    evt.SetExtraLong( keystate );
    AddPendingEvent( evt );
}
