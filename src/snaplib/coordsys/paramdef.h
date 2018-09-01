#ifndef PARAMDEF_H
#define PARAMDEF_H

/*
   $Log: paramdef.h,v $
   Revision 1.1  1995/12/22 16:59:09  CHRIS
   Initial revision

*/

#ifndef IOSTRING_H
#include "util/iostring.h"
#endif

#include "stddef.h"

/* Definition of parameters of an object.  Used to create lists of parameters which
   can be used for input or output of a system definition */

typedef struct
{
    const char *name;     /* Name of parameter - used for descriptive output */
    const char *code;     /* Code - not used at present */
    size_t offset;     /* Offset into the objects data structure */

    /* Read, write, and print functions.  print is a nicely formatted
       version of write.  Read and write are inverse operations.

       Return status should be as defined in errdef.h */

    int (*read)( input_string_def *is, void *address );
    int (*write)( output_string_def *os, void *address );
    int (*print)( output_string_def *os, void *address );
} param_def;

/* Useful routines to be used in parameter definitions (reading, writing.. ) */

int print_int( output_string_def *os, void *address );
int print_short( output_string_def *os, void *address );
int print_long( output_string_def *os, void *address );
int print_double0( output_string_def *os, void *address );
int print_double3( output_string_def *os, void *address );
int print_double6( output_string_def *os, void *address );
int print_radians( output_string_def *os, void *address );
int print_latitude( output_string_def *os, void *address );
int print_longitude( output_string_def *os, void *address );
void print_param_list( output_string_def *os, param_def *prms, int nprm,
                       void *base, const char *prefix );

int read_radians( input_string_def *is, void *address );
int read_param_list( input_string_def *is, param_def *prms, int nprm, void *base );



/* Macro for determining offset of member in a structure */

#ifndef OFFSET_OF 
#define OFFSET_OF(m,t) offsetof(t,m)
#endif

/* Macro to obtain the address of an offset into a structure */

#ifndef OFFSET_ADDRESS
#define OFFSET_ADDRESS(base,offset) \
    ( (void *) ((char *)base + offset) )
#endif

/* Macro to determine the number of an elements in an array */

#ifndef COUNT_OF
#define COUNT_OF(array) \
    ( (sizeof(array))/(sizeof(array[0])) )
#endif

#endif
