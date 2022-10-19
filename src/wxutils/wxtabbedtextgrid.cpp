#include "snapconfig.h"
// wxTabbedTextGrid: A wxGrid for displaying a symbology list
//
// Requires a Symbology defining the symbologies to display, "tickon" and "tickoff" icons for
// show/hide display.
//

#include "wx_includes.hpp"
#include "wxtabbedtextgrid.hpp"

wxString wxTabbedTextSource::GetText()
{
    int nrow = GetRowCount();
    wxString text(GetHeader());

    // Guestimate an allocation amount
    int nchar = (int) ((text.Len()+1) * (nrow+1) * 1.1);
    text.Alloc( nchar );
    text.Trim( true );
    text.Trim( false );

    wxRegEx re( " *\t *");
    re.ReplaceAll( &text, "\t" );

    text.Append('\n');
    for( int i = 0; i < nrow; i++ )
    {
        text.Append( GetRow(i) );
        text.Append('\n');
    }
    return text;
}

wxTabbedTextTable::wxTabbedTextTable()
{
    nrow = 0;
    ncol = 0;
    currow = -1;
    bufferlen = 0;
    buffer = 0;
    colWidth = 0;
    colName = 0;
    rightJustify = 0;
    rowData = 0;

}

wxTabbedTextTable::~wxTabbedTextTable()
{
    ClearSource();
    if( buffer != 0 ) { delete [] buffer; buffer = 0; bufferlen = 0; }
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
    wxString value(rowData[col]);
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

void wxTabbedTextTable::StoreString( char *string )
{
    int len = strlen( string ) + 1;
    if( len > bufferlen )
    {
        if( buffer != 0 ) delete [] buffer;
        len *= 2;
        if( len < 2048 ) len = 2048;
        buffer = new char[len];
        bufferlen = len;
    }
    strcpy( buffer, string );
}

void wxTabbedTextTable::ClearSource()
{
    ttsource = 0;
    if(	rowData != 0 ) { delete [] rowData; rowData = 0; }
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
        StoreString(ttsource->GetHeader());

        // Count and allocate the columns
        ncol = 1;
        for( char *c = buffer; *c; c++ )
        {
            if( *c == '\t' ) ncol++;
        }
        colWidth = new int[ncol];
        colName = new wxString[ncol];
        rowData = new char *[ncol];
        rightJustify = new bool[ncol];

        // Parse the column names and alignments

        char *c = buffer;
        for( int i = 0; i < ncol; i++ )
        {
            rightJustify[i] = false;

            // If first character is blank, then right justify
            if( *c == ' ' )
            {
                rightJustify[i] = true;
                c++;
            }

            // Find end of string, and last non-blank character, and point c
            // to the beginning of the next column name

            char *s = c;
            char *e;
            char *e1;
            for( e = c, e1 = c; *e && *e != '\t'; e++ ) { if( *e != ' ') e1 = e; }
            c = e;
            if( *c ) c++;
            *e = 0;

            // Get the length of the column, then skip any leading blanks
            int collen = strlen(s);
            while( *s == ' ' ) s++;

            // Mark the character after the last non-blank character, and set
            // the column data ...
            if( *e1 ) e1++;
            *e1 = 0;

            colWidth[i] = collen;
            colName[i] = s;
        }
    }
}

void wxTabbedTextTable::GetRow( int nrow )
{
    if( ! ttsource ) return;
    if( currow == nrow ) return;

    StoreString( ttsource->GetRow(nrow) );

    char *c = buffer;
    for( int i = 0; i < ncol; i++ )
    {
        // Find first non-blank character

        while( *c == ' ' ) c++;

        char *s = c;
        char *e;
        char *e1;
        for( e = c, e1 = c; *e && *e != '\t'; e++ ) { if( *e != ' ') e1 = e; }
        c = e;
        if( *c ) c++;
        *e = 0;

        // Mark the character after the last non-blank character, and set
        // the column data ...
        if( *e1 ) e1++;
        *e1 = 0;

        rowData[i] = s;
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

void wxTabbedTextGrid::OnRangeSelect( wxGridRangeSelectEvent &event )
{
    if( ! event.Selecting() ) return;
    if( event.GetBottomRow() != event.GetTopRow() )
    {
        SelectRow( event.GetBottomRow() );
        return;
    }
    wxCommandEvent evt( WX_TTGRID_ROW_SELECTED, GetId() );
    evt.SetEventObject( this );
    evt.SetInt( event.GetBottomRow() );
    long keyState = 0;
    if( event.ShiftDown() ) keyState |= WX_TTGRID_SHIFTDOWN;
    if( event.ControlDown() ) keyState |= WX_TTGRID_CTRLDOWN;
    if( event.AltDown() ) keyState |= WX_TTGRID_ALTDOWN;
    evt.SetExtraLong( keyState );

    AddPendingEvent( evt );
}