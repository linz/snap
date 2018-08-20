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
#include "util/snapctype.h"

#include "coordsys/coordsys.h"
#include "network/network.h"
#include "snap/cfgprocs.h"
#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "snap/survfile.h"
#include "snapdata/obsmod.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/dateutil.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/readcfg.h"
#include "util/xprintf.h"

int stations_read = 0;

// #pragma warning (disable : 4100)

static int load_merge_coordinate_file( CFG_FILE *cfg, char *string, void *value, int len, int mergeopts, double mergedate )
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
        int adding=net ? 1 : 0;
        int n0=adding ? number_of_stations(net) : 0;
        const char *action=(! adding) ? "Reading" 
                         :  mergeopts & NW_MERGEOPT_ADDNEW ? "Reading additional" 
                         : "Updating";
        xprintf("\n%s coordinates from file %s\n",action,fname);
        if( adding && mergeopts & (NW_MERGEOPT_COORDS | NW_MERGEOPT_EXU | NW_MERGEOPT_CLASSES ))
        {
            const char *comma=", ";
            const char *sep="";
            xprintf("Updating ");
            if( mergeopts & NW_MERGEOPT_COORDS )
            {
                xprintf("coordinates");
                sep=comma;
            }
            if( mergeopts & NW_MERGEOPT_EXU )
            {
                xprintf("%sgeoid/deflections",sep);
                sep=comma;
            }
            if( mergeopts & NW_MERGEOPT_CLASSES )
            {
                xprintf("%sclassifications",sep);
                sep=comma;
            }
            xprintf("\n");
        }
        sts = read_station_file( fname, get_config_directory(cfg), format, csvdata, mergeopts, mergedate );
        if( sts == OK )
        {
            stations_read = 1;
            xprintf("    %d %sstations read\n",(int) number_of_stations(net)-n0,
                    adding ? "additional " : "" );
        }
        else
        {
            send_config_error( cfg, INVALID_DATA,
                               "Errors encountered reading coordinate file");
        }
    }

    if( sts != OK ) abort_config_file( cfg );

    /* Return NO_MORE_DATA to terminate reading config file */
    return sts == OK ? OK : ABORT_CONFIG_FILE;
}

int load_coordinate_file( CFG_FILE *cfg, char *string, void *value, int len, int mergeopts )
{
    return load_merge_coordinate_file( cfg, string, value, len, mergeopts, UNDEFINED_DATE );
}

int add_coordinate_file( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    /* Parse merge options, then call load_coordinate file */
    int mergeopts=0;
    double mergedate=UNDEFINED_DATE;
    char *opt;
    char *str;
    int sts=OK;

    if( ! stations_read )
    {
        send_config_error(cfg,INVALID_DATA,
            "Cannot use add_coordinate_file before coordinate_file is loaded");
        return ABORT_CONFIG_FILE;
    }

    str=string;
    while( str && NULL != (opt = strtok(str," ")) )
    {
        str = strtok(NULL,"");
        if( _stricmp(opt,"coordinates") == 0 )
        {
            mergeopts |= NW_MERGEOPT_COORDS;
        }
        else if( _stricmp(opt,"geoid") == 0 )
        {
            mergeopts |= NW_MERGEOPT_EXU;
        }
        else if( _stricmp(opt,"classes") == 0 )
        {
            mergeopts |= NW_MERGEOPT_CLASSES | NW_MERGEOPT_ADDCLASSES;
        }
        else if( _stricmp(opt,"existing_classes") == 0 )
        {
            mergeopts |= NW_MERGEOPT_CLASSES;
        }
        else if( _stricmp(opt,"stations") == 0 )
        {
            mergeopts |= NW_MERGEOPT_ADDNEW;
        }
        else if( _stricmp(opt,"epoch") == 0 )
        {
            if( ! str )
            {
                send_config_error( cfg,INVALID_DATA,
                        "Date missing in add_coordinate_file epoch option");
                sts=INVALID_DATA;
            }
            char *epochstr=strtok(str," ");
            str=strtok(NULL,"");
            if( ! parse_crdsys_epoch(epochstr,&mergedate) )
            {
                char errmsg[100];
                sprintf(errmsg,"Invalid date %.20s in add_coordinate_file epoch",
                        epochstr);
                send_config_error( cfg,INVALID_DATA,errmsg);
                sts=INVALID_DATA;
            }
        }
        else if( _stricmp(opt,"from") == 0 )
        {
            break;
        }
        else
        {
            char errmsg[80];
            sprintf(errmsg,"Invalid add_coordinate_file option %.20s",opt);
            send_config_error( cfg, INVALID_DATA, errmsg );
            return ABORT_CONFIG_FILE;
        }
    }
    if( ! mergeopts ) mergeopts = NW_MERGEOPT_ADDNEW;
    if( sts != OK ) return ABORT_CONFIG_FILE;
    return load_merge_coordinate_file( cfg, str, value, len, mergeopts, mergedate );
}

int set_output_coordinate_file( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char *fname;
    char *base_dir=get_config_directory(cfg);
    int nch=strlen(string);
    if( string[0] == '.' )
    {
        fname=(char *) check_malloc( strlen(root_name)+nch+1);
        strcpy(fname,root_name);
        strcat(fname,string);
    }
    else
    {
        nch += (base_dir ? strlen(base_dir) : 0 ) + 1;
        fname=(char *) check_malloc( nch );
        build_filespec( fname, nch, base_dir, string, NULL );
    }
    set_output_station_file( fname );
    check_free(fname);
    return OK;
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
    if(! filespec )  filespec=find_file( filename,DFLTSTOFFS_EXT,get_config_directory(cfg),FF_TRYALL,0);
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
    char *specs;
    int ftype;
    int fileid;
    double factor;

    fname = strtok( string, " " );

    if( !fname )
    {
        send_config_error(cfg,MISSING_DATA,"Data file name missing");
        return OK;
    }

    ftype = SNAP_FORMAT;
    factor = 1.0;

    specs = strtok(NULL,"");
    while( specs && NULL != (format = strtok(specs," ")) )
    {
        int readoptions=0;
        specs = strtok(NULL,"");
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
            recode = strtok(specs," ");
            specs = strtok(NULL,"");
        }
        else if ( _stricmp(format,"ERROR_FACTOR") == 0 )
        {
            format = strtok(specs," ");
            specs = strtok(NULL,"");
            if( !format || sscanf(format,"%lf",&factor) != 1 )
            {
                send_config_error( cfg, INVALID_DATA, "Invalid error factor for data file");
                return OK;
            }
        }
        else if ( _stricmp(format,"CSV") == 0 )
        {
            ftype = CSV_FORMAT;
            readoptions=1;
        }
        else if ( _stricmp(format,"SINEX") == 0 )
        {
            ftype = SINEX_FORMAT;
            readoptions=1;
        }
        else
        {
            sprintf(errmess,"Invalid format %.20s specified for data file",format);
            send_config_error( cfg, INVALID_DATA,errmess);
            return OK;
        }

        if( readoptions && specs )
        {
            char *endopts;
            char *nextfield;
            int inopts;
            /* Options are following fields containing '=' */

            while( ISSPACE(*specs) ) specs++;
            options=specs;

            inopts=0;
            nextfield=options;
            endopts=0;
            for( char *c=specs; *c; c++ )
            {
                if( ! ISSPACE(*c))
                {
                    if( *c == '=' )
                    {
                        inopts=1;
                        nextfield=0;
                        endopts=0;
                    }
                    else if( ! inopts )
                    {
                        if(! nextfield ) nextfield=c;
                    }
                }
                else if( ! inopts )
                {
                    break;
                }
                else
                {
                    inopts=0;
                    if( ! endopts ) endopts=c;
                }
            }
            if( nextfield == options ) options=0;
            if( endopts ) *endopts=0;
            specs=nextfield;
        }
    }

    fileid=add_data_file( fname, ftype, options, recode, get_config_directory(cfg) );

    if( factor != 1.0 )
    {
        void *obs_modifications=snap_obs_modifications( true );
        add_obs_modifications_datafile_factor(cfg,obs_modifications,fileid,survey_data_file_name(fileid),factor);
    }

    return OK;
}


int read_obs_modification_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    int sts=OK;
    double err_factor=1.0;
    void *obs_modifications;
    if( code == OBS_MOD_REWEIGHT )
    {
        char *field=next_field(&string);
        if( _stricmp(field,"by_set") == 0 )
        {
            code=OBS_MOD_REWEIGHT_SET;
            field=next_field(&string);
        }
        if( _stricmp(field,"by") == 0 )
        {
            field=next_field(&string);
        }
        if( ! field || sscanf(field,"%lf", &err_factor) != 1 || err_factor <= 0 )
        {
            send_config_error(cfg, INVALID_DATA, "Invalid or missing data in reweight observations command");
            return OK;
        }
    }
    obs_modifications=snap_obs_modifications( true );
    sts=add_obs_modifications( cfg, obs_modifications, string, code, err_factor );
    return OK;
}


int read_classification_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    double errfct;
    char *classification;
    char *classvalue;
    int isdatafile;
    int action;
    int missing_error;
    char *st;
    void *obs_modifications;

    missing_error=INVALID_DATA;
    st = strtok( string, " " );
    if( !st )
    {
        send_config_error( cfg, MISSING_DATA, "Name of the classification is missing");
        return OK;
    }

    classification=st;
    isdatafile=_stricmp(classification,"data_file")==0;

    errfct = -1.0;
    action=0;

    st = strtok( NULL, " " );
    if( st )
    {
        if( _stricmp( st, "reject" ) == 0 )
        {
            action |= OBS_MOD_REJECT;
        }
        else if( _stricmp( st, "ignore" ) == 0 )
        {
            action |= OBS_MOD_IGNORE;
        }
        if( action ) st = strtok( NULL, " " );
    }

    if( isdatafile && st && _stricmp(st,"ignore_missing") == 0 ) 
    {
        missing_error=OK;
        st = strtok( NULL, " " );
    }
    else if( isdatafile && st && _stricmp(st,"warn_missing") == 0 ) 
    {
        missing_error=INFO_ERROR;
        st = strtok( NULL, " " );
    }
    else if( isdatafile && st && _stricmp(st,"fail_missing") == 0 ) 
    {
        st = strtok( NULL, " " );
    }

    if( !st )
    {
        send_config_error( cfg, MISSING_DATA, "Value of classification is missing");
        return OK;
    }

    classvalue=st;

    st = strtok( NULL, " " );

    if( ! action )
    {
        if( !st )
        {
            send_config_error( cfg, MISSING_DATA,"Classification commands needs ignore, reject, or error_factor specified");
            return OK;
        }

        if( _stricmp( st, "reject" ) == 0 )
        {
            action |= OBS_MOD_REJECT;
        }
        else if( _stricmp( st, "ignore" ) == 0 )
        {
            action |= OBS_MOD_IGNORE;
        }
        if( action ) st = strtok( NULL, " " );
    }

    if( st )
    {
        if( _stricmp(st,"error_factor") != 0 || (st=strtok(NULL," ")) == NULL ||
                sscanf(st, "%lf", &errfct ) != 1 || errfct <= 0.0 ||
                strtok(NULL," ") != NULL )
        {

            send_config_error(cfg, INVALID_DATA, "Invalid or missing data in classification command");
            return OK;
        }
        if( errfct != 1.0 ) action |= OBS_MOD_REWEIGHT;
    }

    if( ! action ) return OK;

    obs_modifications=snap_obs_modifications( true );

    add_obs_modifications_classification(cfg,obs_modifications,classification,classvalue,
            action,errfct,missing_error);

    return OK;
}

int read_recode_command( CFG_FILE *cfg, char *string, void *value, int len, int code )
{

    if( ! stations_read )
    {
        send_config_error(cfg,INVALID_DATA,
                          "Stations cannot be recoded before the station file is loaded");
        return OK;
    }
    if( ! stnrecode ) stnrecode=create_stn_recode_map( net );
    if( read_station_recode_definition( stnrecode, string, cfg->name ) != OK )
    {
        send_config_error(cfg,INVALID_DATA,"Errors encountered in recode command" );
    }
    return OK;
}

