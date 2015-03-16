#ifndef IOSTRING_H
#define IOSTRING_H

/*
   $Log: iostring.h,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:51:15  CHRIS
   Initial revision

*/

#ifndef IOSTRING_H_RCSID
#define IOSTRING_H_RCSID "$Id: iostring.h,v 1.2 2004/04/22 02:35:26 ccrook Exp $"
#endif

/* Definitions of an input and output string structure */

typedef int (*input_string_errfunc)( void *source, int status, const char *message );
typedef struct
{
    char *buffer;   /* The input string data */
    char *ptr;      /* Pointer into the data for the next read operation */
    char *sourcename; /* Name of the source - file name for a file source */
    void *source;   /* Used for the error handler */
    input_string_errfunc report_error;
} input_string_def;

/* Output string def - defines a way of sending strings to some form
   of output device */

typedef int (*output_string_func)( const char *string, void *sink );

typedef struct
{
    void *sink;
    output_string_func write;
} output_string_def;

/* Input string functions.  Return status values are as defined in
   errdef.h, ie 0 = OK, non-zero represent errors.

   The end_of_string function returns 1 if there is no more data available
   in the string, 0 otherwise.  */

void set_input_string_def( input_string_def *s, char *string );
int next_string_field( input_string_def *is, char *buf, int nbuf );
int test_next_string_field( input_string_def *is, const char *test );
int skip_string_field( input_string_def *is );
int double_from_string( input_string_def *is, void *value );
int float_from_string( input_string_def *is, void *value );
int long_from_string( input_string_def *is, void *value );
int int_from_string( input_string_def *is, void *value );
int short_from_string( input_string_def *is, void *value );
int character_from_string( input_string_def *is, void *c );
long get_string_loc( input_string_def *is );
void set_string_loc( input_string_def *is, long loc );
int end_of_string( input_string_def *is );
char *unread_string( input_string_def *is );
void report_string_error( input_string_def *is, int status, const char *message );

int write_output_string( output_string_def *os, const char *s );
int write_output_string2( output_string_def *os, const char *s, int options, const char *prefix );
void output_string_to_file( output_string_def *os, FILE *f );

#define OSW_TRIMR      1
#define OSW_TRIML      2
#define OSW_SKIPBLANK  4

#define OSW_TRIM       3
#define OSW_CLEAN      7

#endif


