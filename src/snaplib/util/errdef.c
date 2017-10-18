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

static errhandler_type user_error_handler = (errhandler_type) 0;
static FILE *error_file = NULL;
static int error_level = 0;
static int use_prefix = 1;
static int error_count = 0;

#define LOCATION_LEN 256
static char location[LOCATION_LEN] = {0};


int default_error_handler( int sts, const char *mess1, const char *mess2 )
{
    FILE *out = error_file ? error_file : stderr;
    if( use_prefix )
    {
        if (FATAL_ERROR_CONDITION(sts)) 
        {
            fprintf(out,"\nFatal error: ");
        }
        else if( WARNING_ERROR_CONDITION(sts)) 
        {
            fprintf(out,"\nWarning: ");
        }
        else 
        {
            fprintf(out,"\nInformation: ");
        }
    }
    fprintf(out,"%s\n",mess1);
    if (mess2 != NULL) fprintf(out,"%s\n",mess2);
    return sts;
}

int null_error_handler( int sts, const char *mess1, const char *mess2 )
{
    if( ! FATAL_ERROR_CONDITION( sts ) ) return sts;
    return default_error_handler( sts, mess1, mess2 );
}

int handle_error( int sts, const char *mess1, const char *mess2 )
{
    if (! REPORTABLE_ERROR(sts) ) return sts;
    if ( WARNING_ERROR_CONDITION(sts) ) error_count++;
    if( sts >= error_level || FATAL_ERROR_CONDITION(sts) )
    {
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
            default:   
                { 
                    if( INFO_ERROR_CONDITION(sts) )
                    {
                        mess1="Notice";
                    }
                    else
                    {
                        mess1 = "Undefined error"; 
                    }
                }
            }
        if( mess2 == NULL && location[0] ) mess2 = location;
        if( user_error_handler )
        {
            (*user_error_handler)( sts, mess1, mess2 );
        }
        else
        {
            default_error_handler( sts, mess1, mess2 );
        }
    }
    if (FATAL_ERROR_CONDITION(sts)) exit(sts);
    return sts;
}

FILE *set_error_file( FILE *err )
{
    FILE *oldfile=error_file;
    error_file = err;
    return oldfile;
}

int set_error_level( int level )
{
    int oldlevel=error_level;
    error_level=level;
    return oldlevel;
}

int set_error_prefix( int prefix )
{
    int oldprefix=use_prefix;
    use_prefix=prefix;
    return oldprefix;
}

errhandler_type set_error_handler( errhandler_type errhndler )
{
    errhandler_type olderrhndler;
    olderrhndler = user_error_handler;
    user_error_handler = errhndler;
    return olderrhndler;
}

int get_error_count( void )
{
    int errc = error_count;
    // error_count = 0;
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

