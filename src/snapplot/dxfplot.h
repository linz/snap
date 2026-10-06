#ifndef DXFPLOT_H
#define DXFPLOT_H

/*
   $Log: dxfplot.h,v $
   Revision 1.1  1996/01/03 22:16:09  CHRIS
   Initial revision

*/

#include <string>

int open_dxf_file( const std::string &dxfname ) ;
int close_dxf_file( void ) ;
int plot_dxf( void );

#endif

