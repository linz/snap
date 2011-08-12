#ifndef _SURVFILE_H
#define _SURVFILE_H

/*
   $Log: survfile.h,v $
   Revision 1.2  1998/06/15 02:27:45  ccrook
   Modified to handle long integer number of observations

   Revision 1.1  1995/12/22 18:37:41  CHRIS
   Initial revision

*/

#ifndef SURVFILE_H_RCSID
#define SURVFILE_H_RCSID "$Id: survfile.h,v 1.2 1998/06/15 02:27:45 ccrook Exp $"
#endif

#ifndef _DATATYPE_H
#include "snapdata/datatype.h"
#endif

enum { SNAP_FORMAT, GB_FORMAT, CSV_FORMAT };

typedef struct
{
    char *name;
    int format;
    char *subtype;
    double errfct;
    long obscount[NOBSTYPE];
    unsigned char usage;
} survey_data_file;

int  add_data_file( char *name, int format, char *subtype, double errfct );
survey_data_file *survey_data_file_ptr( int ifile );
char *survey_data_file_name( int ifile );
int survey_data_file_id( char *name );
int survey_data_file_count( void );
double survey_data_file_errfct( int ifile );

#ifdef _BINFILE_H
void dump_filenames( BINARY_FILE *b );
int reload_filenames( BINARY_FILE *b );
#endif

#endif

