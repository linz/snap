#ifndef _PLOTFUNC_H
#define _PLOTFUNC_H

/* Definitions of functions etc used for plotting */

#define LINE(plotter,x,y,pen) map_plotter_line(plotter,x,y,pen,0)
#define DASHED_LINE(plotter,x,y,pen) map_plotter_line(plotter,x,y,pen,1)
#define PLOTTEXT map_plotter_text
#define ELLIPSE map_plotter_ellipse
#define SYMBOL map_plotter_symbol

/* Continue line code */

#define CONTINUE_LINE (-1)

/* To facilitate background processing draw functions are called repeatedly
   until they return status ALL_DONE.  Otherwise they should always be called
   with the previous return value (and initially with 0). */

#define ALL_DONE (-1)

typedef struct
{
    void *plotobj;
    void (*line_func)( void *plotter, double px, double py, int pen, int dashed );
    void (*text_func)( void *plotter, double px, double py, double size, int pen, char *text );
    void (*ellipse_func)( void *plotter, double px, double py, double a, double b, double az, int pen );
    void (*symbol_func)( void *plotter, double px, double py, int pen, int symbol );
    double (*symbol_size_func)( void *plotter, int symbol );
} map_plotter;

/* To facilitate background processing draw functions are called repeatedly
   until they return status ALL_DONE.  Otherwise they should always be called
   with the previous return value (and initially with 0). */

/* Drawing functions */

void map_plotter_line( map_plotter *plotter, double px, double py, int pen, int dashed );
void map_plotter_text( map_plotter *plotter, double px, double py, double size, int pen, char *text );
void map_plotter_ellipse( map_plotter *plotter, double px, double py, double a, double b, double az, int pen );
void map_plotter_symbol( map_plotter *plotter, double px, double py, int pen, int symbol );
double map_plotter_symbol_size( map_plotter *plotter, int symbol );

#endif
