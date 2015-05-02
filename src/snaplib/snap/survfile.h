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

#include "snapdata/datatype.h"
#include "snapdata/stnrecode.h"

enum { SNAP_FORMAT, GB_FORMAT, CSV_FORMAT, SINEX_FORMAT };

typedef struct
{
    char *name;
    int format;
    char *subtype;
    double errfct;
    char *recodefile;
    double mindate;
    double maxdate;
    long nnodate;
    long obscount[NOBSTYPE];
    unsigned char usage;
    stn_recode_map *recode;
} survey_data_file;

int  add_data_file( char *name, int format, char *subtype, double errfct, char *recode );
survey_data_file *survey_data_file_ptr( int ifile );
char *survey_data_file_name( int ifile );
int survey_data_file_id( char *name );
int survey_data_file_count( void );
void survey_data_file_dates( double *mindate, double *maxdate, int *nnodate );
double survey_data_file_errfct( int ifile );

void delete_survey_file_recodes();
void delete_survey_file_list();

#ifdef _BINFILE_H
void dump_filenames( BINARY_FILE *b );
int reload_filenames( BINARY_FILE *b );
#endif

#endif

