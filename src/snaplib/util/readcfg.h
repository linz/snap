#ifndef _READCFG_H
#define _READCFG_H

/*
   $Log: readcfg.h,v $
   Revision 1.2  2004/04/22 02:35:27  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:55:50  CHRIS
   Initial revision

*/

#ifndef _ERRDEF_H
#include "util/errdef.h"
#endif

/* Header file for readcfg.c  - code to read a configuration file */

/* A configuration file */

#define CFG_FILE_NAME_LEN 256

typedef struct
{
    FILE *f;       			/* The file handle */
    char *name;                 /* The file name */
    char *dirname;              /* The directory containing the config file (or blank if none)  */
    int lineno;			/* The current line number */
    int errcount;                 /* The number of errors encountered */
    int read_options;             /* Options controlling reading of config file */
    int command_flag;             /* Flag defining which commands are acceptable */
    int ignore_flag;             /* Flag defining which commands are to be ignored */
    char comment_char;		/* Text after this character is ignored */
    char abort;
    char *buffer;               /* Text buffer for reading options */
    int nbuffer;                /* Size of buffer */
} CFG_FILE;


/* Definition of an item in a configuration file - the user supplies an
   array of items to the read_config_file routine */

typedef int (*config_store_func)( CFG_FILE *cfg, char *valst, void *value, int vallen, int code);

typedef struct
{
    const char *option;   /* Address of option name */
    void *value;    /* Address into which value is to be stored */
    int offset;     /* Offset from address pointed to */
    int  vallen;    /* Length allocated to store value for strings..*/
    config_store_func store;  /* Optional routine to store value */
    int flags;      /* Flag defining required options  - on exit
			   identifies which options were present */
    int code;       /* User defined integer code */
} config_item;

/* Definition of NULL function for storage - default is to store as a
   string of up to length-1 characters. The store function should return
   one of the following codes defined in errdef.h...

      OK            - data read OK, no action required
      INVALID_DATA  - send invalid data message
      MISSING_DATA  - send missing data
      NO_MORE_DATA  - exit the read_config_file function.

      */

#define STORE_AS_STRING ( (config_store_func) 0 )
#define CFG_ABSOLUTE (-1)


/* Definition of flags for configuration items */

#define CFG_REQUIRED    1   /* The option is required in the configuration file */
#define CFG_ONEONLY     2   /* Duplicate definitions are invalid */
#define CFG_PRESENT     4   /* Set by the readcfg routine if the item is present */
#define CFG_END         8   /* The item will terminate the read_config_file function */
#define CFG_USERFLAG1   (1*256)
#define CFG_USERFLAG2   (2*256) 
#define CFG_USERFLAG3   (4*256)
#define CFG_USERFLAG4   (8*256)

#define CFG_INIT_ITEMS    1 /* Read option flags */
#define CFG_CHECK_MISSING 2
#define CFG_IGNORE_BAD    4
#define CFG_IGNORE_COMMENT 8
#define CFG_POSITIONAL_COMMENT 16
#define CFG_SET_PATH   32

#define CFG_DFLT_BUFFER_SIZE 1024

#ifndef DEFAULT_ERROR_HANDLER
#define DEFAULT_ERROR_HANDLER ( (int (*)()) 0 )
#endif



/* Macro to define the offset of a member m of structure referenced by
   typedef t (can be called as offsetof( struct s, m) */

#define OFFSETOF(t,m) ((int)(&((t*)0)->m))

#define FIELD_DELIMS " \t\n="


/* Basic routines for reading a configuration file */

CFG_FILE *open_config_file( const char *name, char comment_char );
void close_config_file( CFG_FILE *cfg );

int set_config_read_options( CFG_FILE *cfg, int options );
int set_config_command_flag( CFG_FILE *cfg, int flag );
int set_config_ignore_flag( CFG_FILE *cfg, int flag );
void initiallize_config_items( config_item item[] );
int read_config_file( CFG_FILE *cfg, config_item item[] );
int report_missing_config_items( CFG_FILE *cfg, config_item item[] );
int send_config_error( CFG_FILE *cfg, int errstat, const char *errmsg );
char *get_config_line( CFG_FILE *cfg, char *line, int nch, int *noverrun );
char *get_config_location( CFG_FILE *cfg );
char *get_config_filename( CFG_FILE *cfg );
char *get_config_directory( CFG_FILE *cfg );

void abort_config_file( CFG_FILE *cfg );
void cancel_config_abort( CFG_FILE *cfg );


/* Items mainly of use in defining configuration file items.  In each
   case the length and code parameters are ignored, except for
   readcfg_boolean, for which the length is a bit mask defining which
   bits of an unsigned char variable are modified. */

int readcfg_int( CFG_FILE *cfg, char *str, void *value, int length, int code );
int readcfg_short( CFG_FILE *cfg, char *str, void *value, int length, int code );
int readcfg_long( CFG_FILE *cfg, char *str, void *value, int length, int code );
int readcfg_float( CFG_FILE *cfg, char *str, void *value, int length, int code );
int readcfg_double( CFG_FILE *cfg, char *str, void *value, int length, int code );
int readcfg_boolean( CFG_FILE *cfg, char *str, void *value, int length, int code );

#endif  /* READCFG_H defined */

