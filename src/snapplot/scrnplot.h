#ifndef SCRNPLOT_H
#define SCRNPLOT_H

/*
   $Log: scrnplot.h,v $
   Revision 1.1  1996/01/03 22:30:39  CHRIS
   Initial revision

*/

void view_data( void );
void install_help_file( char *fname );
int get_text_rows( void );
void set_text_rows( int nrows );
void set_screen_mode( int mode );
void add_config_menu_item( char *filename, char *text );

#endif
