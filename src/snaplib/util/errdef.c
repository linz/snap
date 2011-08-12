#include "snapconfig.h"
/*
   errdef.c : part of the gina_pci translator
   Copyright: Department of Survey and Land Information
              New Zealand

   Author:  Chris Crook, Research and Development Group
   Revision: October 1990

*/

/*
   $Log: errdef.c,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:58:16  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util/errdef.h"    /* Error code definitions */

static char rcsid[]="$Id: errdef.c,v 1.2 2004/04/22 02:35:24 ccrook Exp $";

static errhandler_type user_error_handler = (errhandler_type) 0;
static FILE *error_file = NULL;
static int error_count = 0;

#define LOCATION_LEN 256
static char location[LOCATION_LEN] = {0};

int handle_error( int sts, const char *mess1, const char *mess2 )
{
    FILE *out;
    if (!FATAL_ERROR_CONDITION(sts) & !WARNING_ERROR_CONDITION(sts)) return sts;
    error_count++;
    if (mess1==NULL) switch (sts)
        {
        case FILE_OPEN_ERROR:  { mess1 = "Error opening file"; break;}
        case FILE_READ_ERROR:  { mess1 = "Error reading file"; break;}
        case FILE_WRITE_ERROR: { mess1 = "Error writing file"; break;}
        case UNEXPECTED_EOF:   { mess1 = "End of file encountered"; break;}
        case SYNTAX_ERROR:     { mess1 = "Syntax error"; break; }
        case INVALID_DATA:     { mess1 = "Invalid data error"; break; }
        case MISSING_DATA:     { mess1 = "Missing data"; break; }
        case INCONSISTENT_DATA: {mess1 = "Inconsistent data"; break;}
        case TOO_MUCH_DATA:    { mess1 = "Too much data"; break; }
        case MEM_ALLOC_ERROR:  { mess1 = "Memory allocation error"; break; }
        case INTERNAL_ERROR:   { mess1 = "Internal program error"; break;}
        case OPERATION_ABORTED: { mess1 = "Aborted by user"; break;}
        default:               { mess1 = "Undefined error"; }
        }
    if( mess2 == NULL && location[0] ) mess2 = location;
    if( user_error_handler )
    {
        (*user_error_handler)( sts, mess1, mess2 );
    }
    else
    {
        out = error_file ? error_file : stderr;
        if (FATAL_ERROR_CONDITION(sts)) fprintf(out,"\nFatal error: ");
        else fprintf(out,"\nWarning: ");
        fprintf(out,"%s\n",mess1);
        if (mess2 != NULL) fprintf(out,"%s\n",mess2);
    }
    if (FATAL_ERROR_CONDITION(sts)) exit(sts);
    return sts;
}

void set_error_file( FILE *err )
{
    error_file = err;
}

errhandler_type set_error_handler( errhandler_type errhndler )
{
    errhandler_type olderrhndler;
    olderrhndler = errhndler;
    user_error_handler = errhndler;
    return olderrhndler;
}

int get_error_count( void )
{
    int errc = error_count;
    error_count = 0;
    return errc;
}

void set_error_location( const char *loc )
{
    if( loc )
    {
        strncpy(location,loc,LOCATION_LEN-1);
        location[LOCATION_LEN-1] = 0;
    }
    else
    {
        location[0] = 0;
    }
}
/* default_error_handler is provided for consistency with the
   previous error handling library */

int default_error_handler( int sts, const char *mess1, const char *mess2 )
{
    int result;
    errhandler_type saved_handler;
    saved_handler = set_error_handler( (errhandler_type) 0 );
    result = handle_error( sts, mess1, mess2 );
    set_error_handler( saved_handler );
    return result;
}

