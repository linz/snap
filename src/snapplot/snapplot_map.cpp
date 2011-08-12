#include "snapconfig.h"
#include "snapplot_map.hpp"


//extern "C" {
#include "plotpens.h"
#include "plotscal.h"
#include "plotstns.h"
#include "plotconn.h"
#include "backgrnd.h"
//}

// Functions used by map_plotter object based on wxMapDrawer object

static void mapdrawer_line_func( void *plotobj, double px, double py, int pen, int dashed )
{
    wxMapDrawer *drawer = static_cast<wxMapDrawer *>( plotobj );
    if( pen == CONTINUE_LINE )
    {
        drawer->LineTo( MapPoint(px,py));
    }
    else
    {
        drawer->SetLayer( pen );
        if( dashed ) { drawer->SetLineStyle( wxDOT ); }
        else { drawer->SetLineStyle(wxSOLID); }
        drawer->MoveTo( MapPoint(px,py));
    }
}

static void mapdrawer_text_func( void *plotobj, double px, double py, double WXUNUSED(size), int pen, char *text )
{
    wxMapDrawer *drawer = static_cast<wxMapDrawer *>( plotobj );
    TextAlign alignment(1,-1);
    drawer->SetLayer( pen );
    drawer->SetTextStyle( get_station_font_id() );
    // TODO: Sort out text size and alignment properly..
    drawer->PlotText( MapPoint(px,py),alignment,_T(text));
}

static void mapdrawer_ellipse_func( void *plotobj, double px, double py, double a, double b, double az, int pen )
{
    wxMapDrawer *drawer = static_cast<wxMapDrawer *>( plotobj );
    drawer->SetLayer( pen );
    drawer->SetLineStyle(wxSOLID );
    drawer->PlotEllipse( MapPoint(px,py),a,b,az );
}

static void	mapdrawer_symbol_func( void *plotobj, double px, double py, int pen, int symbol )
{
    wxMapDrawer *drawer = static_cast<wxMapDrawer *>( plotobj );
    drawer->SetLayer( pen );
    symbol = get_symbol(symbol);
    drawer->SetSymbol( symbol );
    drawer->PlotSymbol(MapPoint(px,py));
}

static double mapdrawer_symbol_size_func( void * WXUNUSED(plotobj), int WXUNUSED(symbol) )
{
    return 0;
}

SnapplotMap::SnapplotMap()
{
    section = item = 0;
    plotter.line_func = mapdrawer_line_func;
    plotter.text_func = mapdrawer_text_func;
    plotter.ellipse_func = mapdrawer_ellipse_func;
    plotter.symbol_func = mapdrawer_symbol_func;
    plotter.symbol_size_func = mapdrawer_symbol_size_func;

}

SnapplotMap::~SnapplotMap()
{
}

void SnapplotMap::GetMapExtents(MapRect &extents)
{
    double emin, emax, nmin, nmax, e, n;

    // Put a margin around the range of station coordinates to
    // keep map from touching edges of display area..

    get_station_range( &emin, &nmin, &emax, &nmax );

    e = (emin+emax)/2;
    emin = (emax-emin)*1.1/2.0;
    emax = e + emin;
    emin = e - emin;

    n = (nmin+nmax)/2;
    nmin = (nmax-nmin)*1.1/2.0;
    nmax = n + nmin;
    nmin = n - nmin;

    extents.min.x = emin;
    extents.min.y = nmin;
    extents.max.x = emax;
    extents.max.y = nmax;
}

void SnapplotMap::GetLayer( Symbology & WXUNUSED(symbology) )
{
// Assume symbology is already set up in snapplot_layers.cpp
}

// TODO: Sort out plotting the scale ... this should perhaps be a function
// of the map window?
/*
void SnapplotMap::DrawScale() {
   double xmin, xmax, xinc, xtick;
   ccWinUnits txthgt;
   ccPoint plotPt;
   ccMapPoint mapPlotPt;
   xmin = GetViewExtents().min.x;
   xmax = GetViewExtents().max.x;
   xinc = (xmax - xmin)/20;
   if( xinc < 0 ) xinc = -xinc;
   if( xinc < 1.0e-10 ) return;
   xtick = pow( 10.0, floor(log10(xinc)) );
   int major;
   if( xtick * 5  < xinc ) {
      xtick *= 5;
      major = 2;
      }
   else if( xtick * 2 < xinc ) {
      xtick *= 2;
      major = 5;
      }
   else {
      major = 10;
      }
   txthgt = TextSize("X").y/2;
   mapPlotPt.y = GetViewExtents().min.y;
   SetColour( clWhite );
   int ntick = 0;
   for( xmin += xtick; xmin < xmax; xmin += xtick ) {
      mapPlotPt.x = xmin;
      Convert( mapPlotPt, plotPt );
      MoveTo( plotPt );
      plotPt.y -= txthgt;
      ntick++;
      if( ntick % major == 0 ) plotPt.y -= txthgt;
      LineTo( plotPt );
      }
   char buf[80];
   char usekm = 0;
   if( xtick >= 999.9 ) { usekm = 1; xtick /= 1000; }

   int ndp =  - floor( log10( xtick*1.1 ) );
   if( ndp < 0 ) ndp = 0;
   sprintf( buf, "%.*lf %s", ndp, xtick, usekm ? "km" : "m" );
   Convert( GetViewExtents().min, plotPt );
   plotPt.x += txthgt*2;
   plotPt.y -= txthgt*4;
   MoveTo( plotPt );
   WriteText( buf );
   }
   */

void *SnapplotMap::StartDrawing()
{
    rebuild_station_symbols();
    startDrawing = true;
    return 0;
}

void SnapplotMap::EndDrawing( void * WXUNUSED(state) )
{
}

bool SnapplotMap::DrawSome( wxMapDrawer &drawer, void *WXUNUSED(state) )
{
    plotter.plotobj = (void *) (&drawer);

    if( startDrawing  )
    {
        startDrawing = false;
        section = smFirstSection;
        item = 0;
        init_plotting_stations();
        MapRect extents = drawer.GetScale().GetWorldExtents();
        set_plot_range( extents.min.x, extents.min.y, extents.max.x, extents.max.y );
        data_offset = calc_obs_offset();
        pconn_option = merge_common_obs;
        if( show_oneway_obs ) pconn_option |= PCONN_SHOW_ONEWAY_OBS;
    }

    switch( section )
    {
    case smBackground:
        if( item == 0 )
        {
            int pen = get_pen(BACKGROUND_PEN);
            wxColour colour = drawer.GetSymbology().LayerColour(pen);
            drawer.Clear( colour );
        }
        item = plot_background( &plotter, item );
        break;

    case smStations: item = plot_stations( &plotter, item, 0);
        break;

    case smObs: item = plot_connections(  &plotter, item, pconn_option, data_offset, 0 );
        break;

    case smCovariances: item = plot_covariances(  &plotter, item );
        break;

    case smHgtErrors: item = plot_height_errors(  &plotter, item );
        break;

    case smAdjustments: item = plot_adjustments(  &plotter, item );
        break;

    case smStnNames: item = plot_station_names(  &plotter, item );
        break;

        // Replot highlighted observations to ensure that they are
        // shown on top....

    case smHltStns: item = plot_stations(  &plotter, item, 1 );
        break;

    case smHltObs: item = plot_connections(  &plotter, item, pconn_option,
                              data_offset, PCONN_REDRAW_HIGHLIGHT );
        break;

    case smDrawScale:
        // DrawScale();
        item = ALL_DONE;
        break;

    case smRedrawObs:
        // TODO: Redrawing observations to highlight ... may need a different approach.
        /*
         if( item == 0 ) redrawPen = NextRedrawDataPen();
         if( redrawPen ) {
             item = plot_connections( item, pconn_option,
                 data_offset, redrawPen );
             if( item == ALL_DONE ) item = 0;
             }
         else {
             item = ALL_DONE;
             }
         */
        item = ALL_DONE;
        break;

    default:
        section = smMapDone;
        break;
    }

    if( section != smMapDone && item == ALL_DONE ) { section++; item = 0; }

    return section != smMapDone;
}

