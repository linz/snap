#ifndef _PLOTPENS_H
#define _PLOTPENS_H

/*
   $Log: plotpens.h,v $
   Revision 1.1  1996/01/03 22:28:01  CHRIS
   Initial revision

*/

#ifndef PLOTPENS_H_RCSID
#define PLOTPENS_H_RCSID "$Id: plotpens.h,v 1.1 1996/01/03 22:28:01 CHRIS Exp $"
#endif


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
    char *name;
    int  *pen;
    int  *opt;
    int  datapen;
} key_def;

void setup_data_layers( int ndatapens, char **datapennames, char *header, int sorted  );
void setup_station_layers( int class_id );
void rebuild_station_symbols();
int get_pen( int item_code );
int pen_visible( int pen );
int get_symbol( int item_code );
void set_pen_colour_range();
int pen_colour( int pen );
int pen_count( void );
int pen_selected( int pen );
char *pen_name( int pen );
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
int select_display_options( void );
int background_option( int layer_id );

int read_key_definition( char *def );
void print_key( FILE *out, char *prefix );

#endif
