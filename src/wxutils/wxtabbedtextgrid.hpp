#ifndef WXTABBEDTEXTGRID_HPP
#define WXTABBEDTEXTGRID_HPP

// wxTabbedTextGrid, a window for displaying a grid of data supplied in tabbed text.
// Data comes from a wxTabbedTextSource object, which returns data strings separated by
// tab characters for each requested row.

#include "wx_includes.hpp"

// Abstract base class for a tabbed text data source ..

class wxTabbedTextSource
{
public:
    wxTabbedTextSource() {}
    virtual ~wxTabbedTextSource() {};

    // Get row headers as tab delimited string.  Labels prefixed with blank for right
    // justification.  Labels preceded or followed by blanks to fill to desired display
    // length (blank first character is ignored for character count).
    virtual char *GetHeader() = 0;

    // Get the number of rows in the table
    virtual int GetRowCount() = 0;

    // Get a tab delimited text string representing the data for the i'th row of the table.
    virtual char *GetRow( int i ) = 0;

    // Return the entire contents of the data source as a tab/new line delimited string
    wxString GetText();
};


// wxGridTableBase object used to interface between data source and the grid
//

class wxTabbedTextTable : public wxGridTableBase
{
public:
    wxTabbedTextTable();
    ~wxTabbedTextTable();
    int GetNumberRows();
    int GetNumberCols();

    int GetColWidth( int col );
    bool GetColRightJustify( int col );
    wxString GetColLabelValue( int col );
    wxString GetValue( int row, int col );
    void SetValue( int , int , const wxString&  );
    bool IsEmptyCell( int row, int col );
    void SetSource( wxTabbedTextSource *source );

    // Return the entire list as text ..
    wxString GetText();

private:
    void StoreString( char *string );
    void ClearSource();
    void GetRow( int row );

    int nrow;
    int ncol;
    int currow;
    int bufferlen;
    char *buffer;
    int *colWidth;
    bool *rightJustify;
    wxString *colName;
    char **rowData;

    wxTabbedTextSource *ttsource;
};

DECLARE_EVENT_TYPE( WX_TTGRID_ROW_SELECTED, -1 )

#define WX_TTGRID_SHIFTDOWN 1
#define WX_TTGRID_CTRLDOWN 2
#define WX_TTGRID_ALTDOWN 4

class wxTabbedTextGrid : public wxGrid
{
public:
    wxTabbedTextGrid();

    wxTabbedTextGrid(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize );

    ~wxTabbedTextGrid();

    void SetTabbedTextSource( wxTabbedTextSource *source );

    wxString GetText();



private:
    wxTabbedTextTable * table;
    void OnRangeSelect( wxGridRangeSelectEvent &event );

    DECLARE_DYNAMIC_CLASS( wxTabbedTextGrid );
    DECLARE_EVENT_TABLE();

};

#endif