#include "snapconfig.h"
/*
   $Log: cfgprocs.c,v $
   Revision 1.2  2004/04/22 02:34:47  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 17:39:58  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "snap/snapglob.h"
#include "util/readcfg.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "snap/stnadj.h"
#include "snap/survfile.h"
#include "snap/cfgprocs.h"
#include "util/xprintf.h"

int stations_read = 0;

// #pragma warning (disable : 4100)

int load_coordinate_file( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    int sts;
    char *fname;
    char *typestr;
    char *csvdata;
    int format;

    sts = OK;
    format = STN_FORMAT_SNAP;
    csvdata = 0;

    fname = strtok( string, " ");
    typestr = strtok( NULL, " ");

    if( !fname )
    {
        send_config_error( cfg, MISSING_DATA,
                           "The name of the coordinate file is missing");
        sts = MISSING_DATA;
    }

    if( sts == OK && typestr )
    {
        if( _stricmp(typestr,"SNAP") == 0 )
        {
            format = STN_FORMAT_SNAP;
        }
        else if( _stricmp(typestr,"GB") == 0 )
        {
            format = STN_FORMAT_GB;
        }
        else if( _stricmp(typestr,"CSV") == 0 )
        {
            format = STN_FORMAT_CSV;
            csvdata = strtok( NULL, "" );
        }
        else
        {
            send_config_error( cfg, INVALID_DATA,
                               "The format specified for the coordinate file is invalid");
            sts = INVALID_DATA;
        }
    }

    if( sts == OK )
    {
        xprintf("\nReading coordinates from file %s\n",fname);
        sts = read_station_file( fname, cmd_dir, format, csvdata );
        if( sts == OK )
        {
            stations_read = 1;
            xprintf("    %d stations read\n",(int) number_of_stations(net));
        }
        else
        {
            send_config_error( cfg, INVALID_DATA,
                               "Errors encountered reading coordinate file");
        }
    }

    return sts == OK ? OK : NO_MORE_DATA;
}

int load_offset_file( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    const char *filename;
    const char *filespec;
    int sts;

    filename=strtok(string," ");
    if( ! filename )
    {
        send_config_error( cfg, INVALID_DATA, "station_offset_file command requires a filename");
        return OK;
    }

    filespec = find_relative_file( station_filespec, filename,DFLTSTOFFS_EXT);
    if(! filespec )  filespec=find_file( filename,DFLTSTOFFS_EXT,cmd_dir,FF_TRYALL,0);
    if(! filespec )
    {
        send_config_error( cfg, INVALID_DATA, "Cannot find station offset file");
        return OK;
    }
    sts=read_network_station_offsets( net, filespec );
    if( sts != OK )
    {
        send_config_error( cfg, INVALID_DATA, "Errors reading station offset file");
    }
    return OK;
}


int load_data_file( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *fname, *format, errmess[80];
    char *options = 0;
    char *recode = 0;
    int ftype;
    double factor;

    fname = strtok( string, " " );

    if( !fname )
    {
        send_config_error(cfg,MISSING_DATA,"Data file name missing");
        return OK;
    }

    ftype = SNAP_FORMAT;
    factor = 1.0;

    while( NULL != (format = strtok(NULL," ")) )
    {
        if( _stricmp(format,"SNAP") == 0 )
        {
            ftype = SNAP_FORMAT;
        }
        else if ( _stricmp(format,"GB") == 0 )
        {
            ftype = GB_FORMAT;
        }
        else if ( _stricmp(format,"RECODE") == 0 )
        {
            recode = strtok(NULL," ");
        }
        else if ( _stricmp(format,"ERROR_FACTOR") == 0 )
        {
            format = strtok(NULL," ");
            if( !format || sscanf(format,"%lf",&factor) != 1 )
            {
                send_config_error( cfg, INVALID_DATA, "Invalid error factor for data file");
                return OK;
            }
        }
        else if ( _stricmp(format,"CSV") == 0 )
        {
            ftype = CSV_FORMAT;
            options = strtok(NULL,"");
            break;
        }
        else
        {
            sprintf(errmess,"Invalid format %.20s specified for data file",format);
            send_config_error( cfg, INVALID_DATA,errmess);
            return OK;
        }
    }

    add_data_file( fname, ftype, options, factor, recode );

    return OK;
}


