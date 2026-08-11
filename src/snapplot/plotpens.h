#ifndef _PLOTPENS_H
#define _PLOTPENS_H

/*
   $Log: plotpens.h,v $
   Revision 1.1  1996/01/03 22:28:01  CHRIS
   Initial revision

*/

enum { OTHER_PEN = 0,
       FREE_STN_PEN,
       FIXED_STN_PEN,
       HOR_FIXED_STN_PEN,
       VRT_FIXED_STN_PEN,
       REJECTED_STN_PEN,
       TEXT_PEN,
       ELLIPSE_PEN,
       REL_ELL_PEN,
       HOR_ADJ_PEN,
       HGTERR_PEN,
       REL_HGT_PEN,
       HGT_ADJ_PEN,
       HIGHLIGHT_PEN,
       SELECTED_PEN,
       BACKGROUND_PEN,
       N_BASE_PENS
     };


enum { OTHER_OPT = 0,
       FREE_STN_OPT,
       FIXED_STN_OPT,
       HOR_FIXED_STN_OPT,
       VRT_FIXED_STN_OPT,
       REJECTED_STN_OPT,
       SYMBOL_OPT,
       CODE_OPT,
       NAME_OPT,
       ELLIPSE_OPT,
       REL_ELL_OPT,
       HOR_ADJ_OPT,
       HGTERR_OPT,
       REL_HGT_OPT,
       HGT_ADJ_OPT,
       USED_OBS_OPT,
       REJECTED_OBS_OPT,
       UNUSED_OBS_OPT,
       N_BASE_OPTS
     };

enum
{
    FREE_STN_SYM = 0,
    FIXED_STN_SYM,
    HOR_FIXED_STN_SYM,
    VRT_FIXED_STN_SYM,
    REJECTED_STN_SYM,
    N_STN_SYM
};

typedef struct
{
    const char *name;
    int  *pen;
    int  *opt;
    int  datapen;
} key_def;

typedef struct
{
    double x;
    double y;
} symbolpoint;

// Returns true if the list was freshly built, false if reused from cache.
bool setup_data_layers( int ndatapens, const char **datapennames, const char *header, int sorted  );
// Points data_user_layers at classification class_type's list (building and
// caching it first if needed), the exclusive Colour-by mode path. Returns
// true if it was freshly built, false if reused from cache or class_type has
// no values.
bool setup_classification_pens_layers( int class_type );
// Triggers a full symbology rebuild, e.g. after a Display-by toggle changes
// which lists should be included.
void rebuild_displayby_symbology();
void setup_station_layers( int class_id );
void reset_data_user_layers( bool is_on );
// Resets every station_user_layers row's checkbox to is_on.
void reset_station_user_layers( bool is_on );
// Evicts whichever station classification list is currently active, without needing to know its class_id.
void invalidate_active_station_class_layer_cache();
// Resets every data_type_layers row's colour back to the default palette entry.
void reset_data_type_layer_colours();
// Evicts a cached colour-by list by its header text, forcing a rebuild next time it's selected.
void invalidate_data_user_layer_cache( const char *header );
// Evicts whichever colour-by list is currently active, without needing to know its header.
void invalidate_active_data_user_layer_cache();
// Returns the pen-type code (DPEN_BY_* or a classification id) of the active observation colour-by mode.
int get_data_pen_type();
void rebuild_station_symbols();
int get_pen( int item_code );
int pen_visible( int pen );
int get_symbol( int item_code );
int get_symbol_points( int symbol_id, symbolpoint *ptlist, int maxpts );
void set_pen_colour_range();
int pen_colour( int pen );
int pen_count( void );
int pen_selected( int pen );
const char *pen_name( int pen );
bool pen_has_colour( int pen );
void get_pen_colour( int ipen, unsigned char &red, unsigned char &green, unsigned char &blue );

int station_class_pen( int cvalue );
int background_pen( int layer_id );
const char *get_station_font();
void set_station_font( const char *fontdef );
int get_station_font_id();

void init_options( void );
int option_selected( int option_code );
int data_pen( int dpen );
int datatype_selected( int datatype );
// Whether the given data file's checkbox is currently on, independent of
// whatever mode is active for Colour by.
bool filetype_selected( int file );
// Whether the checkbox for classification class_type's value value_id is
// currently on. Callers should only call this for a class_type they already
// know is active, via is_displayby_enabled() or get_data_pen_type().
bool classification_value_selected( int class_type, int value_id );
int select_display_options( void );
int background_option( int layer_id );

int read_key_definition( char *def );
void print_key( FILE *out, const char *prefix );

#endif
