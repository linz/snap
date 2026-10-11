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
#include <stdlib.h>
#include <string.h>

#include <string>

#include <boost/numeric/conversion/cast.hpp>

#include "util/errdef.h" /* Error code definitions */

static errhandler_type user_error_handler = (errhandler_type)0;
static FILE *error_file = NULL;
static int error_level = 0;
static int use_prefix = 1;
static int error_count = 0;

static std::string location;

int default_error_handler(const int sts, const std::string_view mess1, const error_message mess2)
{
    FILE *out = error_file ? error_file : stderr;
    if (use_prefix)
    {
        if (FATAL_ERROR_CONDITION(sts))
        {
            fprintf(out, "\nFatal error: ");
        }
        else if (WARNING_ERROR_CONDITION(sts))
        {
            fprintf(out, "\nWarning: ");
        }
        else
        {
            fprintf(out, "\nInformation: ");
        }
    }
    fprintf(out, "%.*s\n", boost::numeric_cast<int>(mess1.size()), mess1.data());
    if (mess2) fprintf(out, "%.*s\n", boost::numeric_cast<int>(mess2->size()), mess2->data());
    return sts;
}

int null_error_handler(const int sts, const std::string_view mess1, const error_message mess2)
{
    if (!FATAL_ERROR_CONDITION(sts)) return sts;
    return default_error_handler(sts, mess1, mess2);
}

static std::string_view default_message(const int sts)
{
    switch (sts)
    {
    case FILE_OPEN_ERROR:
        return "Error opening file";
    case FILE_READ_ERROR:
        return "Error reading file";
    case FILE_WRITE_ERROR:
        return "Error writing file";
    case UNEXPECTED_EOF:
        return "End of file encountered";
    case SYNTAX_ERROR:
        return "Syntax error";
    case INVALID_DATA:
        return "Invalid data error";
    case MISSING_DATA:
        return "Missing data";
    case INCONSISTENT_DATA:
        return "Inconsistent data";
    case TOO_MUCH_DATA:
        return "Too much data";
    case MEM_ALLOC_ERROR:
        return "Memory allocation error";
    case INTERNAL_ERROR:
        return "Internal program error";
    case OPERATION_ABORTED:
        return "Aborted by user";
    default:
        return INFO_ERROR_CONDITION(sts) ? "Notice" : "Undefined error";
    }
}

int handle_error(const int sts, const error_message mess1, error_message mess2)
{
    if (!REPORTABLE_ERROR(sts)) return sts;
    if (WARNING_ERROR_CONDITION(sts)) error_count++;
    if (sts >= error_level || FATAL_ERROR_CONDITION(sts))
    {
        const std::string_view text = mess1 ? *mess1 : default_message(sts);
        if (!mess2 && !location.empty()) mess2 = location;
        if (user_error_handler)
        {
            (*user_error_handler)(sts, text, mess2);
        }
        else
        {
            default_error_handler(sts, text, mess2);
        }
    }
    if (FATAL_ERROR_CONDITION(sts)) exit(sts);
    return sts;
}

FILE *set_error_file(FILE *err)
{
    FILE *oldfile = error_file;
    error_file = err;
    return oldfile;
}

int set_error_level(int level)
{
    int oldlevel = error_level;
    error_level = level;
    return oldlevel;
}

int set_error_prefix(int prefix)
{
    int oldprefix = use_prefix;
    use_prefix = prefix;
    return oldprefix;
}

errhandler_type set_error_handler(errhandler_type errhndler)
{
    errhandler_type olderrhndler;
    olderrhndler = user_error_handler;
    user_error_handler = errhndler;
    return olderrhndler;
}

int get_error_count(void)
{
    int errc = error_count;
    // error_count = 0;
    return errc;
}

void set_error_location(const error_message loc)
{
    location = loc ? std::string(*loc) : std::string();
}
