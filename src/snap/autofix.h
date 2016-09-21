#ifndef _AUTOFIX_H
#define _AUTOFIX_H

#define AUTOFIX_HOR 1
#define AUTOFIX_VRT 2

void compile_station_autofix_data();
int station_autofix_constraints( int istn );
int station_autofix_reject( int istn );
void free_station_autofix_data();

#endif
