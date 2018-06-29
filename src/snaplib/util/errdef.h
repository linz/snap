#ifndef _ERRDEF_H
#define _ERRDEF_H

/*
   $Log: errdef.h,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:58:53  CHRIS
   Initial revision

*/

/* Error codes for C routines:
   The conventions used are

   0     = No error
   1..31 = Informational error - normal program conditions
   32-63 = Warning error
   64-95 = Fatal error - terminate processing

*/


#define OK                0
#define NO_MORE_DATA      1
#define EOF_ENCOUNTERED   2

#define INFO_ERROR       16

#define WARNING_ERROR    32
#define FILE_OPEN_ERROR  33
#define FILE_READ_ERROR  34
#define FILE_WRITE_ERROR 35
#define UNEXPECTED_EOF   36
#define SYNTAX_ERROR     37
#define INVALID_DATA     38
#define MISSING_DATA     39
#define INCONSISTENT_DATA 40
#define TOO_MUCH_DATA     41
#define OPERATION_ABORTED 42

#define FATAL_ERROR      64
#define MEM_ALLOC_ERROR  65
#define INTERNAL_ERROR   66

#define REPORTABLE_ERROR(sts)        ((sts) & (INFO_ERROR | WARNING_ERROR | FATAL_ERROR))
#define INFO_ERROR_CONDITION(sts)    (((sts) & INFO_ERROR) && !((sts) & (FATAL_ERROR | WARNING_ERROR)))
#define FATAL_ERROR_CONDITION(sts)    ((sts) & FATAL_ERROR)
#define WARNING_ERROR_CONDITION(sts)  ((sts) & WARNING_ERROR)

/* Define a default error handler - takes three parameters,
   1) An integer error status
   2) An error description (if NULL, a message is generated based on
      the error condition)
   3) A qualifier (e.g. file location) (if NULL nothing is printed)

   The null error handler does not generate output for warning and 
   info errors.

   The handler writes a message to STDERR, and for fatal errors,
   terminates the process.

   The user can supply an alternative output file, or an alternative
   program to report errors.  If a fatal error condition is encountered
   the program always exits. */

typedef int (*errhandler_type)( int sts, const char *msg1, const char *msg2 );

int handle_error( int sts, const char *mess1, const char *mess2 );
int default_error_handler( int sts, const char *mess1, const char *mess2 );
int null_error_handler( int sts, const char *mess1, const char *mess2 );
errhandler_type set_error_handler( errhandler_type errhndler );
int get_error_count( void );

/* Settings for default error handler - output file, prefix (true or false),
 * and reporting level (INFO_ERROR, WARNING_ERROR, FATAL_ERROR ), and location
 * used for second line of error message if not null in call.
 *
 * Each returns old value, apart from location.
 */

FILE * set_error_file( FILE *errfile );
int  set_error_level( int level );
int set_error_prefix( int prefix );

void set_error_location( const char *loc );


#define NO_MESSAGE ((char *) 0)
#define DEFAULT_ERROR_HANDLER ( (errhandler_type) 0 )
#define DEFAULT_ERROR_FILE    ( (FILE *) 0 )

#endif
