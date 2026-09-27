#include "snapconfig.h"
/* Routines to read a configuration file */
/* The syntax of the file is assumed to be of the form

	option = value

	where option is a string containing no spaces.  value is the rest
	of the string up to a comment character, or the end of the line.

	The main routine to be called is read_config_file.  It is supplied
	with a file handle of the configuration file, and with an array
	of config_item, the structure of which is defined in the header file
	associated with this routine.  Each item may have the following
	components..

	1) The address of the option name.  The name should be
		entirely in lower case.
	2) The address into which the value is to be stored
	3) The address offset.  If this has value CFG_ABSOLUTE (-1) then
		the address is used directly.  Otherwise it is taken
		as a pointer to an address from which an offset is
		calculated (mainly used for putting values in structures).
	4) A length parameter used by the routine which stores the data.
	5) Optionally the address of a routine used to store the value
		This routine should take three arguments, these being the
		address of the string defined in the configuration file,
		the address of the value, and the length of the value.  It
		should return a value of 0 if the value is valid, otherwise
		it should return a non-zero value.  If the address is NULL, then
		the address of the value is used as the location into which to
		store the value as a string.
	6) A flag used to by an optional user supplied function to
		process commands.
	7) An integer code for the users use - not processed by the readcfg
		routines.

	The array should be terminated by a NULL pointer for the address of
	the option name

	*/

/*
   $Log: readcfg.c,v $
   Revision 1.2  2004/04/22 02:35:27  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:55:25  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <optional>
#include <string>
#include "util/snapctype.h"

#include "util/chkalloc.h"
#include "util/readcfg.h"
#include "util/readcfg_internal.h"
#include "util/fileutil.h"

CFG_FILE::CFG_FILE( const std::string &name, const char comment_char ) :
    name(name), dirname(std::filesystem::path(name).parent_path().string()),
    lineno(0), errcount(0), read_options(CFG_INIT_ITEMS|CFG_CHECK_MISSING),
    command_flag(0), ignore_flag(0), comment_char(comment_char), abort(0)
{
    f.open( name );
    if( ! f.is_open() )
    {
        handle_error(FILE_OPEN_ERROR,"Cannot open configuration file",name.c_str());
        return;
    }
    if( ! skip_utf8_bom(f) )
    {
        f.close();
        handle_error(FILE_OPEN_ERROR,"Cannot handle UTF16 file",name.c_str());
    }
}

CFG_FILE *open_config_file( const std::string &name, const char comment_char )
{
    CFG_FILE *cfg = new CFG_FILE( name, comment_char );
    if( ! cfg->f.is_open() )
    {
        delete cfg;
        return nullptr;
    }
    return cfg;
}

void close_config_file( CFG_FILE *cfg )
{
    delete cfg;
}


int set_config_read_options( CFG_FILE *cfg, const int options )
{
    const int old_options = cfg->read_options;
    cfg->read_options = options;
    return old_options;
}

int set_config_command_flag( CFG_FILE *cfg, const int flag )
{
    const int old_flag = cfg->command_flag;
    cfg->command_flag = flag;
    return old_flag;
}

int set_config_ignore_flag( CFG_FILE *cfg, const int flag )
{
    const int old_flag = cfg->ignore_flag;
    cfg->ignore_flag = flag;
    return old_flag;
}

bool CFG_FILE::get_config_line( ConfigLine &line, const std::size_t max_len )
{
    if( ! std::getline( f, line.content ) ) return false;

    const CommentRule comment{
        (read_options & CFG_IGNORE_COMMENT) ? std::nullopt : std::optional<char>(comment_char),
        (read_options & CFG_POSITIONAL_COMMENT) != 0
    };
    filter_line( line.content, comment );
    line.overrun = cap_line( line.content, max_len );
    lineno++;
    return true;
}

std::string get_config_location( CFG_FILE *cfg )
{
    return "Line " + std::to_string(cfg->lineno) + ": File " + cfg->name.substr(0,CFG_FILE_NAME_LEN);
}

std::string get_config_filename( CFG_FILE *cfg )
{
    return cfg->name;
}

std::string get_config_directory( CFG_FILE *cfg )
{
    return cfg->dirname;
}

int send_config_error( CFG_FILE *cfg, const int stat, const std::string &mess1 )
{
    const std::string mess2 = get_config_location(cfg);
    handle_error(stat,mess1.c_str(),mess2.c_str());
    if( WARNING_ERROR_CONDITION(stat)) cfg->errcount++;
    return stat;
}


void initialise_config_items( config_item item[] )
{
    config_item *it;
    for( it=item; it->option; it++ ) it->flags &= ~CFG_PRESENT;
}

int report_missing_config_items( CFG_FILE *cfg, config_item item[] )
{
    char errmess[256];
    config_item *it;
    int errcount;

    /* Check that all required parameters were present */

    errcount = 0;
    for( it = item; it->option; it++ )
    {
        if( it->flags & CFG_REQUIRED && !(it->flags & CFG_PRESENT) )
        {
            sprintf(errmess,"Definition of %.32s is missing",it->option);
            send_config_error(cfg,MISSING_DATA,errmess);
            errcount++;
        }
    }
    return errcount;
}


int read_config_file( CFG_FILE *cfg, config_item item[] )
{

    char errmess[256];
    char *opt, *val, *storestr, *address;
    int end;
    config_item *it;
    int errstat;
    char blank[2]={0,0};
    ConfigLine line;

    /* Get the initial error count */

    const int initcount = cfg->errcount;

    if( cfg->read_options & CFG_SET_PATH )
    {
        push_file_context( get_config_directory(cfg) );
    }

    /* Do we want to initialise */

    if( cfg->read_options & CFG_INIT_ITEMS )
    {
        initialise_config_items( item );
    }

    while( !cfg->abort && cfg->get_config_line(line, CFG_MAX_LINE_LENGTH) )
    {

        /* If blank line or comment then skip */

        if( NULL == (opt = strtok(line.content.data(),FIELD_DELIMS))) continue;

        /* Is the record too long?  If so then send error and skip */

        if( line.overrun )
        {
            if( !(cfg->read_options & CFG_IGNORE_BAD) )
            {
                sprintf(errmess,"Line %d characters too long in configuration file",line.overrun);
                send_config_error(cfg,INVALID_DATA,errmess);
            }
            continue;
        }

        /* Is it a valid option record - if not print warning and
            continue */

        _strlwr(opt);
        for ( it = item; it->option; it++ )
        {
            if( ( !cfg->command_flag ) | (cfg->command_flag & it->flags) )
            {
                if( strcmp( it->option, opt ) == 0 ) break;
            }
        }

        /* Is this a valid command - it is not the option is NULL, ie
           no match found above, or the command_flag is defined as non
        zero, and is not matched by this command */


        if( ! it->option )
        {
            if( !(cfg->read_options & CFG_IGNORE_BAD) )
            {
                sprintf(errmess,"Invalid item %.32s in configuration file",opt);
                send_config_error(cfg,INVALID_DATA,errmess);
            }
            continue;
        }

        /* Is this option to be ignored */

        if( it->flags & cfg->ignore_flag ) continue;

        /* Is this option illegally duplicated */

        if( it->flags & CFG_ONEONLY && it->flags & CFG_PRESENT )
        {
            sprintf(errmess,"Definition of %.32s is duplicated",opt);
            send_config_error(cfg,INVALID_DATA,errmess);
            continue;
        }

        it->flags |= CFG_PRESENT;

        /* Does this item mark the end of a configuration section */

        if( it->flags & CFG_END ) break;

        /* Is there a value defined ... */

        if( (val = strtok(NULL,"\n")) )
        {
            /* Delete leading field delimiters, then trailing blanks
               and tab characters */

            while( *val && strchr( FIELD_DELIMS, *val )) val++;
            end = strlen(val)-1;
            while( end >= 0 && (val[end]==' ' || val[end]=='\t') )
            {
                val[end] = '\0';
                end--;
            }
        }
        else
        {
            blank[0]=0;
            val=blank;
        }

        /* Get the address in which the value is to be stored */

        if( it->offset == CFG_ABSOLUTE )
        {
            address = (char *) (it->value);
        }
        else
        {
            address = * (char **) (it->value) + it->offset;
        }

        /* Offer the command to the user supplied processor */

        if( it->store == STORE_AS_STRING )
        {
            storestr = address;
            strncpy( storestr, val, it->vallen-1 );
            storestr[it->vallen-1] = '\0';
        }
        else
        {
            errstat = (*it->store)( cfg, val, (void *) address, it->vallen, it->code );
            if( errstat == ABORT_CONFIG_FILE ) break;
            if( errstat != OK )
            {
                sprintf(errmess,"Invalid value %.32s defined for %.32s in configuration file",
                        val,opt);
                send_config_error(cfg,errstat,errmess);
                continue;
            }
        }

        /* And now go on with the next line */
    }

    if( !cfg->abort && (cfg->read_options & CFG_CHECK_MISSING) )
    {
        report_missing_config_items( cfg, item );
    }

    if( cfg->read_options & CFG_SET_PATH )
    {
        pop_file_context();
    }

    return cfg->errcount - initcount;
}

void abort_config_file( CFG_FILE *cfg )
{
    if( cfg ) cfg->abort = 1;
}

int readcfg_int( CFG_FILE *, char *str, void *value, int, int )
{

    int val;
    char check[2];
    check[0] = 0;
    if( sscanf(str,"%d%1s",&val,check) >= 1 && check[0] == 0)
    {
        * (int *) value = val;
        return 0;
    }
    else
    {
        return 1;
    }
}


int readcfg_short( CFG_FILE *, char *str, void *value, int, int )
{

    short ival;
    char check[2];
    check[0] = 0;
    if( sscanf(str,"%hd%1s",&ival,check) >= 1 && check[0] == 0)
    {
        * (short *) value = ival;
        return 0;
    }
    else
    {
        return 1;
    }
}


int readcfg_long( CFG_FILE *, char *str, void *value, int, int )
{

    long val;
    char check[2];
    check[0] = 0;
    if( sscanf(str,"%ld%1s",&val,check) >= 1 && check[0] == 0)
    {
        * (long *) value = val;
        return 0;
    }
    else
    {
        return 1;
    }
}



int readcfg_float( CFG_FILE *, char *str, void *value, int, int )
{
    float val;
    char check[2];
    check[0] = 0;
    if( sscanf(str,"%f%1s",&val,check) >= 1 && check[0] == 0)
    {
        * (float *) value = val;
        return OK;
    }
    else
    {
        return INVALID_DATA;
    }
}



int readcfg_double( CFG_FILE *, char *str, void *value, int, int )
{
    double val;
    char check[2];
    check[0] = 0;
    if( sscanf(str,"%lf%1s",&val,check) >= 1 && check[0] == 0 )
    {
        * (double *) value = val;
        return OK;
    }
    else
    {
        return INVALID_DATA;
    }
}

int readcfg_boolean( CFG_FILE *, char *str, void *value, int length, int )
{
    unsigned char flag;
    flag = length;
    if( flag == 0 ) flag=1;
    _strlwr(str);
    if( strcmp(str,"y")==0 || strcmp(str,"yes") ==0 ||
            strcmp(str,"t")==0 || strcmp(str,"true") ==0 ||
            strcmp(str,"on")==0 )
    {
        * (unsigned char *) value |= flag;
        return OK;
    }
    else if( strcmp(str,"n")==0 || strcmp(str,"no") ==0 ||
             strcmp(str,"f")==0 || strcmp(str,"false") ==0 ||
             strcmp(str,"off")==0 )
    {
        * (unsigned char *) value &= ~flag;
        return OK;
    }
    else
        return INVALID_DATA;
}
