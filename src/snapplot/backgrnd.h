#ifndef BACKGRND_H
#define BACKGRND_H

/*
   $Log: backgrnd.h,v $
   Revision 1.1  1996/01/03 22:15:32  CHRIS
   Initial revision

*/

#ifndef _PLOTFUNC_H
#include "plotfunc.h"
#endif

void add_background_file( const char *fname, char *crdsysdef, char *layer );
void load_background_files( void );
int background_layer_count( void );
char *background_layer_name( int nlayer );
int plot_background( map_plotter *plotter, int start );

#endif
