#ifndef WXPALETTEPOPUP_HPP
#define WXPALETTEPOPUP_HPP

// wxPalettePopup: A popup window for displaying a palette of colours, allowing the selection
// of a colour.  It closes on any mouse button event.  On a left mouse button event it selects
// the colour under the mouse, if any.
//
// This is built on wxPopupTransientWindow (a wxPopupWindow subclass) rather than wxDialog.
// wxDialog::Move()/SetPosition() have no effect on the final on-screen position in this
// environment: moving before showing, moving again after showing, deferring the move to the
// next event loop iteration, and Center() followed by Move() were all tried and all failed
// identically, with the window manager always recentering the dialog on its parent regardless.
// wxPopupWindow is an override-redirect top-level window, the same kind used for menus and
// combo popups, and it honours Move()/Position() reliably.
//
// wxPopupTransientWindow specifically (rather than plain wxPopupWindow) is used because it
// already implements "close when the user loses focus to a different application", which
// losing keyboard focus alone doesn't reliably give us for this kind of override-redirect
// window. wxPopupTransientWindow is the same tested mechanism wxComboBox-style dropdown popups
// rely on for that.
//
// We still capture the mouse ourselves (CaptureMouse()) rather than leaving all dismissal to
// wxPopupTransientWindow: its own "click outside the popup" handling is activation-based, and
// where the popup overlaps a sibling panel like the map view, the OS's own hit-testing can still
// route a click there instead of to us unless we hold the mouse for as long as we're shown.
//
// The click that opens the popup is still in progress (button still down) at the point it
// opens, since it's the very click wxLayerKey::OnLeftClick is handling. Its eventual button-up
// (or, once we capture the mouse, sometimes a capture-lost notification instead) reaches the
// popup regardless of how long the button is held, on both GTK and native Windows.
// ignoreNextButtonUp records whether the button was already down when we were shown, so
// OnMouseEvent()/OnCaptureLost() can recognise and ignore specifically that one event, rather
// than an arbitrary time window that a slower, deliberate press could simply outlast.
//
// Since none of this is modal, SelectColour() runs its own nested event loop instead of using
// ShowModal()/EndModal(), exited via OnDismiss().
//

#include "wx_includes.hpp"
#include "wxsymbology.hpp"

#include <wx/popupwin.h>

class wxPalettePopup : public wxPopupTransientWindow
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
    bool IgnoreAsOpeningClick();

    void OnPaint( wxPaintEvent &event );
    void OnMouseEvent( wxMouseEvent &event );
    void OnKeyDownEvent( wxKeyEvent &event );
    void OnCaptureLost( wxMouseCaptureLostEvent &event );
    void OnDismiss() wxOVERRIDE;

    int nrow;
    int ncol;
    int bitmapsize;
    int spacing;
    int selectedColour;

    wxLongLong shownTime;
    bool ignoreNextButtonUp;

    wxColour highlightColour;

    ColourPalette *palette;

    DECLARE_EVENT_TABLE();
};

#endif
