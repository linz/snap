#ifndef _PROGRESS_H
#define _PROGRESS_H

/*
   $Log: progress.h,v $
   Revision 1.2  2004/04/22 02:35:27  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:55:06  CHRIS
   Initial revision

*/

#ifndef PROGRESS_H_RCSID
#define PROGRESS_H_RCSID "$Id: progress.h,v 1.2 2004/04/22 02:35:27 ccrook Exp $"
#endif

void init_progress_meter( long total_size );
void update_progress_meter( long progress );
void end_progress_meter( void );

void init_file_display( FILE *infile );
void update_file_display( void );
void end_file_display( void );

typedef struct
{
    void (*init_meter)( long total_size );
    void (*update_meter)( long progress );
    void (*end_meter)( void );
} progress_meter_def;

void install_progress_meter( progress_meter_def *meter );
void uninstall_progress_meter( void );
void install_default_progress_meter( void );

#endif
