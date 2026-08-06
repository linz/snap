#ifndef WXLAYERKEY_HPP
#define WXLAYERKEY_HPP

// wxLayerKey: A wxGrid for displaying a symbology list
//
// Requires a Symbology defining the symbologies to display, "tickon" and "tickoff" icons for
// show/hide display.
//

#include "wx_includes.hpp"
#include "wxsymbology.hpp"

#include <vector>

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

    // True if the layer at layerIndex is a control checkbox whose range has
    // exactly one child - the checkbox is then redundant, since it can only
    // ever match that one child's own status.
    bool IsSingleChildMasterRow( int layerIndex ) const;

    // Maps a grid row to the underlying layer index it currently displays, or
    // -1 if row is out of range. Every layer has its own row today, so this is
    // the identity mapping - it will diverge once a collapsed range hides some
    // layers from the grid.
    int RowToLayerIndex( int row ) const;
    // Maps a layer index to the grid row currently displaying it, or -1 if
    // that layer is hidden (e.g. inside a collapsed range).
    int LayerIndexToRow( int layerIndex ) const;

private:
    Symbology *symbologyKey;
    void FireSymbologyChangedEvent();

    // A control checkbox layer and the range of ordinary status layers below it
    // that it controls (inclusive). A symbology can contain more than one of
    // these (e.g. the Data type list and the Data file/Residual/Redundancy list
    // can both show a header at once), so every one found is kept, not just the
    // first.
    struct MasterRange
    {
        int master;
        int lastChild;
    };
    // Recomputed by SetSymbology(), not on every click.
    std::vector<MasterRange> masterRanges;

    // Grid row -> layer index, in display order (see RowToLayerIndex).
    // Recomputed by SetSymbology().
    std::vector<int> visibleRows;

    // Result of looking up which masterRanges entry a layer index belongs to,
    // as either the master itself or one of its children. rangeIndex is -1 if
    // the layer is not part of any range, in which case isMaster is meaningless.
    struct MasterLookup
    {
        int rangeIndex;
        bool isMaster;
    };
    MasterLookup FindMasterRange( int layerIndex ) const;

    DECLARE_DYNAMIC_CLASS( wxLayerKey );
    DECLARE_EVENT_TABLE();

};

#endif
