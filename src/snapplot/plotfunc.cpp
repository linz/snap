#include "snapconfig.h"
#include "plotfunc.h"


void map_plotter_line( map_plotter *plotter, double px, double py, int pen, int dashed )
{
    (plotter->line_func)(plotter->plotobj, px,  py,  pen, dashed );
}

void map_plotter_text( map_plotter *plotter, double px, double py, double size, int pen, char *text )
{
    (plotter->text_func)(plotter->plotobj,  px,  py,  size,  pen,  text );
}

void map_plotter_ellipse( map_plotter *plotter, double px, double py, double a, double b, double az, int pen )
{
    (plotter->ellipse_func)(plotter->plotobj,  px,  py,  a,  b,  az,  pen );
}

void map_plotter_symbol( map_plotter *plotter, double px, double py, int pen, int symbol )
{
    if( plotter->symbol_func )
    {
        (plotter->symbol_func)(plotter->plotobj,  px,  py,  pen, symbol );
    }
}

double map_plotter_symbol_size( map_plotter *plotter, int symbol )
{
    double size = 0.0;
    if( plotter->symbol_size_func )
    {
        size = (plotter->symbol_size_func)(plotter->plotobj,  symbol );
    }
    return size;
}

