#ifndef _DATAFILE_H
#define _DATAFILE_H

/*
   $Log: datafile.h,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:56:46  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <optional>
#include <string>

#ifndef IOSTRING_H
#include "util/iostring.h"
#endif

struct DATAFILE
{
    std::string fname;
    FILE *f;
    long startloc;
    long startlineno;
    long reclineno;
    long lineno;
    int  maxreclen;
    int  errcount;
    char unicode;
    char *inrec;
    char *inrecptr;
    char *lastrecptr;
    char comment_char;
    char continuation_char;
    char quote_char;
    std::optional<input_string_def> instr; ///< constructed fresh by each df_input_string() call
};


struct datafile_loc
{
    long loc;
    long line;
};

int   df_data_file_default_reclen( int newlen );
DATAFILE *df_open_data_file( const char *fname, const char *description ) ;
std::string df_file_name( DATAFILE *d );
void  df_close_data_file( DATAFILE *d ) ;
void  df_set_data_file_comment( DATAFILE *d, char comment );
void  df_set_data_file_quote( DATAFILE *d, char quote );
void  df_set_data_file_continuation( DATAFILE *d, char continuation );
int df_skip_to_blank_line( DATAFILE *d );
int df_read_data_file( DATAFILE *d );
char *df_rest_of_line( DATAFILE *d );

/// Constructs (or reconstructs) d's input_string_def to view its current
/// record, wired to report errors via df_data_file_error(). Called fresh
/// each time the current record needs a read cursor - the previous return
/// value is no longer valid once this is called again.
input_string_def &df_input_string( DATAFILE *d );

long  df_line_number( DATAFILE *d ) ;
int df_data_file_error( DATAFILE *d, int sts, const char *errmsg ) ;
int df_data_file_errcount( DATAFILE *d );
void  df_save_data_file_loc( DATAFILE *d, datafile_loc *dl );
void  df_reset_data_file_loc( DATAFILE *d, datafile_loc *dl );

/* The following functions should really be handled by the input string def .. */

int df_skip_character( DATAFILE *d );
int df_read_field( DATAFILE *d, char *field, int nfld );
int df_read_code( DATAFILE *d, char *field, int nfld );
int df_read_int( DATAFILE *d, int *v );
int df_read_short( DATAFILE *d, short *v );
int df_read_long( DATAFILE *d, long *v );
int df_read_double( DATAFILE *d, double *v );
int df_read_degangle( DATAFILE *d, double *v );
int df_read_dmsangle( DATAFILE *d, double *v );
int df_read_hpangle( DATAFILE *d, double *v );
int df_read_rest( DATAFILE *d, char *line, int nline );
void  df_reread_field( DATAFILE *d );
int df_end_of_line( DATAFILE *d );

#endif
