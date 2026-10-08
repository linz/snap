#include "snapconfig.h"
#include "snapplot_layers.hpp"
#include "wxsymbology.hpp"

#include "util/snapctype.h"
#include "util/fieldscanner.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/iterator_range.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Provides a crude interface to existing C code by replacing functions
// defined in plotpens.h

//extern "C" {
#include "plotpens.h"
#include "plotconn.h"
#include "snap/snapglob.h"
#include "snap/survfile.h"
#include "snapdata/datatype.h"
#include "snap/stnadj.h"
#include "plotstns.h"
#include "backgrnd.h"
#include "util/classify.h"
#include "util/dstring.h"
#include "util/errdef.h"
//}

// Null terminated list of default palette colours ..

static constexpr std::array<std::string_view,14> defaultPalette =
{
    "BLACK",
    "GREY",
    "LIGHT GREY",
    "WHITE",
    "BLUE",
    "CYAN",
    "SEA GREEN",
    "GREEN",
    "YELLOW",
    "CORAL",
    "ORANGE",
    "RED",
    "PURPLE",
    "BROWN"
};

#define UNUSED_LAYER_PEN_ID -2
#define UNUSED_PEN_ID -1
#define UNUSED_OPT_ID -1

static constexpr std::string_view dflt_stn_colour = "RED";
static constexpr std::string_view dflt_data_colour = "BLUE";
static constexpr std::string_view dflt_background_colour = "LIGHT GREY";

/// The text with its first letter in upper case.
static std::string capitalised( const std::string_view text )
{
    std::string result( text );
    const auto first = result.begin();
    auto firstLetter = boost::make_iterator_range( first, result.empty() ? first : first + 1 );
    boost::algorithm::to_upper( firstLetter );
    return result;
}

struct layer_s
{
    /// A row of one of the fixed tables, with its own pen and option ids.
    layer_s(
        const std::string_view name,
        const int pen_id,
        const int opt_id,
        const std::string_view dfltColour, ///< a literal, empty if the row has no colour
        const bool dfltOption,
        const bool need_hor,
        const bool need_vrt,
        const bool need_cvr,
        const bool is_control_checkbox = false ) :
        name( name ),
        pen_id( pen_id ),
        opt_id( opt_id ),
        dfltColour( dfltColour ),
        dfltOption( dfltOption ),
        need_hor( need_hor ),
        need_vrt( need_vrt ),
        need_cvr( need_cvr ),
        is_control_checkbox( is_control_checkbox )
    {
    }

    /// A row built while snapplot runs, named after the data it stands for,
    /// with the first letter of its name capitalised.
    layer_s(
        const std::string_view name,
        const std::string_view dfltColour, ///< a literal
        const bool isHeader ) ///< true for the row that heads a list and controls the rows below it
        : name( capitalised( name ) ),
        pen_id( isHeader ? UNUSED_PEN_ID : OTHER_PEN ),
        opt_id( OTHER_OPT ),
        dfltColour( dfltColour ),
        dfltOption( true ),
        is_control_checkbox( isHeader )
    {
    }

    const std::string name;
    int  pen_id = UNUSED_PEN_ID;
    const int opt_id = UNUSED_OPT_ID;
    const std::string_view dfltColour;
    const bool dfltOption = false;
    const bool need_hor = false;
    const bool need_vrt = false;
    const bool need_cvr = false;
    int lyr_id = -1;
    // true if this row's checkbox is to control a following range of
    // other rows' status, instead of its own
    const bool is_control_checkbox = false;
    // Last known live status for this row - and colour if this row has one -
    // saved by save_and_invalidate_layer_state() before this list is
    // repointed elsewhere or freed and rebuilt. savedColour being empty
    // means either it's never been saved, or this row has no colour at all -
    // restore_layer_state() leaves such rows at their fresh construction-time
    // colour default.
    bool savedStatus = true;
    std::optional<wxColour> savedColour;
};

/// A list of rows in the order they are added to the symbology, indexed from 0.
///
/// The lists built from data (a station classification, a colour-by mode,
/// data types, data files) have a header row at index 0, so the row for item
/// i of that data, counting from 0, is at index i+1. The lookups
/// station_class_pen, data_pen, datatype_selected, filetype_selected and
/// classification_value_selected take such an item number and add the 1.
/// background_layers has no header row; the background layer numbered n, which
/// counts from 1, is at index n-1. station_layers and the other fixed tables
/// are in the order written, and station_layers has a second control row
/// partway down, so it is not indexed by item number.
using layer_list = std::vector<layer_s>;

// Non-owning: points at an entry of station_class_layer_cache, or null if no
// classification is active.
static layer_list *station_user_layers = nullptr;

static constexpr std::array<int,5> station_layers_pens = {FREE_STN_PEN,FREE_STN_PEN,HOR_FIXED_STN_PEN,HOR_FIXED_STN_PEN,REJECTED_STN_PEN};

static layer_list station_layers =
{
    {"Station types",UNUSED_PEN_ID,OTHER_OPT,{},true,false,false,false,true},
    {"Free stations",FREE_STN_PEN,FREE_STN_OPT,"GREY",true,false,false,false},
    {"Fixed stations",FIXED_STN_PEN,FIXED_STN_OPT,"RED",true,true,true,false},
    {"Hor fixed stns",HOR_FIXED_STN_PEN,HOR_FIXED_STN_OPT,"RED",true,true,false,false},
    {"Vrt fixed stns",VRT_FIXED_STN_PEN,VRT_FIXED_STN_OPT,"RED",true,false,true,false},
    {"Rejected stns",REJECTED_STN_PEN,REJECTED_STN_OPT,"WHITE",true,false,false,false},
    {"Symbol",UNUSED_PEN_ID,SYMBOL_OPT,{},true,false,false,false},
    {"Name",UNUSED_PEN_ID, NAME_OPT,{},false,false,false,false},
    {"Code",UNUSED_PEN_ID, CODE_OPT,{},false,false,false,false},
    {"",UNUSED_PEN_ID,UNUSED_OPT_ID,{},false,false,false,false},
    {"Station metrics",UNUSED_PEN_ID,OTHER_OPT,{},true,false,false,false,true},
    {"Error ellipses",ELLIPSE_PEN,ELLIPSE_OPT,"SEA GREEN",true,true,false,true},
    {"Relative ellipse",REL_ELL_PEN, REL_ELL_OPT,"SEA GREEN",false,true,false,true},
    {"Hor adjustment",HOR_ADJ_PEN,HOR_ADJ_OPT,"RED",false,true,false,true},
    {"Height error",HGTERR_PEN,HGTERR_OPT,"SEA GREEN",false,false,true,true},
    {"Relative hgt err", REL_HGT_PEN, REL_HGT_OPT,"SEA GREEN",false,false,true,true},
    {"Vrt adjustment",HGT_ADJ_PEN,HGT_ADJ_OPT,"PURPLE",false,false,true,true}
};

// Non-owning: points at an entry of data_mode_layer_cache, or null if no
// colour-by list is active.
static layer_list *data_user_layers = nullptr;
static bool sort_data_user_layers = true;

static void save_and_invalidate_layer_state( layer_list *layers );
static void restore_layer_state( layer_list *layers );

// data_user_layers holds whichever mode's list is currently active; the
// cache below is what actually owns each mode's list. A given mode's
// content (a classification, the data file list) is fixed for a snap
// session, so once built it's kept alive here rather than deleted and
// rebuilt on every colour-by switch - the same way data_type_layers
// already behaves - which lets copy_layer()'s existing lyr_id matching
// persist checkbox status and colour across switches for free. Residual
// and redundancy lists depend on dialog-chosen parameters rather than the
// underlying data, so those entries are explicitly evicted by
// invalidate_data_user_layer_cache() when their parameters change.
static std::map<std::string, layer_list> data_mode_layer_cache;

// station_user_layers holds whichever classification's list is currently
// active; the cache below is what actually owns each classification's list,
// keyed by class_id, for the same reason data_mode_layer_cache exists - a
// classification's content is fixed for a snap session, so once built it's
// kept alive here rather than deleted and rebuilt on every switch. No
// explicit invalidation is needed: unlike data_user_layers' residual/
// redundancy entries, classification definitions never change mid-session.
static std::map<int, layer_list> station_class_layer_cache;

// Built on first use, so empty until then.
static layer_list data_type_layers;
// Persistent, always-shown filter list of data files - unlike data_user_layers,
// this is independent of which mode is selected for Colour by, exactly like
// data_type_layers already is. Built on first use, so empty until then.
static layer_list data_file_layers;

static layer_list data_usage_layers =
{
    {"Obs status",UNUSED_PEN_ID,OTHER_OPT,{},true,false,false,false,true},
    {"Used obs",UNUSED_PEN_ID, USED_OBS_OPT,{},true,false,false,false},
    {"Rejected obs",UNUSED_PEN_ID, REJECTED_OBS_OPT,{},true,false,false,false},
    {"Unused obs",UNUSED_PEN_ID, UNUSED_OBS_OPT,{},true,false,false,false}
};

static layer_list base_background_layers =
{
    {"Map background",BACKGROUND_PEN, UNUSED_OPT_ID,"WHITE",true,false,false,false},
    {"Text",TEXT_PEN,UNUSED_OPT_ID,"BLACK",false,false,false,false},
    {"Highlight",HIGHLIGHT_PEN,UNUSED_OPT_ID,"GREEN",true,false,false,false},
    {"Selected",SELECTED_PEN,UNUSED_OPT_ID,"CORAL",true,false,false,false}
};

// Empty until the background layers have been set up, and stays empty if
// there are none.
static layer_list background_layers;

// Symbols used for plotting stations ..

struct symbol_s
{
    std::string_view name; ///< a literal
    int id;
    int nNodes;
    double size;
    std::string_view borderColour; ///< a literal
    std::string_view fillColour; ///< a literal, empty for no fill
};

static const std::array<symbol_s,5> default_symbols =
{{
    {"Free_station", FREE_STN_SYM, 0, 1.0, "TEXT", {} },
    {"Fixed_station", FIXED_STN_SYM, 4, 1.2, "TEXT", {} },
    {"Hor_fixed_station", HOR_FIXED_STN_SYM, 3, 1.3, "TEXT", {} },
    {"Vrt_fixed_station", VRT_FIXED_STN_SYM, -3, 1.3, "TEXT", {} },
    {"Rejected_station", REJECTED_STN_SYM, 0, 1.0, "TEXT", {} }
}};

static int symbol_lookup[N_STN_SYM];

// Colours used for assigning a range of colours ... needs reviewing
// TODO: May be better to calculate a range of colours using RGB, or
// at least define a better set as RGB values..

static constexpr std::array<std::string_view,5> range_colours =
{ "RED", "ORANGE", "PURPLE", "BLUE", "GREY" };

// Symbology ...

static std::unique_ptr<Symbology> symbology;

// Lookup from external ids to symbology number -

static int basepenid[N_BASE_PENS];   /* Lookup basic pens to layer id */
static int baseoptid[N_BASE_OPTS];   /* Lookup basic options to layer id */

static wxColour symbolBorderColour = wxNullColour;
static wxString stationFontDesc;
static int stationTextStyleId = 0;

static wxFont stationFont()
{
    if( ! stationFontDesc.IsEmpty() )
    {
        wxFont stfont;
        stfont.SetNativeFontInfoUserDesc( stationFontDesc );
        if( stfont.IsOk() ) return stfont;
        stationFontDesc.Clear();
    }
    return wxFont( 10, wxFONTFAMILY_SWISS,wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
}


Symbology *CreateSymbology()
{
    Symbology *s = new Symbology();

    ColourPalette p;

    for( auto c : defaultPalette )
    {
        p.AddColour( wxColour( c.data() ));
    }

    s->InitialisePalette( p );
    stationTextStyleId = s->AddTextStyle( new TextStyle("text", stationFont()) );

    return s;
}

Symbology *GetSnapplotSymbology()
{
    if( ! symbology ) symbology.reset( CreateSymbology() );
    return symbology.get();
}


static void copy_layer( Symbology *sym, int id, Symbology *oldsym, int oldid )
{
    LayerSymbology &l = sym->GetLayer( id );
    LayerSymbology &ol = oldsym->GetLayer( oldid );

    // Just in case we've got things misaligned ... (could put in a debug assert here!)
    wxASSERT_MSG( l.Name() == ol.Name(), "Layer mismatch in copy_layer" );
    if( l.Name() != ol.Name() ) return;

    if( l.HasColour() && ol.HasColour() )
    {
        int colourid = sym->GetPalette()->AddColour( oldsym->GetPalette()->Colour( ol.ColourId() ) );
        l.SetColourId( colourid );
    }
    l.SetStatus( ol.Status() );
}

static void build_station_symbols();

std::string get_station_font()
{
    return stationFont().GetNativeFontInfoUserDesc().ToStdString();
}

void set_station_font( const std::string &fontdef )
{
    stationFontDesc=wxString(fontdef);
    symbology->GetTextStyle(stationTextStyleId)->SetFont( stationFont() );
}

int get_station_font_id()
{
    return stationTextStyleId;
}

static void set_station_layer_colourflag( bool on )
{
    for( size_t i = 0; i < station_layers_pens.size(); i++ )
    {
        // i+1 to skip the "Station types" header row added at index 0
        station_layers[i+1].pen_id = on ? station_layers_pens[i] : UNUSED_PEN_ID;
    }
}

static void setup_station_class_layers( int class_id )
{
    save_and_invalidate_layer_state( station_user_layers );
    set_station_layer_colourflag( true );
    if( class_id < 1 || class_id > net->classification_count())
    {
        station_user_layers = nullptr;
        return;
    }

    const auto cached = station_class_layer_cache.find( class_id );
    if( cached != station_class_layer_cache.end() )
    {
        set_station_layer_colourflag( false );
        station_user_layers = &cached->second;
        return;
    }

    int nlayer = net->class_count( class_id );
    if( nlayer <= 0 )
    {
        station_user_layers = nullptr;
        return;
    }

    set_station_layer_colourflag( false );
    layer_list layers;
    layers.reserve( nlayer + 1 );
    layers.emplace_back( net->class_name(class_id), dflt_data_colour, true );
    for( int i = 0; i < nlayer; i++ )
    {
        const std::string v = net->class_value(class_id,i);
        const std::string label = v.substr( 0, 120 );
        layers.emplace_back( "SC_" + label + "|" + label, dflt_stn_colour, false );
    }

    station_user_layers = &station_class_layer_cache.emplace( class_id, std::move( layers ) ).first->second;
}

static void setup_data_type_layers()
{
    if( ! data_type_layers.empty() ) return;
    // NOBSTYPE data rows + 1 header/control-checkbox row (index 0)
    data_type_layers.reserve( NOBSTYPE + 1 );
    data_type_layers.emplace_back( "Data type", dflt_data_colour, true );
    for( int itype = 0; itype<NOBSTYPE; itype++ )
    {
        // itype+1 into data_type_layers to skip the header row; datatype[]/obstypecount[]
        // have no header row of their own, so they stay indexed by the plain itype
        layer_s &l = data_type_layers.emplace_back( datatype[itype].name, dflt_data_colour, false );
        if( obstypecount[itype] == 0 ) l.pen_id = UNUSED_LAYER_PEN_ID;
    }
}

// Builds the persistent, always-shown data file filter list once - the file
// list is fixed for a snap session, so this never needs rebuilding.
static void setup_data_file_layers()
{
    if( ! data_file_layers.empty() ) {
        return;
    }
    const int nfiles = survey_data_file_count();
    // nfiles data rows + 1 header/control-checkbox row (index 0)
    data_file_layers.reserve( nfiles + 1 );
    data_file_layers.emplace_back( "Data file", dflt_data_colour, true );
    for( int ifile = 0; ifile < nfiles; ifile++ ) {
        // ifile+1 to skip the header row added at index 0
        data_file_layers.emplace_back( survey_data_file_name(ifile), dflt_data_colour, false );
    }
}

// Resets every row's lyr_id to -1, marking this list as not present in the
// current symbology generation. Called by save_and_invalidate_layer_state()
// below - without this, a list excluded from a rebuild would keep its
// stale, still-positive lyr_id from whenever it was last shown, so a later
// save on it (e.g. from an unrelated rebuild while this list stays excluded)
// would misread that lyr_id as still valid and capture whatever unrelated
// layer now sits at that index in the current symbology, corrupting
// savedColour/savedStatus.
static void invalidate_layer_ids( layer_list &layers )
{
    for( layer_s &l : layers ) {
        l.lyr_id = -1;
    }
}

// Snapshots a layer_s list's current live status/colour onto each row's own
// savedStatus/savedColour, then invalidates every row's lyr_id (see
// invalidate_layer_ids()) - together, what a list needs before it's
// repointed elsewhere, freed and rebuilt, or simply left out of the next
// symbology rebuild. Must run while `symbology` is still the generation this
// list was last shown in - copy_layer()'s lyr_id matching only survives one
// rebuild, so this is the only reliable way to carry state across the
// intervening rebuilds that happen while a different list is active.
static void save_and_invalidate_layer_state( layer_list *layers )
{
    if( ! layers || ! symbology ) {
        return;
    }
    for( layer_s &l : *layers ) {
        if( l.lyr_id < 0 ) {
            continue;
        }
        LayerSymbology &ls = symbology->GetLayer( l.lyr_id );
        l.savedStatus = ls.Status();
        if( ls.HasColour() ) {
            l.savedColour = symbology->GetPalette()->Colour( ls.ColourId() );
        }
    }
    invalidate_layer_ids( *layers );
}

// Applies a layer_s list's saved state onto the live LayerSymbology, once the
// symbology has been rebuilt and each row has a fresh lyr_id. Status is
// applied unconditionally - savedStatus defaults to true, matching a fresh
// row's own default, so this is a no-op the first time a list is shown -
// since some rows (e.g. a list's own header/control-checkbox row) have a
// status but no colour at all. Colour is only applied if this row actually
// had one saved.
static void restore_layer_state( layer_list *layers )
{
    if( ! layers || ! symbology ) {
        return;
    }
    for( const layer_s &l : *layers ) {
        if( l.lyr_id < 0 ) {
            continue;
        }
        LayerSymbology &ls = symbology->GetLayer( l.lyr_id );
        ls.SetStatus( l.savedStatus );
        if( ls.HasColour() && l.savedColour ) {
            ls.SetColourId( symbology->GetPalette()->AddColour( *l.savedColour ) );
        }
    }
}

// Looks up `header` in data_mode_layer_cache; if absent, builds a fresh list
// from ndatapens/datapennames and caches it. Returns the list either way,
// reporting via freshlyBuilt whether it was just constructed. Does not touch
// data_user_layers - callers needing the exclusive Colour-by mode slot do
// that themselves.
static layer_list *get_or_build_data_mode_layers( const std::vector<std::string> &datapennames, const std::string &header, bool &freshlyBuilt )
{
    const auto inserted = data_mode_layer_cache.emplace( header, layer_list() );
    layer_list &layers = inserted.first->second;
    freshlyBuilt = inserted.second;
    if( ! freshlyBuilt ) {
        return &layers;
    }

    layers.reserve( datapennames.size() + 1 );
    layers.emplace_back( header, dflt_data_colour, true );
    for( const std::string &penname : datapennames ) {
        layers.emplace_back( penname, dflt_data_colour, false );
    }

    return &layers;
}

// Builds classification class_type's row names ("OC_value|value", mirroring
// plotconn.cpp's old setup_classification_pens()), shared by
// setup_classification_pens_layers() and get_classification_filter_layers().
// Returns false if class_type has no values, leaving header/names untouched.
static constexpr std::string_view classificationPrefix = "OC_";
static constexpr size_t CLASSIFICATION_LABEL_SIZE = 64;

static bool build_classification_pen_names( const int class_type, std::string &header, std::vector<std::string> &names )
{
    const int npens = obs_classes.value_count( class_type );
    if( npens <= 0 ) {
        return false;
    }
    header = obs_classes.name( class_type );
    names.clear();
    for( int i = 0; i < npens; i++ ) {
        const std::string value = obs_classes.value_name( class_type, i );
        const std::string label = value.substr( 0, CLASSIFICATION_LABEL_SIZE );
        names.push_back( std::string( classificationPrefix ) + label + "|" + label );
    }
    return true;
}

// Makes classification class_type the active Colour-by mode, by pointing
// data_user_layers at its list - the same single-slot mechanism used for
// every other Colour-by mode (Data file, Residual, Redundancy, ...).
bool setup_classification_pens_layers( const int class_type )
{
    std::string header;
    std::vector<std::string> names;
    if( ! build_classification_pen_names( class_type, header, names ) ) {
        return false;
    }
    return setup_data_layers( names, header, true );
}

// Points at classification class_type's list (building and caching it first
// if needed) as an always-on Display-by filter, independent of whichever
// mode is the active Colour-by selection. Doesn't touch data_user_layers or
// trigger a rebuild itself - setup_snapplot_symbology() is already rebuilding
// when it calls this. Returns nullptr if class_type has no values.
static layer_list *get_classification_filter_layers( const int class_type )
{
    std::string header;
    std::vector<std::string> names;
    if( ! build_classification_pen_names( class_type, header, names ) ) {
        return nullptr;
    }
    bool freshlyBuilt;
    return get_or_build_data_mode_layers( names, header, freshlyBuilt );
}

static bool setup_data_pens_layers( const std::vector<std::string> &datapennames, const std::string &header )
{
    save_and_invalidate_layer_state( data_user_layers );
    if( datapennames.empty() ) {
        data_user_layers = nullptr;
        return false;
    }
    bool freshlyBuilt;
    data_user_layers = get_or_build_data_mode_layers( datapennames, header, freshlyBuilt );
    return freshlyBuilt;
}

// Evicts and frees the cached list for the given header, e.g. when a
// dialog changes the parameters (bin count, max value) that generated it,
// so it gets rebuilt fresh next time that mode is selected.
void invalidate_data_user_layer_cache( const std::string &header )
{
    const auto cached = data_mode_layer_cache.find( header );
    if( cached == data_mode_layer_cache.end() ) {
        return;
    }
    if( data_user_layers == &cached->second ) {
        data_user_layers = nullptr;
    }
    data_mode_layer_cache.erase( cached );
}

// Evicts and frees the cache entry currently pointed to by `activeLayers`,
// found by pointer rather than by key so the caller doesn't need to know
// which mode/classification is active - used by "reset all" so the list
// rebuilds with default colours as well as the requested checkbox status.
// Restricted to the two key types data_mode_layer_cache/station_class_layer_cache
// actually use, rather than left open to any type by mistake.
template <typename K>
static void invalidate_active_layer_cache( std::map<K, layer_list> &cache, layer_list *&activeLayers )
{
    static_assert( std::is_same_v<K, std::string> || std::is_same_v<K, int>,
                   "invalidate_active_layer_cache is only used with std::string (per-mode header) or int (per-classification id) keys" );
    if( ! activeLayers ) {
        return;
    }
    for( auto it = cache.begin(); it != cache.end(); ++it ) {
        if( &it->second == activeLayers ) {
            cache.erase( it );
            activeLayers = nullptr;
            return;
        }
    }
}

// Evicts and frees data_user_layers' own cache entry - see invalidate_active_layer_cache().
void invalidate_active_data_user_layer_cache()
{
    invalidate_active_layer_cache( data_mode_layer_cache, data_user_layers );
}

// Evicts and frees station_user_layers' own cache entry - see invalidate_active_layer_cache().
void invalidate_active_station_class_layer_cache()
{
    invalidate_active_layer_cache( station_class_layer_cache, station_user_layers );
}

static void reset_layer_status( const layer_list *layers, const bool is_on )
{
    if( ! layers || ! symbology ) {
        return;
    }
    for( const layer_s &l : *layers ) {
        if( l.lyr_id < 0 ) {
            continue;
        }
        LayerSymbology &ls = symbology->GetLayer( l.lyr_id );
        ls.SetStatus( is_on );
        if( l.is_control_checkbox ) {
            ls.SetMixedRowStatus( false );
        }
    }
}

void reset_data_user_layers( const bool is_on )
{
    // data_user_layers holds the active colour-by list (residual/redundancy/
    // classification); data_type_layers and data_file_layers are independent,
    // always-shown filter lists, present under OBSERVATIONS regardless of
    // which mode is active. All three get reset together.
    reset_layer_status( data_user_layers, is_on );
    reset_layer_status( &data_type_layers, is_on );
    reset_layer_status( &data_file_layers, is_on );
}

void reset_station_user_layers( const bool is_on )
{
    reset_layer_status( station_user_layers, is_on );
}

// Resets every data_type_layers row's colour back to the default palette
// entry. Unlike data_user_layers, this list is never rebuilt, so it has no
// cache entry to evict - this is its only way back to default colours.
void reset_data_type_layer_colours()
{
    if( ! symbology ) {
        return;
    }
    for( const layer_s &l : data_type_layers ) {
        if( l.lyr_id < 0 ) {
            continue;
        }
        LayerSymbology &ls = symbology->GetLayer( l.lyr_id );
        if( ! ls.HasColour() ) {
            continue;
        }
        ls.SetColourId( symbology->GetPalette()->AddColour( wxColour( dflt_data_colour.data() ) ) );
    }
}

static void setup_background_layers()
{
    if( ! background_layers.empty() )  return;
    int nlayer = background_layer_count();
    if( nlayer <= 0 ) return;
    background_layers.reserve( nlayer );
    for( int i = 0; i < nlayer; i++ )
    {
        background_layers.emplace_back( background_layer_name(i+1), dflt_background_colour, false );
    }
}

static void remove_unwanted_layers( layer_list &layers )
{
    bool got_hor = dimension == 2 || dimension == 3;
    bool got_vrt = dimension == 1 || dimension == 3;
    bool got_cvr = (got_covariances() != 0);

    for( layer_s &l : layers )
    {
        if( (l.need_hor && ! got_hor ) ||
                (l.need_vrt && ! got_vrt ) ||
                (l.need_cvr && ! got_cvr ) )
        {
            l.pen_id = UNUSED_LAYER_PEN_ID;
        }
    }
}

static void add_layer_to_symbology( Symbology *symbology, layer_s &l, Symbology *oldSymbology, const bool colourEditable = true )
{
    int oldid = l.lyr_id;
    l.lyr_id = -1;

    if( l.pen_id == UNUSED_LAYER_PEN_ID ) return;

    int type = 0;
    if( l.pen_id != UNUSED_PEN_ID ) type |= LayerSymbology::hasColour;
    if( l.opt_id != UNUSED_OPT_ID ) type |= LayerSymbology::hasStatus;

    // dfltColour views a literal, so data() is null terminated (or null if there is no colour)
    wxColour colour = wxColour( l.dfltColour.data() );
    bool display = true;

    l.lyr_id = symbology->AddLayer( l.name, type, colour, display, l.is_control_checkbox, colourEditable );
    if( oldSymbology && oldid >= 0 ) copy_layer( symbology, l.lyr_id, oldSymbology, oldid );

    if( l.pen_id >= 0 ) basepenid[l.pen_id] = l.lyr_id;
    if( l.opt_id >= 0 ) baseoptid[l.opt_id] = l.lyr_id;
}

static void add_layers_to_symbology( Symbology *symbology, layer_list &layers, Symbology *oldSymbology, const bool colourEditable = true )
{
    remove_unwanted_layers( layers );

    for( layer_s &l : layers )
    {
        add_layer_to_symbology( symbology, l, oldSymbology, colourEditable );
    }
}

static void add_sorted_layers_to_symbology( Symbology *symbology, layer_list &layers, Symbology *oldSymbology, bool sort, const bool colourEditable = true )
{
    remove_unwanted_layers( layers );
    std::vector<layer_s *> sorted;
    sorted.reserve( layers.size() );
    for( layer_s &l : layers )
    {
        if( l.pen_id == UNUSED_LAYER_PEN_ID ) continue;
        // A control checkbox row should remain at the top of the list,
        // rather than sorting alongside the children it controls.
        if( l.is_control_checkbox )
        {
            add_layer_to_symbology( symbology, l, oldSymbology, colourEditable );
            continue;
        }
        // Otherwise add it to list to be sorted..
        sorted.push_back( &l );
    }
    if( sort )
    {
        std::stable_sort( sorted.begin(), sorted.end(),
            []( const layer_s *l1, const layer_s *l2 ) { return stncodecmp( l1->name, l2->name ) < 0; } );
    }
    for( layer_s *l : sorted ) { add_layer_to_symbology( symbology, *l, oldSymbology, colourEditable ); }
}

static void setup_snapplot_symbology()
{
    // Snapshot every cached data-mode list's live state before this
    // generation's symbology is discarded, and mark all of them as not
    // present - only whichever ones actually get re-added further down (via
    // add_layers_to_symbology/add_sorted_layers_to_symbology) pick up a
    // fresh lyr_id this generation. Covers data_user_layers (also saved
    // explicitly by setup_data_pens_layers(), harmlessly redundant here) and
    // every classification's Display-by filter list, which can be included
    // or excluded from one rebuild to the next based on Display-by toggles,
    // with no other "before rebuild" hook of its own.
    for( auto &entry : data_mode_layer_cache ) {
        save_and_invalidate_layer_state( &entry.second );
    }
    save_and_invalidate_layer_state( &data_type_layers );
    save_and_invalidate_layer_state( &data_file_layers );
    save_and_invalidate_layer_state( &data_usage_layers );

    const std::unique_ptr<Symbology> oldSymbology = std::move( symbology );
    symbology.reset( CreateSymbology() );
    if( oldSymbology ) symbology->InitialisePalette( *(oldSymbology->GetPalette()) );

    setup_data_type_layers();
    setup_data_file_layers();
    setup_background_layers();

    for( int i = 0; i < N_BASE_PENS; i++ ) { basepenid[i] = -1; }
    for( int i = 0; i < N_BASE_OPTS; i++ ) { baseoptid[i] = -1; }

    Symbology *const current = symbology.get();
    Symbology *const previous = oldSymbology.get();

    symbology->AddTitle("STATIONS");
    if( station_user_layers )
    {
        add_sorted_layers_to_symbology( current, *station_user_layers, previous, true );
        restore_layer_state( station_user_layers );
        symbology->AddSpacer();
    }
    add_layers_to_symbology( current, station_layers, previous );
    if( ! oldSymbology )
    {
        // Default is not to show codes or names .. clutters map!
        // Ditto for relative errors
        int pen;
        pen = baseoptid[NAME_OPT];
        if( pen >= 0 ) symbology->GetLayer(pen).SetStatus(false);
        pen = baseoptid[CODE_OPT];
        if( pen >= 0 ) symbology->GetLayer(pen).SetStatus(false);
        pen = baseoptid[REL_ELL_OPT];
        if( pen >= 0 ) symbology->GetLayer(pen).SetStatus(false);
        pen = baseoptid[REL_HGT_OPT];
        if( pen >= 0 ) symbology->GetLayer(pen).SetStatus(false);
    }

    symbology->AddSpacer();
    symbology->AddTitle("OBSERVATIONS");
    if( data_user_layers )
    {
        add_sorted_layers_to_symbology( current, *data_user_layers, previous, sort_data_user_layers );
        restore_layer_state( data_user_layers );
        symbology->AddSpacer();
    }
    if( is_displayby_enabled( DISPLAYBY_DATATYPE ) )
    {
        add_layers_to_symbology( current, data_type_layers, previous, get_data_pen_type() == DPEN_BY_TYPE );
        restore_layer_state( &data_type_layers );
        symbology->AddSpacer();
    }
    if( is_displayby_enabled( DISPLAYBY_DATAFILE ) )
    {
        add_layers_to_symbology( current, data_file_layers, previous, get_data_pen_type() == DPEN_BY_FILE );
        restore_layer_state( &data_file_layers );
        symbology->AddSpacer();
    }
    if( is_displayby_enabled( DISPLAYBY_OBSSTATUS ) )
    {
        add_layers_to_symbology( current, data_usage_layers, previous );
        restore_layer_state( &data_usage_layers );
        symbology->AddSpacer();
    }

    // Classifications enabled as Display-by filters, other than whichever
    // one (if any) is already shown above as the active Colour-by mode via
    // data_user_layers - showing it again here would duplicate the row.
    // Never colour-editable here: a classification only reaches this loop
    // when it is *not* the active Colour-by mode, and only the active mode's
    // list should ever have editable colour swatches.
    for( int classType = 1; classType <= obs_classes.count(); classType++ ) {
        if( classType == get_data_pen_type() ) {
            continue;
        }
        if( ! is_displayby_enabled( classType ) ) {
            continue;
        }
        layer_list *filterLayers = get_classification_filter_layers( classType );
        if( ! filterLayers ) {
            continue;
        }
        add_sorted_layers_to_symbology( current, *filterLayers, previous, true, false );
        restore_layer_state( filterLayers );
        symbology->AddSpacer();
    }

    symbology->AddSpacer();
    symbology->AddTitle("BACKGROUND");
    add_layers_to_symbology( current, base_background_layers, previous );
    if( ! background_layers.empty() )
    {
        add_sorted_layers_to_symbology( current, background_layers, previous, true );
    }

    build_station_symbols();
}

//extern "C"
bool setup_data_layers( const std::vector<std::string> &datapennames, const std::string &header, const bool sorted )
{
    const bool freshlyBuilt = setup_data_pens_layers( datapennames, header );
    sort_data_user_layers = sorted;
    setup_snapplot_symbology();
    return freshlyBuilt;
}

//extern "C"
void setup_station_layers( int class_id )
{
    setup_station_class_layers( class_id );
    setup_snapplot_symbology();
}

void rebuild_displayby_symbology()
{
    setup_snapplot_symbology();
}

static void build_station_symbols()
{
    // Create symbols ...

    symbolBorderColour = wxNullColour;

    for( const symbol_s &sym : default_symbols )
    {
        wxColour borderColour = wxNullColour;
        wxColour fillColour = wxNullColour;
        if( boost::algorithm::iequals( sym.borderColour, "TEXT" ) )
        {
            borderColour = symbology->LayerColour(basepenid[TEXT_PEN]);
            symbolBorderColour = borderColour;
        }
        else if( ! sym.borderColour.empty() )
        {
            // The views are of literals, so data() is null terminated
            borderColour = wxColour(sym.borderColour.data());
        }
        if( ! sym.fillColour.empty() ) fillColour = wxColour(sym.fillColour.data());
        PointSymbology *ptsym = new PointSymbology( sym.name.data(), sym.nNodes, sym.size, 0.0, borderColour, fillColour );
        int idSym = symbology->AddPointSymbol( ptsym );
        symbol_lookup[sym.id] = idSym;
    }
}

void rebuild_station_symbols()
{
    if( symbolBorderColour != wxNullColour &&
            symbolBorderColour != symbology->LayerColour(basepenid[TEXT_PEN]) )
    {
        build_station_symbols();
    }
}

int station_class_pen( int cvalue )
{
    if( station_user_layers ) return (*station_user_layers)[cvalue+1].lyr_id;
    return 0;
}

int get_pen( int item )
{
    int np = basepenid[ item ];
    return np < 0 ? 0 : np;
}

int pen_visible( int pen )
{
    // TODO: Ideally this should check that the pen is different from the canvas background
    // Except this won't work for symbols, as outline may still be visible ... but not called
    // for symbols ...
    return symbology->GetLayer(pen).ColourId() !=
           symbology->GetLayer(get_pen(BACKGROUND_PEN)).ColourId();
}

int get_symbol( int id )
{
    return symbol_lookup[id];
}

int get_symbol_points( int symbol_id, symbolpoint *ptlist, int maxpts )
{
    PointSymbology *symbol = symbology->GetPointSymbol(symbol_id);
    if( ! symbol ) return -1;
    int npts = symbol->NPoints();
    if( maxpts < symbol->NPoints() || maxpts < 1 ) return -1;
    if( npts == 0 )
    {
        symbol->GetPoint(0,&ptlist[0].x, &ptlist[0].y);
    }
    else
    {
        for( int i = 0; i < npts; i++ ) 
        {
            symbol->GetPoint(i,&ptlist[i].x, &ptlist[i].y);
        }
    }
    return npts;
}

void set_pen_colour_range( )
{
    size_t colourIdx = 0;
    if( ! data_user_layers ) return;
    for( const layer_s &l : *data_user_layers )
    {
        LayerSymbology &ls = symbology->GetLayer( l.lyr_id );
        int colourid = symbology->GetPalette()->AddColour(
            wxColour(range_colours[std::min(colourIdx, range_colours.size()-1)].data()) );
        ls.SetColourId( colourid );
        colourIdx++;
    }
}

int pen_count( void )
{
    return symbology->LayerCount();
}

const std::string &pen_name( int ipen )
{
    return symbology->GetLayer(ipen).NameString();
}

bool pen_has_colour( int ipen )
{
    return symbology->GetLayer(ipen).HasColour();
}

void get_pen_colour( int ipen, unsigned char &red, unsigned char &green, unsigned char &blue )
{
    const wxColour &c = symbology->LayerColour(ipen);
    red = c.Red();
    green = c.Green();
    blue = c.Blue();
}

int option_selected( int option )
{
    return pen_selected( baseoptid[option] );
}

int background_option( int layer_id )
{
    return pen_selected( background_pen( layer_id ));
}

int background_pen( int layer_id )
{
    return background_layers[layer_id-1].lyr_id;
}

int data_pen( int dpen )
{
    // dpen+1 to account for header layer..
    if( data_user_layers ) {
        return (*data_user_layers)[dpen+1].lyr_id;
    }
    if( get_data_pen_type() == DPEN_BY_FILE ) {
        return data_file_layers[dpen+1].lyr_id;
    }
    return data_type_layers[dpen+1].lyr_id;
}

int pen_selected( int pen )
{
    int selected = 0;
    if( pen >= 0 && symbology->GetLayer(pen).Show() ) selected = 1;
    return selected;
}

int datatype_selected( int datatype )
{
    int lyrid;
    // datatype+1 to account for header layer..
    lyrid = data_type_layers[datatype+1].lyr_id;
    return pen_selected( lyrid );
}

bool filetype_selected( const int file )
{
    // file+1 to account for header layer..
    const int lyrid = data_file_layers[file+1].lyr_id;
    return pen_selected( lyrid ) != 0;
}

// Looks up class_type's cached Display-by filter list, without building it -
// unlike get_classification_filter_layers(), safe to call from the
// connection-drawing loop (once per connection) without risking a wasted
// allocation on every call.
static const layer_list *find_classification_filter_layers( int class_type )
{
    const auto cached = data_mode_layer_cache.find( obs_classes.name( class_type ) );
    if( cached == data_mode_layer_cache.end() ) {
        return nullptr;
    }
    return &cached->second;
}

bool classification_value_selected( const int class_type, const int value_id )
{
    const layer_list *filterLayers = find_classification_filter_layers( class_type );
    if( ! filterLayers ) {
        return true;
    }
    // value_id+1 to account for header layer..
    return pen_selected( (*filterLayers)[value_id+1].lyr_id ) != 0;
}


int read_key_definition( std::string_view def )
{
    int nk = -1;
    int on = -1;
    int pen = -1;

    const size_t start = def.find_first_not_of( " \t" );
    if( start == std::string_view::npos ) return INVALID_DATA;
    def.remove_prefix( start );
    const char delim = def.front();
    def.remove_prefix( 1 );
    const size_t itemEnd = def.find( delim );
    if( itemEnd == std::string_view::npos ) return INVALID_DATA;
    const std::string item( def.substr( 0, itemEnd ) );
    def.remove_prefix( itemEnd + 1 );
    FieldScanner scanner( def );
    for( auto fld = scanner.nextToken( " \t\r\n" ); fld; fld = scanner.nextToken( " \t\r\n" ) )
    {
        if( boost::algorithm::iequals( *fld, "on" ) )
        {
            on = 1;
        }
        else if( boost::algorithm::iequals( *fld, "off" ) )
        {
            on = 0;
        }
        else
        {
            const std::string colourName( *fld );
            wxString colname( colourName );
            colname.Replace("_"," ");
            wxColour c = wxColour(colname);
            pen = symbology->GetPalette()->AddColour( c );
            // TODO: Need some handling here of invalid colour
        }
    }
    if( on < 0 && pen < 0 ) return MISSING_DATA;
    nk = symbology->GetLayerByIdentifier( wxString( item ) );
    if( nk < 0 ) return INCONSISTENT_DATA;
    LayerSymbology &ls = symbology->GetLayer(nk);
    if( on >= 0 ) { ls.SetStatus( on ? true : false ); }
    if( pen >= 0 ) { ls.SetColourId( pen ); }
    return OK;
}

void print_key( FILE *out, const char *prefix )
{
    int i;
    for( i = 0; i < symbology->LayerCount(); i++ )
    {
        LayerSymbology &ls = symbology->GetLayer(i);
        int type = ls.Type();
        if( type & LayerSymbology::hasColourAndStatus )
        {
            fprintf(out,"%s \"%s\"",prefix,(const char *)(ls.Identifier().mb_str()));
            if( type & LayerSymbology::hasStatus )
            {
                fputs( ls.Status() ? " on" : " off", out );
            }
            if( type & LayerSymbology::hasColour )
            {
                wxColour colour = symbology->GetPalette()->Colour( ls.ColourId());
                wxString colname = wxTheColourDatabase->FindName( colour );
                if( colname.IsEmpty() ) colname = colour.GetAsString(wxC2S_CSS_SYNTAX);
                colname.Replace(" ","_");
                fprintf(out," %s",(const char *)(colname.mb_str()));
            }
            fputs("\n",out);
        }
    }
}
