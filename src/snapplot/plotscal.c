#include "snapconfig.h"
/*
   $Log: plotscal.c,v $
   Revision 1.2  1996/07/12 20:33:13  CHRIS
   Modified to support hidden stations.

   Revision 1.1  1996/01/03 22:28:22  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>

#define _PLOTSCAL_C
#include "plotpens.h"
#include "plotconn.h"
#include "plotscal.h"
#include "plotstns.h"
#include "util/probfunc.h"
#include "snap/snapglob.h"

static char rcsid[]="$Id: plotscal.c,v 1.2 1996/07/12 20:33:13 CHRIS Exp $";

static char autoscale_stnsize;
static char autoscale_txtsize;
static double stn_autosf;
static double txt_autosf;

static char autoscale_ellipse;
static char autoscale_hgterr;
static double ell_autosf;
static double he_autosf;

void init_plot_scales( void )
{
    set_stn_name_size( 1.0, 1 );
    set_stn_symbol_size( 1.0, 1 );
    set_errell_exaggeration( 1.0, 1 );
    set_hgterr_exaggeration( 1.0, 1 );
    /*
    apriori=program_mode == PREANALYSIS ? 1 : 0;
    errconflim=0;
    errconfval=1.0;
    */
    set_confidence_limit();
    offset_spacing = 1.0;
    autospacing = 1.0;
    show_oneway_obs = 1;
    use_default_font = 0;
    show_hidden_stn_obs = 0;
    merge_common_obs = PCONN_DIFFERENT_TYPES;
}


void set_stn_name_size( double size, int autoscl )
{
    if( autoscl )
    {
        txt_autosf = size;
        autoscale_txtsize = 1;
    }
    else
    {
        stn_name_size = size;
        autoscale_txtsize = 0;
    }
}

void get_stn_name_size( double *size, int *autoscl )
{
    if( autoscale_txtsize )
    {
        *size = txt_autosf;
        *autoscl = 1;
    }
    else
    {
        *size = stn_name_size;
        *autoscl = 0;
    }
}


void set_stn_symbol_size( double size, int autoscl )
{
    if( autoscl )
    {
        stn_autosf = size;
        autoscale_stnsize = 1;
    }
    else
    {
        stn_symbol_size = size;
        autoscale_stnsize = 0;
    }
}

void get_stn_symbol_size( double *size, int *autoscl )
{
    if( autoscale_stnsize )
    {
        *size = stn_autosf;
        *autoscl = 1;
    }
    else
    {
        *size = stn_symbol_size;
        *autoscl = 0;
    }
}

void set_errell_exaggeration( double scale, int autoscl )
{
    if( autoscl )
    {
        autoscale_ellipse = 1;
        ell_autosf = scale;
    }
    else
    {
        autoscale_ellipse = 0;
        errell_scale = scale;
    }
}

void get_errell_exaggeration( double *scale, int *autoscl )
{
    if( autoscale_ellipse )
    {
        *scale = ell_autosf;
        *autoscl = 1;
    }
    else
    {
        *scale = errell_scale;
        *autoscl = 0;
    }
}

void set_hgterr_exaggeration( double scale, int autoscl )
{
    if( autoscl )
    {
        autoscale_hgterr = 1;
        he_autosf = scale;
    }
    else
    {
        autoscale_hgterr = 0;
        hgterr_scale = scale;
    }
}

void get_hgterr_exaggeration( double *scale, int *autoscl )
{
    if( autoscale_hgterr )
    {
        *scale = he_autosf;
        *autoscl = 1;
    }
    else
    {
        *scale = hgterr_scale;
        *autoscl = 0;
    }
}


void set_confidence_limit()
{
    double prob;
    int dofd;

    use_confidence_limit = errconflim;
    confidence_limit=errconfval;
    aposteriori_errors = apriori ? 0 : 1;

    if( errconflim )
    {
        prob = (100 - confidence_limit) / 100.0;
        dofd = apriori ? 0 :  dof;
        hgterr_factor = inv_f_distn( prob, 1, dofd );
        hgterr_factor = hgterr_factor > 0.0 ? sqrt(hgterr_factor) : 0.0;
        errell_factor = inv_f_distn( prob, 2, dofd );
        errell_factor = errell_factor > 0.0 ? sqrt(errell_factor*2.0) : 0.0;
    }
    else
    {
        errell_factor = hgterr_factor = confidence_limit;
    }

    if( ! apriori )
    {
        errell_factor *= seu;
        hgterr_factor *= seu;
    }
}

/* Routine to find a "nice" number close to a given value */

static double nice_number( double value )
{
    double nice;
    double *best;
    static double goodvalues[] = { 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0, -1.0 };
    if ( value < 0.0 ) value = -value;
    nice = 0.000001;
    while( nice < value ) nice *= 10.0;
    nice /= 10.0;

    for( best = goodvalues+1; *best > 0.0; best++ )
        if( nice * *best > value ) break;

    best--;

    return nice * *best;
}

double calc_default_error_scale( void )
{
    double dfltscale;
    double cvrmax;
    double h, v;

    if( !got_covariances() ) return 0.0;
    cvrmax = 0.0;
    max_covariance_component( &h, &v );
    if( option_selected(ELLIPSE_OPT) && h > cvrmax ) cvrmax = h;
    if( option_selected(HGTERR_OPT)  && v > cvrmax ) cvrmax = v;
    maximum_relative_covariance( &h, &v );
    if( option_selected(REL_ELL_OPT) && h > cvrmax ) cvrmax = h;
    if( option_selected(REL_HGT_OPT) && v > cvrmax ) cvrmax = v;

    if( errell_factor > hgterr_factor )
    {
        cvrmax *= errell_factor;
    }
    else
    {
        cvrmax *= hgterr_factor;
    }

    max_adjustment_component( &h, &v );
    if( option_selected(HOR_ADJ_OPT) && h > cvrmax ) cvrmax = h;
    if( option_selected(HGT_ADJ_OPT) && v > cvrmax ) cvrmax = v;

    if( cvrmax <= 0 ) cvrmax = 1.0;
    dfltscale = (plot_emax - plot_emin + plot_nmax - plot_nmin) /
                (20.0 * cvrmax);
    return dfltscale;
}


static void autoscale_errors( void )
{
    double dfltscale;
    dfltscale = calc_default_error_scale();
    if( autoscale_ellipse ) errell_scale = nice_number( dfltscale * ell_autosf );
    if( autoscale_hgterr ) hgterr_scale = nice_number( dfltscale * he_autosf );
}


double calc_default_stn_size( void )
{
    return (plot_emax - plot_emin + plot_nmax - plot_nmin) / 500.0;
}

static void autoscale_sizes( void )
{
    double dfltscale;

    dfltscale = calc_default_stn_size();

    if( autoscale_txtsize ) stn_name_size = nice_number( dfltscale * 2 * txt_autosf);
    if( autoscale_stnsize ) stn_symbol_size = nice_number( dfltscale * stn_autosf );

}

void set_plot_range( double emin, double nmin, double emax, double nmax )
{
    plot_emin = emin;
    plot_nmin = nmin;
    plot_emax = emax;
    plot_nmax = nmax;

    autoscale_sizes();
    autoscale_errors();
}

double calc_obs_offset( void )
{
    double offset = offset_spacing;
    if( autospacing )
    {
        offset *= (plot_emax-plot_emin+plot_nmax-plot_nmin)/1000;
    }
    return offset;
}
