#include "snapconfig.h"
// wxLayerKey: A wxGrid for displaying a symbology list
//
// Requires a Symbology defining the symbologies to display, "tickon" and "tickoff" icons for
// show/hide display.
//

#include "wxlayerkey.hpp"
#include "wxsimpleevent.hpp"
#include "wxpalettepopup.hpp"

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
    if( symkey && symkey && row >= 0 && row < symkey->LayerCount() )
    {
        LayerSymbology &sym = symkey->GetLayer( row );

        const wxBitmap *bmp = 0;
        if( col  == 0 && sym.HasColour() )
        {
            bmp = &(symkey->LayerBitmap(row));
        }
        else if ( col == 1 && sym.HasStatus() )
        {
            bmp = GetTickBitmap( symkey->ShowLayer(row) );
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
    // Set up new symbology and redraw..

    // TODO: Figure out how to handle this so that it doesn't screw up
    // rendering

    if( GetNumberRows() > 0 ) DeleteRows( 0, GetNumberRows() );

    if( symbologyKey->LayerCount() > 0 )
    {
        AppendRows( symbologyKey->LayerCount(), false );

        for( int i = 0; i < symbologyKey->LayerCount(); i++ )
        {
            // Attempt to reset everything before writing.
            // SetCellSize( i, 0, 1, 1 );
            SetReadOnly( i, 0, true );
            SetReadOnly( i, 1, true );
            SetReadOnly( i, 2, true );
            //SetCellRenderer( i, 0, new wxGridCellStringRenderer() );
            //SetCellRenderer( i, 1, new wxGridCellStringRenderer() );
            //SetCellRenderer( i, 2, new wxGridCellStringRenderer() );

            LayerSymbology &sym = symbologyKey->GetLayer( i );

            if( sym.HasStatus() || sym.HasColour() )
            {
                if( sym.HasColour() )
                {
                    SetCellRenderer( i, 0, new wxBitmapGridRenderer() );
                }
                if( sym.HasStatus() )
                {
                    SetCellRenderer( i, 1, new wxBitmapGridRenderer() );
                }
                SetCellValue( i, 2, sym.Name() );
                SetCellRenderer( i, 2, new wxGridCellStringRenderer() );
            }
            else
            {
                SetCellSize( i, 0, 1, 3 );
                SetCellValue(i, 0, sym.Name() );
                SetCellRenderer( i, 0, new wxGridCellStringRenderer() );
            }
        }
        AutoSizeColumns();
        SetColSize( 0, symbologyKey->PaletteBitmapSize() + 6 );
        SetColSize( 1, wxBitmapGridRenderer::TickBitmapSize() + 6 );
    }
}

Symbology *wxLayerKey::GetSymbologyKey()
{
    return symbologyKey;
}

void wxLayerKey::OnLeftClick( wxGridEvent &event )
{
    if( event.GetCol() == 0 )
    {
        LayerSymbology &sym = symbologyKey->GetLayer( event.GetRow() );
        if( sym.HasColour() )
        {
            int colourId = sym.ColourId();
            wxPalettePopup palettePopup( symbologyKey->GetPalette(), this );
            bool colourChanged = palettePopup.SelectColour( colourId );
            if( colourChanged )
            {
                sym.SetColourId( colourId );
                wxGridCellCoords cell( event.GetRow(), event.GetCol() );
                RefreshRect( BlockToDeviceRect( cell, cell) );
                FireSymbologyChangedEvent();
            }
        }
    }
    else if( event.GetCol() == 1 )
    {
        LayerSymbology &sym = symbologyKey->GetLayer( event.GetRow() );
        if( sym.HasStatus() )
        {
            sym.SetStatus( ! sym.Status() );
            wxGridCellCoords cell( event.GetRow(), event.GetCol() );
            RefreshRect( BlockToDeviceRect( cell, cell) );
            FireSymbologyChangedEvent();
        }
    }
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