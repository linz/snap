#ifndef WXPALETTEPOPUP_HPP
#define WXPALETTEPOPUP_HPP

// wxPalettePopup: A popup window for displaying a palette of colours.  This is a modal popup window that allows
// for the selection of a colour.  It disappears on any mouse key event.  On a left mouse key event it selects the colour
// under the mouse, if any.
//
// Requires a Symbology defining the symbologies to display, "tickon" and "tickoff" icons for
// show/hide display.
//

#include "wx_includes.hpp"
#include "wxsymbology.hpp"

class wxPalettePopup : public wxDialog
{
public:
    wxPalettePopup( ColourPalette *palette, wxWindow* parent, wxWindowID id = wxID_ANY );
    ~wxPalettePopup();

    void SetPalette( ColourPalette *newPalette );
    ColourPalette *Palette() { return palette; }

    bool SelectColour( int &colourId );

private:
    wxPoint GetBitmapPosition( int colourId, bool centre );
    void GetHighlightRect( int colourId, wxRect &rect );
    int ColourIdFromPosition( const wxPoint &position );
    void LayoutPalette( int rows = 0 );
    void PositionWindow();
    void SetSelectedColour( int colourId );

    void OnPaint( wxPaintEvent &event );
    void OnMouseEvent( wxMouseEvent &event );
    void OnMouseCaptureLostEvent( wxMouseCaptureLostEvent &event );

    int nrow;
    int ncol;
    int bitmapsize;
    int spacing;
    int selectedColour;

    wxColour highlightColour;

    ColourPalette *palette;

    DECLARE_EVENT_TABLE();
};

#endif
