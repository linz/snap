#ifndef WXLAYERKEY_HPP
#define WXLAYERKEY_HPP

// wxLayerKey: A wxGrid for displaying a symbology list
//
// Requires a Symbology defining the symbologies to display, "tickon" and "tickoff" icons for
// show/hide display.
//

#include "wx_includes.hpp"
#include "wxsymbology.hpp"

DECLARE_EVENT_TYPE(WX_SYMBOLOGY_CHANGED, -1)

class wxLayerKey : public wxGrid
{
public:
    wxLayerKey();
    wxLayerKey(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize );

    ~wxLayerKey();

    void SetSymbology( Symbology *newSymbologyKey );
    void OnLeftClick( wxGridEvent &event );
    void OnSelectCell( wxGridEvent &event );

    Symbology *GetSymbologyKey();

private:
    Symbology *symbologyKey;
    void FireSymbologyChangedEvent();

    DECLARE_DYNAMIC_CLASS( wxLayerKey );
    DECLARE_EVENT_TABLE();

};

#endif