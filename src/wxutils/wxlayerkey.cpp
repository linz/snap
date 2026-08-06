#include "snapconfig.h"
// wxLayerKey: A wxGrid for displaying a symbology list
//
// Requires a Symbology defining the symbologies to display, "tickon" and "tickoff" icons for
// show/hide display.
//

#include "wxlayerkey.hpp"
#include "wxsimpleevent.hpp"
#include "wxpalettepopup.hpp"
#include <wx/renderer.h>
#include <limits>

DEFINE_EVENT_TYPE(WX_SYMBOLOGY_CHANGED)

// Class used to render symbology bitmaps onto a grid cell..

class wxBitmapGridRenderer : public wxGridCellRenderer
{
public:
    virtual void Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, const wxRect& rect, int row, int col, bool isSelected);
    virtual wxSize GetBestSize(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, int row, int col);
    virtual wxGridCellRenderer* Clone() const;
    static int TickBitmapSize() { return tickBitmapSize; }
private:
    static wxBitmap *GetTickBitmap( bool on );
    static void CreateTickBitmaps();

    static int margin;
    static int tickBitmapSize;
    static wxBitmap *bmpTickOn;
    static wxBitmap *bmpTickOff;
};

int wxBitmapGridRenderer::margin = 2;
int wxBitmapGridRenderer::tickBitmapSize = 13;
wxBitmap *wxBitmapGridRenderer::bmpTickOff = 0;
wxBitmap *wxBitmapGridRenderer::bmpTickOn = 0;

void wxBitmapGridRenderer::CreateTickBitmaps()
{
    wxBitmap *bitmap = new wxBitmap( tickBitmapSize, tickBitmapSize, -1 );

    wxMemoryDC dc;

    dc.SelectObject( *bitmap );
    int width = bitmap->GetWidth();
    int height = bitmap->GetHeight();

    dc.SetBrush( *wxWHITE );
    dc.SetPen(  *wxWHITE );
    dc.DrawRectangle( 0, 0, width, height );
    dc.SetPen( wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW) );
    dc.DrawLine(0,width-2,0,0);
    dc.DrawLine(0,0,width-1,0);
    dc.SetPen( wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW) );
    dc.DrawLine(1,height-3,1,1);
    dc.DrawLine(1,1,height-2,1);
    dc.SetPen( wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE) );
    dc.DrawLine( 1, height-2, width-2, height-2 );
    dc.DrawLine( width-2, width-2, height-2, 0 );

    dc.SelectObject(wxNullBitmap);

    bmpTickOff = bitmap;

    bitmap = new wxBitmap( bitmap->GetSubBitmap( wxRect( 0, 0, width, height ) ) );

    dc.SelectObject( *bitmap );

    dc.SetPen( *wxBLACK );
    int tx0 = 3;
    int ty0 = height - 6;
    int tx1 = 5;
    int ty1 = height - 4;
    int tx2 = width - 3;
    int ty2 = 4;

    dc.DrawLine( tx0, ty0, tx1, ty1 );
    dc.DrawLine( tx1, ty1, tx2, ty2 );
    dc.DrawLine( tx0, ty0-1, tx1, ty1-1 );
    dc.DrawLine( tx1, ty1-1, tx2, ty2-1 );
    dc.DrawLine( tx0, ty0-2, tx1, ty1-2 );
    dc.DrawLine( tx1, ty1-2, tx2, ty2-2 );

    dc.SelectObject(wxNullBitmap);

    bmpTickOn = bitmap;
}


wxBitmap *wxBitmapGridRenderer::GetTickBitmap( bool on )
{
    if( ! bmpTickOn )
    {
        CreateTickBitmaps();
    }
    return on ? bmpTickOn : bmpTickOff;
}

void wxBitmapGridRenderer::Draw( wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, const wxRect& rect, int row, int col, bool isSelected )
{
    // Fill the rectangle and set up the DC..

    wxGridCellRenderer::Draw( grid, attr, dc, rect, row, col, isSelected );

    wxLayerKey &wxsymkey = dynamic_cast<wxLayerKey &> (grid);
    Symbology *symkey = wxsymkey.GetSymbologyKey();
    const int layerIndex = wxsymkey.RowToLayerIndex( row );
    if( symkey && layerIndex >= 0 && layerIndex < symkey->LayerCount() )
    {
        LayerSymbology &sym = symkey->GetLayer( layerIndex );

        if( col == 1 && sym.IsControlCheckbox() ) {
            if( ! wxsymkey.IsSingleChildMasterRow( layerIndex ) ) {
                const int flags = sym.IsMixedRowStatus() ? wxCONTROL_UNDETERMINED :
                                   sym.Status() ? wxCONTROL_CHECKED : wxCONTROL_NONE;
                wxRendererNative::Get().DrawCheckBox( &grid, dc, rect, flags );
            }
            return;
        }

        // A master row never has a colour swatch (see init_layer()'s title
        // case), so column 0 is otherwise blank there - use it for the
        // disclosure triangle instead of widening column 1. Every master row
        // is collapsible, even a single-child one that draws no checkbox.
        if( col == 0 && sym.IsControlCheckbox() ) {
            const int flags = wxsymkey.IsRangeCollapsed( layerIndex ) ? 0 : wxCONTROL_EXPANDED;
            wxRendererNative::Get().DrawTreeItemButton( &grid, dc, rect, flags );
            return;
        }

        const wxBitmap *bmp = 0;
        if( col  == 0 && sym.HasColour() && sym.ColourEditable() )
        {
            bmp = &(symkey->LayerBitmap(layerIndex));
        }
        else if ( col == 1 && sym.HasStatus() )
        {
            bmp = GetTickBitmap( symkey->ShowLayer(layerIndex) );
        }

        if( bmp )
        {
            int left = (rect.GetWidth() - bmp->GetWidth())/2;
            if( left < 0 ) left = 0;
            left += rect.GetLeft();

            int top = (rect.GetHeight() - bmp->GetHeight())/2;
            if( top < 0 ) top = 0;
            top += rect.GetTop();

            dc.DrawBitmap( *bmp, left, top, false );
        }
    }
}

wxSize wxBitmapGridRenderer::GetBestSize(wxGrid& grid, wxGridCellAttr& WXUNUSED(attr), wxDC&  WXUNUSED(dc), int WXUNUSED(row), int WXUNUSED(col) )
{
    wxLayerKey &wxsymkey = dynamic_cast<wxLayerKey &>(grid);
    Symbology *symkey = wxsymkey.GetSymbologyKey();
    int size = 10;
    if( symkey )
    {
        size = symkey->PaletteBitmapSize();
    }
    return wxSize( size + margin*2, size+margin*2 );
}

wxGridCellRenderer* wxBitmapGridRenderer::Clone() const
{
    return new wxBitmapGridRenderer();
}

// Class defining a grid for displaying a symbology

IMPLEMENT_DYNAMIC_CLASS( wxLayerKey, wxGrid );

BEGIN_EVENT_TABLE(wxLayerKey, wxGrid)
    EVT_GRID_SELECT_CELL( wxLayerKey::OnSelectCell )
    EVT_GRID_CELL_LEFT_CLICK( wxLayerKey::OnLeftClick )
END_EVENT_TABLE()

wxLayerKey::wxLayerKey()
{
}

wxLayerKey::wxLayerKey(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size ) :
    wxGrid( parent, id, pos, size )
{
    long style = GetWindowStyleFlag();
    style &= ! wxHSCROLL;
    SetWindowStyleFlag( style );
    CreateGrid( 0, 3, wxGrid::wxGridSelectRows);
    SetDefaultCellBackgroundColour( wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE) );
    SetDefaultCellTextColour( wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT) );
    SetGridLineColour( wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT) );
    SetSelectionBackground( wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE) );
    SetSelectionForeground( wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT) );
    SetCellHighlightPenWidth(0);
    // Every cell in this grid is read-only (see SetReadOnly() below), and
    // wxGrid draws the current cell's border with this separate setting when
    // read-only, not the one above.
    SetCellHighlightROPenWidth(0);
    EnableGridLines( false );
    EnableEditing( false );
    DisableDragGridSize();

    SetSelectionMode(wxGrid::wxGridSelectRows);
    SetRowLabelSize(0);
    SetColLabelSize(0);
}

wxLayerKey::~wxLayerKey()
{
}


void wxLayerKey::SetSymbology( Symbology *newSymbologyKey )
{
    symbologyKey = newSymbologyKey;

    masterRanges.clear();
    visibleRows.clear();

    if( symbologyKey->LayerCount() > 0 )
    {
        // Locate every control checkbox row and the range of ordinary status rows
        // each controls, once here rather than on every click. A symbology can
        // contain more than one (two colour-coding lists can both show a header
        // at once), so a non-status row such as a spacer only ends the range
        // currently being extended - it doesn't stop the search for further
        // control checkbox rows after it.
        int current = -1;
        for( int i = 0; i < symbologyKey->LayerCount(); i++ ) {
            LayerSymbology &sym = symbologyKey->GetLayer( i );
            if( sym.IsControlCheckbox() ) {
                const auto saved = collapsedByName.find( sym.Name() );
                const bool collapsed = saved != collapsedByName.end() && saved->second;
                masterRanges.push_back( MasterRange{ i, i, collapsed } );
                current = (int) masterRanges.size() - 1;
                continue;
            }
            if( current < 0 ) {
                continue;
            }
            if( ! sym.HasStatus() ) {
                current = -1;
                continue;
            }
            masterRanges[current].lastChild = i;
        }

        RecomputeVisibleRows();
    }

    RebuildRows();
}

void wxLayerKey::RecomputeVisibleRows()
{
    visibleRows.clear();
    for( int layerIndex = 0; layerIndex < symbologyKey->LayerCount(); layerIndex++ ) {
        const MasterLookup lookup = FindMasterRange( layerIndex );
        if( ! lookup.isMaster && lookup.rangeIndex >= 0 && masterRanges[lookup.rangeIndex].collapsed ) {
            continue;
        }
        visibleRows.push_back( layerIndex );
    }
}

void wxLayerKey::RebuildRows()
{
    // TODO: Figure out how to handle this so that it doesn't screw up
    // rendering

    if( GetNumberRows() > 0 ) DeleteRows( 0, GetNumberRows() );

    if( visibleRows.empty() ) {
        return;
    }

    AppendRows( static_cast<int>( visibleRows.size() ), false );

    for( size_t row = 0; row < visibleRows.size(); row++ )
    {
        const int gridRow = static_cast<int>( row );
        const int layerIndex = visibleRows[row];

        // Attempt to reset everything before writing.
        // SetCellSize( gridRow, 0, 1, 1 );
        SetReadOnly( gridRow, 0, true );
        SetReadOnly( gridRow, 1, true );
        SetReadOnly( gridRow, 2, true );
        //SetCellRenderer( gridRow, 0, new wxGridCellStringRenderer() );
        //SetCellRenderer( gridRow, 1, new wxGridCellStringRenderer() );
        //SetCellRenderer( gridRow, 2, new wxGridCellStringRenderer() );

        LayerSymbology &sym = symbologyKey->GetLayer( layerIndex );

        if( sym.HasStatus() || sym.HasColour() )
        {
            // A master row has neither colour nor status of its own to show
            // in column 0, but still needs this renderer - it draws that
            // row's disclosure triangle.
            if( sym.HasColour() || sym.IsControlCheckbox() )
            {
                SetCellRenderer( gridRow, 0, new wxBitmapGridRenderer() );
            }
            if( sym.HasStatus() )
            {
                SetCellRenderer( gridRow, 1, new wxBitmapGridRenderer() );
            }
            SetCellValue( gridRow, 2, sym.Name() );
            SetCellRenderer( gridRow, 2, new wxGridCellStringRenderer() );
        }
        else
        {
            SetCellSize( gridRow, 0, 1, 3 );
            SetCellValue( gridRow, 0, sym.Name() );
            SetCellRenderer( gridRow, 0, new wxGridCellStringRenderer() );
        }
    }
    AutoSizeColumns();
    SetColSize( 0, symbologyKey->PaletteBitmapSize() + 6 );
    SetColSize( 1, wxBitmapGridRenderer::TickBitmapSize() + 6 );
}

void wxLayerKey::ToggleRangeCollapsed( const int layerIndex )
{
    const MasterLookup lookup = FindMasterRange( layerIndex );
    if( ! lookup.isMaster ) {
        return;
    }
    MasterRange &range = masterRanges[lookup.rangeIndex];
    range.collapsed = ! range.collapsed;
    collapsedByName[ symbologyKey->GetLayer( range.master ).Name() ] = range.collapsed;
    RecomputeVisibleRows();
    RebuildRows();
}

Symbology *wxLayerKey::GetSymbologyKey()
{
    return symbologyKey;
}

bool wxLayerKey::IsSingleChildMasterRow( const int layerIndex ) const
{
    const MasterLookup lookup = FindMasterRange( layerIndex );
    if( ! lookup.isMaster ) {
        return false;
    }
    const MasterRange &range = masterRanges[lookup.rangeIndex];
    return range.lastChild == range.master + 1;
}

int wxLayerKey::RowToLayerIndex( const int row ) const
{
    if( row < 0 || static_cast<size_t>( row ) >= visibleRows.size() ) {
        return -1;
    }
    return visibleRows[row];
}

int wxLayerKey::LayerIndexToRow( const int layerIndex ) const
{
    for( size_t row = 0; row < visibleRows.size(); row++ ) {
        if( visibleRows[row] != layerIndex ) {
            continue;
        }
        if( row > static_cast<size_t>( std::numeric_limits<int>::max() ) ) {
            return -1;
        }
        return static_cast<int>( row );
    }
    return -1;
}

bool wxLayerKey::IsRangeCollapsed( const int layerIndex ) const
{
    const MasterLookup lookup = FindMasterRange( layerIndex );
    if( ! lookup.isMaster ) {
        return false;
    }
    return masterRanges[lookup.rangeIndex].collapsed;
}

void wxLayerKey::OnLeftClick( wxGridEvent &event )
{
    const int row = event.GetRow();
    const int layerIndex = RowToLayerIndex( row );
    if( event.GetCol() == 0 )
    {
        LayerSymbology &sym = symbologyKey->GetLayer( layerIndex );
        const MasterLookup lookup = FindMasterRange( layerIndex );
        if( lookup.isMaster ) {
            ToggleRangeCollapsed( layerIndex );
            return;
        }
        if( sym.HasColour() && sym.ColourEditable() )
        {
            int colourId = sym.ColourId();
            wxPalettePopup palettePopup( symbologyKey->GetPalette(), this );
            bool colourChanged = palettePopup.SelectColour( colourId );
            if( colourChanged )
            {
                sym.SetColourId( colourId );
                wxGridCellCoords cell( row, event.GetCol() );
                RefreshRect( BlockToDeviceRect( cell, cell) );
                FireSymbologyChangedEvent();
            }
        }
    }
    else if( event.GetCol() == 1 ) {
        LayerSymbology &sym = symbologyKey->GetLayer( layerIndex );
        const MasterLookup lookup = FindMasterRange( layerIndex );
        if( lookup.isMaster && IsSingleChildMasterRow( layerIndex ) ) {
            // No checkbox is drawn for a single-child master, so clicking it is a no-op.
            return;
        }
        if( lookup.isMaster ) {
            // Clicking the master control while it's indeterminate or unchecked
            // selects every child row below it; clicking it while checked deselects
            // them all. The master itself can never end up indeterminate directly
            // from its own click - only from an individual child edit.
            const MasterRange &range = masterRanges[lookup.rangeIndex];
            const bool checkAll = sym.IsMixedRowStatus() || ! sym.Status();
            for( int i = range.master+1; i <= range.lastChild; i++ ) {
                symbologyKey->GetLayer( i ).SetStatus( checkAll );
            }
            sym.SetStatus( checkAll );
            sym.SetMixedRowStatus( false );
            // A collapsed range hides its children from the grid entirely, so
            // there is nothing below the master row itself to refresh.
            const int masterRow = LayerIndexToRow( range.master );
            const int bottomRow = range.collapsed ? masterRow : LayerIndexToRow( range.lastChild );
            wxGridCellCoords topLeft( masterRow, event.GetCol() );
            wxGridCellCoords bottomRight( bottomRow, event.GetCol() );
            RefreshRect( BlockToDeviceRect( topLeft, bottomRight ) );
            FireSymbologyChangedEvent();
        } else if( sym.HasStatus() ) {
            sym.SetStatus( ! sym.Status() );
            wxGridCellCoords cell( row, event.GetCol() );
            RefreshRect( BlockToDeviceRect( cell, cell) );
            // Any time a single child row is clicked, the master control that owns
            // it turns indeterminate to show its children no longer agree, no
            // matter which way that child was toggled or what state the master was
            // in before. It stays indeterminate until the user clicks the master
            // control directly.
            if( lookup.rangeIndex >= 0 ) {
                const int masterLayerIndex = masterRanges[lookup.rangeIndex].master;
                symbologyKey->GetLayer( masterLayerIndex ).SetMixedRowStatus( true );
                const int masterRow = LayerIndexToRow( masterLayerIndex );
                wxGridCellCoords masterCell( masterRow, event.GetCol() );
                RefreshRect( BlockToDeviceRect( masterCell, masterCell ) );
            }
            FireSymbologyChangedEvent();
        }
    }
}

wxLayerKey::MasterLookup wxLayerKey::FindMasterRange( const int layerIndex ) const
{
    for( size_t r = 0; r < masterRanges.size(); r++ ) {
        const int rangeIndex = r <= static_cast<size_t>( std::numeric_limits<int>::max() ) ? static_cast<int>( r ) : -1;
        if( masterRanges[r].master == layerIndex ) {
            return MasterLookup{ rangeIndex, true };
        }
        if( layerIndex > masterRanges[r].master && layerIndex <= masterRanges[r].lastChild ) {
            return MasterLookup{ rangeIndex, false };
        }
    }
    return MasterLookup{ -1, false };
}

// Crude mechanism to disable cell selection (for visual effect).

void wxLayerKey::OnSelectCell( wxGridEvent & WXUNUSED(event) )
{
//	ClearSelection();
}

void wxLayerKey::FireSymbologyChangedEvent()
{
    wxSimpleEvent evt(WX_SYMBOLOGY_CHANGED, true);
    AddPendingEvent(evt);
}