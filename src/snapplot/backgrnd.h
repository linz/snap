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

#include <optional>
#include <string>
#include <string_view>

void add_background_file( std::string_view fname,
    std::optional<std::string_view> crdsysdef = std::nullopt,
    std::optional<std::string_view> layer = std::nullopt );
void load_background_files( void );
int background_layer_count( void );
/// Returns the name of layer nlayer (counting from 1), or an empty string if there is no such layer.
const std::string &background_layer_name( int nlayer );
int plot_background( map_plotter *plotter, int start );

#endif
