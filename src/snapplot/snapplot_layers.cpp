#include "snapconfig.h"
#include "snapplot_layers.hpp"
#include "wxsymbology.hpp"

#include "util/snapctype.h"

#include <map>
#include <memory>

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
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/errdef.h"
//}

// Null terminated list of default palette colours ..

static const char *defaultPalette[] =
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
    "BROWN",
    0
};

#define UNUSED_LAYER_PEN_ID -2
#define UNUSED_PEN_ID -1
#define UNUSED_OPT_ID -1

struct layer_s
{
    const char *name;
    int  pen_id;
    int  opt_id;
    const char *dfltColour;
    bool dfltOption;
    bool need_hor;
    bool need_vrt;
    bool need_cvr;
    int lyr_id;
    // true if this row's checkbox is to control a following range of
    // other rows' status, instead of its own
    bool is_control_checkbox = false;
};

static layer_s *station_user_layers = 0;

static int station_layers_pens[] = {FREE_STN_PEN,FREE_STN_PEN,HOR_FIXED_STN_PEN,HOR_FIXED_STN_PEN,REJECTED_STN_PEN,-1 };

static layer_s station_layers[] =
{
    {"Station types",UNUSED_PEN_ID,OTHER_OPT,0,true,false,false,false,-1,true},
    {"Free stations",FREE_STN_PEN,FREE_STN_OPT,"GREY",true,false,false,false,-1},
    {"Fixed stations",FIXED_STN_PEN,FIXED_STN_OPT,"RED",true,true,true,false,-1},
    {"Hor fixed stns",HOR_FIXED_STN_PEN,HOR_FIXED_STN_OPT,"RED",true,true,false,false,-1},
    {"Vrt fixed stns",VRT_FIXED_STN_PEN,VRT_FIXED_STN_OPT,"RED",true,false,true,false,-1},
    {"Rejected stns",REJECTED_STN_PEN,REJECTED_STN_OPT,"WHITE",true,false,false,false,-1},
    {"Symbol",UNUSED_PEN_ID,SYMBOL_OPT,0,true,false,false,false,-1},
    {"Name",UNUSED_PEN_ID, NAME_OPT,0,false,false,false,false,-1},
    {"Code",UNUSED_PEN_ID, CODE_OPT,0,false,false,false,false,-1},
    {"",UNUSED_PEN_ID,UNUSED_OPT_ID,0,false,false,false,false,-1},
    {"Station metrics",UNUSED_PEN_ID,OTHER_OPT,0,true,false,false,false,-1,true},
    {"Error ellipses",ELLIPSE_PEN,ELLIPSE_OPT,"SEA GREEN",true,true,false,true,-1},
    {"Relative ellipse",REL_ELL_PEN, REL_ELL_OPT,"SEA GREEN",false,true,false,true,-1},
    {"Hor adjustment",HOR_ADJ_PEN,HOR_ADJ_OPT,"RED",false,true,false,true,-1},
    {"Height error",HGTERR_PEN,HGTERR_OPT,"SEA GREEN",false,false,true,true,-1},
    {"Relative hgt err", REL_HGT_PEN, REL_HGT_OPT,"SEA GREEN",false,false,true,true,-1},
    {"Vrt adjustment",HGT_ADJ_PEN,HGT_ADJ_OPT,"PURPLE",false,false,true,true,-1},
    {0,0,0,0,false,false,false,false,-1}
};

static layer_s *data_user_layers = 0;
static bool sort_data_user_layers = true;

static void delete_layers( layer_s **pl );

// Deleter for a dynamically-built layer_s array, so ownership in
// data_mode_layer_cache can be a plain unique_ptr rather than a raw
// pointer someone has to remember to free by hand.
struct LayerArrayDeleter
{
    void operator()( layer_s *layers ) const
    {
        delete_layers( &layers );
    }
};
using LayerArrayPtr = std::unique_ptr<layer_s, LayerArrayDeleter>;

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
static std::map<wxString, LayerArrayPtr> data_mode_layer_cache;

static layer_s *data_type_layers = 0;
// Persistent, always-shown filter list of data files - unlike data_user_layers,
// this is independent of which mode is selected for Colour by, exactly like
// data_type_layers already is.
static layer_s *data_file_layers = 0;

static layer_s data_usage_layers[] =
{
    {"Obs status",UNUSED_PEN_ID,OTHER_OPT,0,true,false,false,false,-1,true},
    {"Used obs",UNUSED_PEN_ID, USED_OBS_OPT,0,true,false,false,false,-1},
    {"Rejected obs",UNUSED_PEN_ID, REJECTED_OBS_OPT,0,true,false,false,false,-1},
    {"Unused obs",UNUSED_PEN_ID, UNUSED_OBS_OPT,0,true,false,false,false,-1},
    {0,0,0,0,false,false,false,false,-1}
};

static layer_s base_background_layers[] =
{
    {"Map background",BACKGROUND_PEN, UNUSED_OPT_ID,"WHITE",true,false,false,false,-1},
    {"Text",TEXT_PEN,UNUSED_OPT_ID,"BLACK",false,false,false,false,-1},
    {"Highlight",HIGHLIGHT_PEN,UNUSED_OPT_ID,"GREEN",true,false,false,false,-1},
    {"Selected",SELECTED_PEN,UNUSED_OPT_ID,"CORAL",true,false,false,false,-1},
    {0,0,0,0,false,false,false,false,-1}
};

static layer_s *background_layers = 0;

// Symbols used for plotting stations ..

struct symbol_s
{
    const char *name;
    int id;
    int nNodes;
    double size;
    const char *borderColour;
    const char *fillColour;
};

static symbol_s default_symbols[] =
{
    {"Free_station", FREE_STN_SYM, 0, 1.0, "TEXT", NULL },
    {"Fixed_station", FIXED_STN_SYM, 4, 1.2, "TEXT", NULL },
    {"Hor_fixed_station", HOR_FIXED_STN_SYM, 3, 1.3, "TEXT", NULL },
    {"Vrt_fixed_station", VRT_FIXED_STN_SYM, -3, 1.3, "TEXT", NULL },
    {"Rejected_station", REJECTED_STN_SYM, 0, 1.0, "TEXT", NULL },
    {NULL,0,0,0.0,NULL,NULL},
};

static int symbol_lookup[N_STN_SYM];

// Colours used for assigning a range of colours ... needs reviewing
// TODO: May be better to calculate a range of colours using RGB, or
// at least define a better set as RGB values..

static const char *range_colours[] =
{ "RED", "ORANGE", "PURPLE", "BLUE", "GREY", 0 };

static const char *dflt_stn_colour = "RED";
static const char *dflt_data_colour = "BLUE";
static const char *dflt_background_colour = "LIGHT GREY";

// Symbology ...

static Symbology *symbology = 0;

// Lookup from external ids to symbology number -

static int basepenid[N_BASE_PENS];   /* Lookup basic pens to layer id */
static int baseoptid[N_BASE_OPTS];   /* Lookup basic options to layer id */

static wxColour symbolBorderColour = wxNullColour;
static wxString stationFontDesc;
static int stationTextStyleId = 0;
static const char *stnfontdesc=0;

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

    for( const char **c = defaultPalette; *c; c++ )
    {
        p.AddColour( wxColour( *c ));
    }

    s->InitialisePalette( p );
    stationTextStyleId = s->AddTextStyle( new TextStyle("text", stationFont()) );

    return s;
}

Symbology *GetSnapplotSymbology()
{
    if( ! symbology ) symbology = CreateSymbology();
    return symbology;
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

const char *get_station_font()
{

    wxString fontdesc = stationFont().GetNativeFontInfoUserDesc();
    if( stnfontdesc ) check_free( (void *)(stnfontdesc) );
    stnfontdesc=copy_string(fontdesc.mb_str());
    return stnfontdesc;
}

void set_station_font( const char *fontdef )
{
    stationFontDesc=wxString(fontdef);
    symbology->GetTextStyle(stationTextStyleId)->SetFont( stationFont() );
}

int get_station_font_id()
{
    return stationTextStyleId;
}

static void delete_layers( layer_s **pl )
{
    if( ! *pl ) return;
    for( layer_s *l = *pl; l->name; l++ )
    {
        check_free( (void *)(l->name) );
    }
    check_free(*pl);
    (*pl) = 0;
}

static void init_layer( layer_s *l, const char *name, const char *colour, bool title )
{
    char *lname=copy_string(name);
    lname[0]=TOUPPER(lname[0]);
    l->name = lname;
    l->pen_id = title ? UNUSED_PEN_ID : OTHER_PEN;
    l->opt_id = title ? UNUSED_OPT_ID : OTHER_OPT;
    l->dfltColour = colour;
    l->dfltOption = true;
    l->need_hor = false;
    l->need_vrt = false;
    l->need_cvr = false;
    l->lyr_id = -1;
    l->is_control_checkbox = false;
}

static void set_station_layer_colourflag( bool on )
{
    for( int i = 0; station_layers_pens[i] >= 0; i++ )
    {
        // i+1 to skip the "Station types" header row added at index 0
        station_layers[i+1].pen_id = on ? station_layers_pens[i] : UNUSED_PEN_ID;
    }
}

static void setup_station_class_layers( int class_id )
{
    set_station_layer_colourflag( true );
    if( station_user_layers )  delete_layers( &station_user_layers );
    if( class_id < 1 || class_id > network_classification_count(net)) return;
    int nlayer = network_class_count( net, class_id );
    if( nlayer > 0 )
    {
        set_station_layer_colourflag( false );
        station_user_layers = (layer_s *) check_malloc( sizeof(layer_s) * (nlayer+2) );
        layer_s *l = &(station_user_layers[0]);
        init_layer( l, network_class_name(net, class_id), dflt_data_colour, true );
        l->opt_id = OTHER_OPT;
        l->is_control_checkbox = true;
        for( int i = 0; i < nlayer; i++ )
        {
            char buf[256];
            l = &(station_user_layers[i+1]);
            const char *v = network_class_value(net,class_id,i);
            sprintf(buf,"SC_%.120s|%.120s",v,v);
            init_layer(l,buf,dflt_stn_colour,false);
        }
        station_user_layers[nlayer+1].name = 0;
    }
}

static void setup_data_type_layers()
{
    if( data_type_layers ) return;
    // NOBSTYPE data rows + 1 header/control-checkbox row (index 0) + 1 terminator (name=0)
    data_type_layers = (layer_s *) check_malloc( sizeof(layer_s) * (NOBSTYPE + 2) );
    layer_s *l = &(data_type_layers[0]);
    init_layer(l,"Data type",dflt_data_colour,true);
    l->opt_id = OTHER_OPT;
    l->is_control_checkbox = true;
    for( int itype = 0; itype<NOBSTYPE; itype++ )
    {
        // itype+1 into data_type_layers to skip the header row; datatype[]/obstypecount[]
        // have no header row of their own, so they stay indexed by the plain itype
        l = &(data_type_layers[itype+1]);
        init_layer(l,datatype[itype].name,dflt_data_colour, 0 );
        if( obstypecount[itype] == 0 ) l->pen_id = UNUSED_LAYER_PEN_ID;
    }
    data_type_layers[NOBSTYPE+1].name = 0;
}

// Builds the persistent, always-shown data file filter list once - the file
// list is fixed for a snap session, so this never needs rebuilding.
static void setup_data_file_layers()
{
    if( data_file_layers ) {
        return;
    }
    const int nfiles = survey_data_file_count();
    // nfiles data rows + 1 header/control-checkbox row (index 0) + 1 terminator (name=0)
    data_file_layers = static_cast<layer_s *>( check_malloc( sizeof(layer_s) * (nfiles + 2) ) );
    layer_s *l = &(data_file_layers[0]);
    init_layer(l,"Data file",dflt_data_colour,true);
    l->opt_id = OTHER_OPT;
    l->is_control_checkbox = true;
    for( int ifile = 0; ifile < nfiles; ifile++ ) {
        // ifile+1 to skip the header row added at index 0
        l = &(data_file_layers[ifile+1]);
        init_layer(l,survey_data_file_name(ifile),dflt_data_colour,false);
    }
    data_file_layers[nfiles+1].name = 0;
}

// Points data_user_layers at the cached list for this header, building and
// caching it first if this is the first time this mode has been selected.
void setup_data_pens_layers( int ndatapens, const char **datapennames, const char *header )
{
    if( ndatapens <= 0 ) {
        data_user_layers = 0;
        return;
    }

    const wxString key( header );
    const auto cached = data_mode_layer_cache.find( key );
    if( cached != data_mode_layer_cache.end() ) {
        data_user_layers = cached->second.get();
        return;
    }

    layer_s *layers = static_cast<layer_s *>( check_malloc( sizeof(layer_s) * (ndatapens+2) ) );
    layer_s *l = &(layers[0]);
    init_layer(l,header,dflt_data_colour,true);
    l->opt_id = OTHER_OPT;
    l->is_control_checkbox = true;
    for( int i = 0; i < ndatapens; i++ ) {
        l = &(layers[i+1]);
        init_layer(l,copy_string(datapennames[i]),dflt_data_colour,false);
    }
    layers[ndatapens+1].name = 0;

    data_user_layers = layers;
    data_mode_layer_cache.emplace( key, LayerArrayPtr( layers ) );
}

// Evicts and frees the cached list for the given header, e.g. when a
// dialog changes the parameters (bin count, max value) that generated it,
// so it gets rebuilt fresh next time that mode is selected.
void invalidate_data_user_layer_cache( const char *header )
{
    const auto cached = data_mode_layer_cache.find( wxString( header ) );
    if( cached == data_mode_layer_cache.end() ) {
        return;
    }
    if( data_user_layers == cached->second.get() ) {
        data_user_layers = 0;
    }
    data_mode_layer_cache.erase( cached );
}

// Evicts and frees data_user_layers' own cache entry, found by pointer
// rather than by header text so the caller doesn't need to know which
// mode is active - used by "reset all" so the list rebuilds with default
// colours as well as the requested checkbox status.
void invalidate_active_data_user_layer_cache()
{
    if( ! data_user_layers ) {
        return;
    }
    for( auto it = data_mode_layer_cache.begin(); it != data_mode_layer_cache.end(); ++it ) {
        if( it->second.get() == data_user_layers ) {
            data_mode_layer_cache.erase( it );
            data_user_layers = 0;
            return;
        }
    }
}

static void reset_layer_status( layer_s *layers, const bool is_on )
{
    if( ! layers || ! symbology ) {
        return;
    }
    // Walk the array until we reach the null-terminated sentinel row.
    for( layer_s *l = layers; l->name; l++ ) {
        if( l->lyr_id < 0 ) {
            continue;
        }
        LayerSymbology &ls = symbology->GetLayer( l->lyr_id );
        ls.SetStatus( is_on );
        if( l->is_control_checkbox ) {
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
    reset_layer_status( data_type_layers, is_on );
    reset_layer_status( data_file_layers, is_on );
}

// Resets every data_type_layers row's colour back to the default palette
// entry. Unlike data_user_layers, this list is never rebuilt, so it has no
// cache entry to evict - this is its only way back to default colours.
void reset_data_type_layer_colours()
{
    if( ! data_type_layers || ! symbology ) {
        return;
    }
    for( layer_s *l = data_type_layers; l->name; l++ ) {
        if( l->lyr_id < 0 ) {
            continue;
        }
        LayerSymbology &ls = symbology->GetLayer( l->lyr_id );
        if( ! ls.HasColour() ) {
            continue;
        }
        ls.SetColourId( symbology->GetPalette()->AddColour( wxColour( dflt_data_colour ) ) );
    }
}

static void setup_background_layers()
{
    if( background_layers )  return;
    int nlayer = background_layer_count();
    if( nlayer <= 0 ) return;
    background_layers = (layer_s *) check_malloc( sizeof(layer_s) * (nlayer+1) );
    for( int i = 0; i < nlayer; i++ )
    {
        layer_s *l = &(background_layers[i]);
        init_layer(l,background_layer_name(i+1),dflt_background_colour,false);
    }
    background_layers[nlayer].name = 0;
}

static void remove_unwanted_layers( layer_s *layers )
{
    bool got_hor = dimension == 2 || dimension == 3;
    bool got_vrt = dimension == 1 || dimension == 3;
    bool got_cvr = (got_covariances() != 0);

    for( layer_s *l = layers; l->name; l++ )
    {
        if( (l->need_hor && ! got_hor ) ||
                (l->need_vrt && ! got_vrt ) ||
                (l->need_cvr && ! got_cvr ) )
        {
            l->pen_id = UNUSED_LAYER_PEN_ID;
        }
    }
}

static void add_layer_to_symbology( Symbology *symbology, layer_s *l, Symbology *oldSymbology, const bool colourEditable = true )
{
    int oldid = l->lyr_id;
    l->lyr_id = -1;

    if( l->pen_id == UNUSED_LAYER_PEN_ID ) return;

    int type = 0;
    if( l->pen_id != UNUSED_PEN_ID ) type |= LayerSymbology::hasColour;
    if( l->opt_id != UNUSED_OPT_ID ) type |= LayerSymbology::hasStatus;

    wxColour colour = wxColour( l->dfltColour );
    bool display = true;

    l->lyr_id = symbology->AddLayer( l->name, type, colour, display, l->is_control_checkbox, colourEditable );
    if( oldSymbology && oldid >= 0 ) copy_layer( symbology, l->lyr_id, oldSymbology, oldid );

    if( l->pen_id >= 0 ) basepenid[l->pen_id] = l->lyr_id;
    if( l->opt_id >= 0 ) baseoptid[l->opt_id] = l->lyr_id;
}

static void add_layers_to_symbology( Symbology *symbology, layer_s *layers, Symbology *oldSymbology, const bool colourEditable = true )
{
    remove_unwanted_layers( layers );

    for( layer_s *l = layers; l->name; l++ )
    {
        add_layer_to_symbology( symbology, l, oldSymbology, colourEditable );
    }
}

static int cmp_layer_names( const void *p1, const void *p2 )
{
    layer_s *l1 = * (layer_s **) p1;
    layer_s *l2 = * (layer_s **) p2;
    return stncodecmp(l1->name, l2->name);
}

static void add_sorted_layers_to_symbology( Symbology *symbology, layer_s *layers, Symbology *oldSymbology, bool sort )
{
    remove_unwanted_layers( layers );
    int nlayer;
    for( nlayer = 0; layers[nlayer].name; nlayer++ ) {};

    layer_s **sorted = (layer_s **) check_malloc( sizeof(layer_s *) * nlayer );
    nlayer = 0;
    for( layer_s *l = layers; l->name; l++ )
    {
        if( l->pen_id == UNUSED_LAYER_PEN_ID ) continue;
        // If it is a header then assume it should remain at the top of the list
        if( l->pen_id == UNUSED_PEN_ID && l->opt_id == UNUSED_OPT_ID )
        {
            add_layer_to_symbology( symbology, l, oldSymbology );
            continue;
        }
        // Otherwise add it to list to be sorted..
        sorted[nlayer++] = l;
    }
    if( sort ) qsort(sorted,nlayer,sizeof(layer_s *),cmp_layer_names);
    for( int i = 0; i < nlayer; i++ ) { add_layer_to_symbology( symbology, sorted[i], oldSymbology );}
    check_free( sorted );
}

static void setup_snapplot_symbology()
{
    Symbology *oldSymbology = symbology;
    symbology = CreateSymbology();
    if( oldSymbology ) symbology->InitialisePalette( *(oldSymbology->GetPalette()) );

    if( ! data_type_layers ) setup_data_type_layers();
    if( ! data_file_layers ) {
        setup_data_file_layers();
    }
    if( ! background_layers ) setup_background_layers();

    for( int i = 0; i < N_BASE_PENS; i++ ) { basepenid[i] = -1; }
    for( int i = 0; i < N_BASE_OPTS; i++ ) { baseoptid[i] = -1; }

    symbology->AddTitle("STATIONS");
    if( station_user_layers )
    {
        add_sorted_layers_to_symbology( symbology, station_user_layers, oldSymbology, true );
        symbology->AddSpacer();
    }
    add_layers_to_symbology( symbology, station_layers, oldSymbology );
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
        add_sorted_layers_to_symbology( symbology, data_user_layers, oldSymbology, sort_data_user_layers );
        symbology->AddSpacer();
    }
    add_layers_to_symbology( symbology, data_type_layers, oldSymbology, get_data_pen_type() == DPEN_BY_TYPE );
    symbology->AddSpacer();
    add_layers_to_symbology( symbology, data_file_layers, oldSymbology, get_data_pen_type() == DPEN_BY_FILE );
    symbology->AddSpacer();
    add_layers_to_symbology( symbology, data_usage_layers, oldSymbology );

    symbology->AddSpacer();
    symbology->AddTitle("BACKGROUND");
    add_layers_to_symbology( symbology, base_background_layers, oldSymbology );
    if( background_layers )
    {
        add_sorted_layers_to_symbology( symbology, background_layers, oldSymbology, true );
    }

    build_station_symbols();

    if( oldSymbology ) delete oldSymbology;
}

//extern "C"
void setup_data_layers( int ndatapens, const char **datapennames, const char *header, int sorted )
{
    setup_data_pens_layers( ndatapens, datapennames, header );
    sort_data_user_layers = (sorted != 0);
    setup_snapplot_symbology();
}

//extern "C" 
void setup_station_layers( int class_id )
{
    setup_station_class_layers( class_id );
    setup_snapplot_symbology();
}


static void build_station_symbols()
{
    // Create symbols ...

    symbolBorderColour = wxNullColour;

    for( symbol_s *sym = default_symbols; sym->name != NULL; sym++ )
    {
        wxColour borderColour = wxNullColour;
        wxColour fillColour = wxNullColour;
        if( _stricmp( sym->borderColour, "TEXT" ) == 0 )
        {
            borderColour = symbology->LayerColour(basepenid[TEXT_PEN]);
            symbolBorderColour = borderColour;
        }
        else if( sym->borderColour != NULL )
        {
            borderColour = wxColour(sym->borderColour);
        }
        if( sym->fillColour ) fillColour = wxColour(sym->fillColour);
        PointSymbology *ptsym = new PointSymbology( sym->name, sym->nNodes, sym->size, 0.0, borderColour, fillColour );
        int idSym = symbology->AddPointSymbol( ptsym );
        symbol_lookup[sym->id] = idSym;
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
    if( station_user_layers ) return station_user_layers[cvalue+1].lyr_id;
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
    const char **colour = range_colours; \
    if( ! data_user_layers ) return;
    for( layer_s *l = data_user_layers; l->name; l++  )
    {
        LayerSymbology &ls = symbology->GetLayer( l->lyr_id );
        int colourid = symbology->GetPalette()->AddColour( wxColour(*colour) );
        ls.SetColourId( colourid );
        colour++;
        if( ! *colour ) colour--;
    }
}

int pen_count( void )
{
    return symbology->LayerCount();
}

const char *pen_name( int ipen )
{
    return symbology->GetLayer(ipen).NamePtr();
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
        return data_user_layers[dpen+1].lyr_id;
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

// Whether the given data file's checkbox is currently on, independent of
// whatever mode is active for Colour by - mirrors datatype_selected().
int filetype_selected( int file )
{
    int lyrid;
    // file+1 to account for header layer..
    lyrid = data_file_layers[file+1].lyr_id;
    return pen_selected( lyrid );
}


int read_key_definition( char *def )
{
    int nk = -1;
    int on = -1;
    int pen = -1;
    char *fld;
    char *item;
    char delim;

    while( *def == ' ' || *def == '\t' ) def++;
    if( ! *def ) return INVALID_DATA;
    delim = *def;
    def++;
    item = def;
    while(*def && *def != delim ) def++;
    if( !*def ) return INVALID_DATA;
    *def = 0;
    def++;
    for( fld = strtok(def," \t\r\n"); fld; fld = strtok(NULL," \t\r\n") )
    {
        if( _stricmp(fld,"on") == 0 )
        {
            on = 1;
        }
        else if( _stricmp( fld, "off") == 0 )
        {
            on = 0;
        }
        else
        {
            wxString colname( fld );
            colname.Replace("_"," ");
            wxColour c = wxColour(colname);
            pen = symbology->GetPalette()->AddColour( c );
            // TODO: Need some handling here of invalid colour
        }
    }
    if( on < 0 && pen < 0 ) return MISSING_DATA;
    nk = symbology->GetLayerByIdentifier( item );
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
