#ifndef _CONTROL_H
#define _CONTROL_H

/*
   $Log: control.h,v $
   Revision 1.1  1996/01/03 21:58:21  CHRIS
   Initial revision

*/

#ifndef CONTROL_H_RCSID
#define CONTROL_H_RCSID "$Id: control.h,v 1.1 1996/01/03 21:58:21 CHRIS Exp $"
#endif

int read_command_file( char *fname );
int read_configuration_file( char *fname );
char *find_configuration_file( char *name, int use_cmd_dir, int use_home_dir );
int process_default_configuration( void );

#endif
