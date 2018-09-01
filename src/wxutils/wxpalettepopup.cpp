#include "snapconfig.h"

#include "wxpalettepopup.hpp"

wxPalettePopup::wxPalettePopup( ColourPalette *thisPalette, wxWindow *parent, wxWindowID id ) :
    wxDialog( parent, id, "", wxDefaultPosition, wxDefaultSize, wxSIMPLE_BORDER )
{
    SetExtraStyle( wxWS_EX_BLOCK_EVENTS );
    bitmapsize = 1;
    spacing = 3;
    SetPalette( thisPalette );
    highlightColour.Set( 255, 127, 0 );
}

wxPalettePopup::~wxPalettePopup()
{
}

BEGIN_EVENT_TABLE( wxPalettePopup, wxWindow )
    EVT_PAINT( wxPalettePopup::OnPaint )
    EVT_MOUSE_EVENTS( wxPalettePopup::OnMouseEvent )
    EVT_KEY_DOWN( wxPalettePopup::OnKeyDownEvent )
END_EVENT_TABLE()

void wxPalettePopup::SetPalette(ColourPalette *newPalette)
{
    palette = newPalette;
    bitmapsize = palette ? palette->PaletteBitmapSize() : 0;
    LayoutPalette();
}

void wxPalettePopup::LayoutPalette( int rows )
{
    int nPalette = palette ? palette->Size() : 0;
    if( nPalette <= 0 ) return;

    if( rows == 0 )
    {
        while( rows*rows < nPalette ) rows++;
        if( rows > 1 ) rows--;
    }
    nrow = rows;
    ncol = ((nPalette-1)/nrow)+1;

    int width = ncol * (bitmapsize + spacing) + spacing;
    int height = nrow * (bitmapsize + spacing) + spacing;
    SetClientSize( width, height );
}

wxPoint wxPalettePopup::GetBitmapPosition( int colourId, bool centre )
{
    int row = colourId / ncol;
    int col = colourId - row * ncol;
    int offset = centre ? bitmapsize/2 : 0;
    offset += spacing;
    wxPoint pt( (bitmapsize + spacing)*col + offset, (bitmapsize + spacing)*row + offset );
    return pt;
}

int wxPalettePopup::ColourIdFromPosition( const wxPoint &position )
{
    int colpx = position.x - spacing;
    int rowpx = position.y - spacing;
    int gridsize = bitmapsize + spacing;
    int col = colpx / gridsize;
    int row = rowpx / gridsize;

    if( col < 0 || col >= ncol || row < 0 || row >= nrow ) return -1;
    if( colpx - col*gridsize >= bitmapsize ) return -1;
    if( rowpx - row*gridsize >= bitmapsize ) return -1;

    int colourId = row * ncol + col;
    if( colourId >= palette->Size() ) return -1;
    return colourId;
}

void wxPalettePopup::SetSelectedColour( int colourId )
{
    if( colourId != selectedColour )
    {
        wxRect refreshRect;
        if( selectedColour >= 0 )
        {
            GetHighlightRect( selectedColour, refreshRect );
            RefreshRect( refreshRect, false );
        }
        selectedColour = colourId;
        if( selectedColour >= 0 )
        {
            GetHighlightRect( selectedColour, refreshRect );
            RefreshRect( refreshRect, false );
        }
    }
}


void wxPalettePopup::PositionWindow()
{
    wxPoint mousePosition = wxGetMousePosition();
    wxSize size = GetSize();

    wxPoint bitmapPosition;
    if( selectedColour >= 0 )
    {
        bitmapPosition = GetBitmapPosition( selectedColour, true );
    }
    else
    {
        bitmapPosition = wxPoint( size.GetWidth()/2, size.GetHeight()/2 );
    }
    bitmapPosition = ClientToScreen( bitmapPosition );

    wxPoint windowPosition = GetPosition();

    wxRect topWindowRect = wxTheApp->GetTopWindow()->GetScreenRect();

    int x = windowPosition.x + mousePosition.x - bitmapPosition.x;
    int y = windowPosition.y + mousePosition.y - bitmapPosition.y;

    if( x + size.GetWidth() > topWindowRect.GetRight() )
    {
        x = topWindowRect.GetRight() - size.GetWidth();
    }

    if( y + size.GetHeight() > topWindowRect.GetBottom() )
    {
        y = topWindowRect.GetBottom() - size.GetHeight();
    }
    if( x < topWindowRect.GetLeft() ) x = topWindowRect.GetLeft();
    if( y < topWindowRect.GetTop() ) y = topWindowRect.GetTop();

    Move( x, y );
}

void wxPalettePopup::GetHighlightRect( int colourId, wxRect &rect )
{
    rect = wxRect(GetBitmapPosition( colourId, false ), wxSize( bitmapsize, bitmapsize ) );
    rect.Inflate( 1, 1 );
}

void wxPalettePopup::OnPaint( wxPaintEvent & WXUNUSED(event) )
{
    wxPaintDC dc(this);
    dc.Clear();
    if( palette )
    {
        for( int i = 0; i < palette->Size(); i++ )
        {
            if( i == selectedColour )
            {
                wxRect highlightRect;
                GetHighlightRect( i, highlightRect );
                dc.SetPen( highlightColour );
                dc.SetBrush( highlightColour );
                dc.DrawRectangle( highlightRect.GetPosition(), highlightRect.GetSize() );
            }
            wxPoint pt = GetBitmapPosition( i, false );
            const wxBitmap &bitmap = palette->Bitmap( i );
            dc.DrawBitmap( bitmap, pt.x, pt.y, false );
        }
    }
}

void wxPalettePopup::OnMouseEvent( wxMouseEvent &event )
{
    if( event.LeftDown() || event.LeftUp() )
    {
        int newColourId = ColourIdFromPosition( event.GetPosition() );
        if( newColourId >= 0 ) { SetSelectedColour( newColourId ); }
    }

    if( event.ButtonDown() || event.ButtonUp() )
    {
        EndModal(0);
    }
}

void wxPalettePopup::OnKeyDownEvent( wxKeyEvent &event )
{
    if( event.GetKeyCode() == WXK_ESCAPE )
    {
        EndModal(0);
    }
}

bool wxPalettePopup::SelectColour( int &colourId )
{
    selectedColour = colourId;
    PositionWindow();
    ShowModal();
    if( selectedColour < 0 ) selectedColour = colourId;
    bool colourChanged = selectedColour != colourId;
    colourId = selectedColour;
    return colourChanged;
}


