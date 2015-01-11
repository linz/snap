#ifndef _PLOTSCAL_H
#define _PLOTSCAL_H

/*
   $Log: plotscal.h,v $
   Revision 1.1  1996/01/03 22:28:54  CHRIS
   Initial revision

*/

#ifndef PLOTSCAL_H_RCSID
#define PLOTSCAL_H_RCSID "$Id: plotscal.h,v 1.1 1996/01/03 22:28:54 CHRIS Exp $"
#endif

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
SCOPE char use_default_font;
SCOPE double stn_symbol_size;
SCOPE double errell_factor;
SCOPE double errell_scale;
SCOPE double hgterr_factor;
SCOPE double hgterr_scale;
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
