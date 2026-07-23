#include "snapconfig.h"

#include "wxpalettepopup.hpp"

#include <wx/evtloop.h>

wxPalettePopup::wxPalettePopup( ColourPalette *thisPalette, wxWindow *parent, wxWindowID id ) :
    wxPopupTransientWindow( parent, wxBORDER_SIMPLE )
{
    SetId( id );
    SetExtraStyle( wxWS_EX_BLOCK_EVENTS );
    bitmapsize = 1;
    spacing = 3;
    ignoreNextButtonUp = false;
    SetPalette( thisPalette );
    highlightColour.Set( 255, 127, 0 );
}

wxPalettePopup::~wxPalettePopup()
{
}

BEGIN_EVENT_TABLE( wxPalettePopup, wxPopupTransientWindow )
    EVT_PAINT( wxPalettePopup::OnPaint )
    EVT_MOUSE_EVENTS( wxPalettePopup::OnMouseEvent )
    EVT_KEY_DOWN( wxPalettePopup::OnKeyDownEvent )
    EVT_MOUSE_CAPTURE_LOST( wxPalettePopup::OnCaptureLost )
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

    // Drawn explicitly rather than relying on the wxBORDER_SIMPLE window style: the popup can
    // sit right over the panel it opened from, and against a background the same colour, a
    // native border isn't always visible enough to tell the two apart.
    wxSize clientSize = GetClientSize();
    dc.SetPen( wxSystemSettings::GetColour( wxSYS_COLOUR_3DSHADOW ) );
    dc.SetBrush( *wxTRANSPARENT_BRUSH );
    dc.DrawRectangle( 0, 0, clientSize.GetWidth(), clientSize.GetHeight() );

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
        if( IgnoreAsOpeningClick() ) return;
        DismissAndNotify();
    }
}

void wxPalettePopup::OnKeyDownEvent( wxKeyEvent &event )
{
    if( event.GetKeyCode() == WXK_ESCAPE )
    {
        DismissAndNotify();
    }
}

void wxPalettePopup::OnCaptureLost( wxMouseCaptureLostEvent & WXUNUSED(event) )
{
    if( IgnoreAsOpeningClick() ) return;
    if( wxEventLoopBase::GetActive() ) wxEventLoopBase::GetActive()->Exit();
}

// True for exactly one event: either the button-up that ends the click that opened the popup
// (tracked via ignoreNextButtonUp, however long that click is held), or, as a secondary safety
// net, anything arriving in the first moment after showing, since capturing the mouse right
// after showing can itself generate a spurious, immediate capture-lost notification.
bool wxPalettePopup::IgnoreAsOpeningClick()
{
    bool ignore = ignoreNextButtonUp || ( wxGetLocalTimeMillis() - shownTime ) < 175;
    ignoreNextButtonUp = false;
    return ignore;
}

void wxPalettePopup::OnDismiss()
{
    if( wxEventLoopBase::GetActive() ) wxEventLoopBase::GetActive()->Exit();
}

// wxPopupTransientWindow isn't modal, so this blocks by running its own nested event loop
// instead of ShowModal().  Popup()/DismissAndNotify() (called from OnMouseEvent/OnKeyDownEvent,
// and automatically by wxPopupTransientWindow itself on an outside click or loss of focus) drive
// OnDismiss(), which is the single place that exits the loop.
bool wxPalettePopup::SelectColour( int &colourId )
{
    selectedColour = colourId;
    PositionWindow();
    // "this" is safe to capture here since we don't return from this function until the popup
    // is dismissed. We defer showing the popup (and capturing the mouse) until the next event
    // loop iteration, rather than doing it directly from within the click that's opening it.
    // We capture the mouse ourselves rather than relying only on wxPopupTransientWindow's own
    // handling: where the popup overlaps a sibling panel like the map view, a click there can
    // still be routed to that sibling by the OS's own hit-testing unless we hold the mouse the
    // whole time we're shown.
    CallAfter( [this]()
    {
        shownTime = wxGetLocalTimeMillis();
        ignoreNextButtonUp = wxGetMouseState().LeftIsDown();
        Popup();
        CaptureMouse();
    } );

    // Deliberately not disabling the rest of the app (e.g. via wxWindowDisabler) while this runs.
    // wxPopupTransientWindow dismisses itself when it loses activation to another top-level
    // window, on Windows this is implemented as a deactivation notification, and a disabled
    // window can't become active from a mouse click. Disabling the rest of the app would stop
    // the user's click from ever reaching it, so the popup would never see that deactivation and
    // would never dismiss on an outside click.
    wxEventLoop loop;
    loop.Run();

    if( HasCapture() ) ReleaseMouse();
    Hide();

    if( selectedColour < 0 ) selectedColour = colourId;
    bool colourChanged = selectedColour != colourId;
    colourId = selectedColour;
    return colourChanged;
}


