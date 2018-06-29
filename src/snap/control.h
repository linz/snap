#ifndef _CONTROL_H
#define _CONTROL_H

/*
   $Log: control.h,v $
   Revision 1.1  1996/01/03 21:58:21  CHRIS
   Initial revision

*/

int read_command_file( const char *fname );
int read_command_file_constraints( const char *command_file );
int read_configuration_file( const char *fname );
const char *find_configuration_file( const char *name );
int process_default_configuration( void );

#endif
