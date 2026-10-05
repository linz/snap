#ifndef _CONTROL_H
#define _CONTROL_H

#include <string>
#include <optional>

/*
   $Log: control.h,v $
   Revision 1.1  1996/01/03 21:58:21  CHRIS
   Initial revision

*/

int read_command_file( const std::string &fname );
int read_command_file_constraints( const std::string &fname );
int read_configuration_file( const std::string &fname );
std::optional<std::string> find_configuration_file( const std::string &name );
int process_default_configuration( void );

#endif
