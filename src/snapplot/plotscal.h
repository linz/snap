#ifndef _PLOTSCAL_H
#define _PLOTSCAL_H

/*
   $Log: plotscal.h,v $
   Revision 1.1  1996/01/03 22:28:54  CHRIS
   Initial revision

*/

void set_plot_range( double emin, double nmin, double emax, double nmax );
void set_stn_name_size( double size, int autoscl );
void get_stn_name_size( double *size, int *autoscl );
void set_stn_symbol_size( double size, int autoscl );
void get_stn_symbol_size( double *size, int *autoscl );
void set_errell_exaggeration( double scale, int autoscl );
void get_errell_exaggeration( double *scale, int *autoscl );
void set_hgterr_exaggeration( double scale, int autoscl );
void get_hgterr_exaggeration( double *scale, int *autoscl );
void set_confidence_limit();
double calc_default_stn_size( void );
double calc_default_error_scale( void );
double calc_default_relative_ellipse_scale( void );
double calc_default_height_error_scale( void );
double calc_default_relative_height_scale( void );
double calc_default_hor_adjustment_scale( void );
double calc_default_vrt_adjustment_scale( void );
double calc_obs_offset( void );

void init_plot_scales( void );

/* Information relating to the size of the plot */

#ifdef _PLOTSCAL_C
#define SCOPE
#else
#define SCOPE extern
#endif

SCOPE double plot_emin, plot_nmin, plot_emax, plot_nmax;

SCOPE double stn_name_size;
// TODO: fix this: use_default_font is set from the use_fixed_size_font config
// command but never read anywhere to actually affect font rendering.
SCOPE char use_default_font;
SCOPE double stn_symbol_size;
SCOPE double errell_factor;
SCOPE double errell_scale;
SCOPE double hgterr_factor;
SCOPE double hgterr_scale;
// Scale factor for "Relative ellipse", independent of errell_scale so that
// toggling one doesn't resize the other.
SCOPE double rel_errell_scale;
// Scale factor for "Relative hgt err", independent of hgterr_scale for the same reason.
SCOPE double rel_hgterr_scale;
// Scale factors for the Hor/Vrt adjustment vectors, also independent of every
// scale above: adjustment magnitudes are typically much larger than
// covariance-based error magnitudes and would otherwise dominate a shared
// auto-scale, or resize unrelated items, whenever more than one is selected.
SCOPE double adjhor_scale;
SCOPE double adjvrt_scale;
SCOPE double confidence_limit;
SCOPE char use_confidence_limit;
SCOPE char aposteriori_errors;
SCOPE char show_oneway_obs;
SCOPE char merge_common_obs;
SCOPE char show_hidden_stn_obs;
SCOPE double offset_spacing;
SCOPE char autospacing;

#undef SCOPE

#endif
