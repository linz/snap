#ifndef _BINDATA2_H
#define _BINDATA2_H

/*
   $Log: bindata2.h,v $
   Revision 1.3  1996/07/12 20:29:23  CHRIS
   Added date field as a possible output in the residual columns.

   Revision 1.2  1996/02/23 17:06:31  CHRIS
   Added MDE to list of possible residual fields.

   Revision 1.1  1996/01/03 21:56:29  CHRIS
   Initial revision

*/

#ifndef BINDATA2_H_RCSID
#define BINDATA2_H_RCSID "$Id: bindata2.h,v 1.3 1996/07/12 20:29:23 CHRIS Exp $"
#endif

#ifndef _SYMMATRX_H
#include "util/symmatrx.h"   /* For definition of ltmat */
#endif

#ifndef _SURVDATA_H
#include "snapdata/survdata.h"
#endif

enum { HDR_OBSDATA, HDR_VECDATA, HDR_PNTDATA };

typedef struct
{
    double *calc;
    ltmat  calccvr;
    double *res;
    ltmat  rescvr;
    double sch;
    double schvar;
    char   diagonal;
} lsdata;

/* Output fields in residual listing */

enum { OF_FROM, OF_TO, OF_FROMNAME, OF_TONAME,
       OF_HI, OF_HT,
       OF_TYPE, OF_FILENAME, OF_FILENO, OF_LINENO,
       OF_OBS, OF_OBSERR,
       OF_CALC, OF_CALCERR,
       OF_RES, OF_RESERR, OF_ALTRES,
       OF_SRES, OF_REDUNDANCY, OF_FLAGS,
       OF_AZIMUTH, OF_PRJAZ, OF_HGTDIFF,
       OF_ARCDST, OF_SLPDST,
       OF_MDE, OF_SIG, OF_DATE, OF_OBSID,
       OF_COUNT
     };



void print_input_data( FILE *out );
void print_station_recoding( FILE *out );

int max_syserr_params( survdata *sd );

int sum_bindata( int iteration );

int got_vector_data( void );
void calc_residuals( void );
void print_residuals( FILE *out );
void write_observation_csv();


/* Functions relating to the residual listing format */

int define_residual_formats( char *typelist, int add_columns  );
int add_residual_field( const char *code, int width, const char *title1, const char *title2 );

int set_residual_listing_data_type( FILE *out, int itype );
void clear_residual_field_defs(void);

/* Functions used to print residual listing */

void print_residual_line( FILE *out );
void print_residual_title( FILE *out );
void clear_residual_fields( void );
/* Note: value in the following call must be valid until
   print_residual_line is called */
void set_residual_field( int field_id, const char *value );
void clear_residual_field( int field_id );
void set_survdata_fields( survdata *sd );
void set_trgtdata_fields( trgtdata *t, survdata *sd);
char *get_field_buffer( int id );
void set_residual_field_value( int id, int ndp, double value );
void set_residual_field_dms( int id, void *format, double value );

void list_file_location( FILE *out, int file, int lineno );

#endif


