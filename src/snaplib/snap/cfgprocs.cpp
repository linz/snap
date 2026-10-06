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
#include <boost/algorithm/string/predicate.hpp>
#include <charconv>
#include <string_view>
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
#include "util/fieldscanner.hpp"
#include "util/iostring.h"
#include "util/dateutil.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/readcfg.h"
#include "util/xprintf.h"

int stations_read = 0;

// #pragma warning (disable : 4100)

static int load_merge_coordinate_file( CFG_FILE *cfg, std::string_view string, void *, int, int mergeopts, double mergedate )
{
    int sts;
    int format;

    sts = OK;
    format = STN_FORMAT_SNAP;
    std::optional<std::string_view> csvdata;

    FieldScanner scanner(string);
    auto fname = scanner.checkAndRecoverQuotedValue( true,
        std::vector<QuoteFollowOption>{QuoteFollowOption::Whitespace,QuoteFollowOption::End} );
    auto typestr = scanner.next();

    if( !fname )
    {
        send_config_error( cfg, MISSING_DATA,
                           "The name of the coordinate file is missing");
        sts = MISSING_DATA;
    }

    if( sts == OK && typestr )
    {
        if( boost::algorithm::iequals(*typestr,"SNAP") )
        {
            format = STN_FORMAT_SNAP;
        }
        else if( boost::algorithm::iequals(*typestr,"GB") )
        {
            format = STN_FORMAT_GB;
        }
        else if( boost::algorithm::iequals(*typestr,"CSV") )
        {
            format = STN_FORMAT_CSV;
            csvdata = scanner.remainder();
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
        const std::string fnameStr(*fname);
        int adding=net ? 1 : 0;
        int n0=adding ? number_of_stations(net) : 0;
        const std::string action=(! adding) ? "Reading"
                         :  mergeopts & NW_MERGEOPT_ADDNEW ? "Reading additional"
                         : "Updating";
        xprintf("\n%s coordinates from file %s\n",action.c_str(),fnameStr.c_str());
        if( adding && mergeopts & (NW_MERGEOPT_COORDS | NW_MERGEOPT_EXU | NW_MERGEOPT_CLASSES ))
        {
            const std::string comma=", ";
            std::string sep="";
            xprintf("Updating ");
            if( mergeopts & NW_MERGEOPT_COORDS )
            {
                xprintf("coordinates");
                sep=comma;
            }
            if( mergeopts & NW_MERGEOPT_EXU )
            {
                xprintf("%sgeoid/deflections",sep.c_str());
                sep=comma;
            }
            if( mergeopts & NW_MERGEOPT_CLASSES )
            {
                xprintf("%sclassifications",sep.c_str());
                sep=comma;
            }
            xprintf("\n");
        }
        const std::string csvdataStr( csvdata.value_or(std::string_view()) );
        sts = read_station_file( fnameStr, get_config_directory(cfg), format,
                                  csvdataStr, mergeopts, mergedate );
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

int load_coordinate_file( CFG_FILE *cfg, std::string_view string, void *value, int len, int mergeopts )
{
    return load_merge_coordinate_file( cfg, string, value, len, mergeopts, UNDEFINED_DATE );
}

int add_coordinate_file( CFG_FILE *cfg, std::string_view string, void *value, int len, int )
{
    /* Parse merge options, then call load_coordinate file */
    int mergeopts=0;
    double mergedate=UNDEFINED_DATE;
    int sts=OK;

    if( ! stations_read )
    {
        send_config_error(cfg,INVALID_DATA,
            "Cannot use add_coordinate_file before coordinate_file is loaded");
        return ABORT_CONFIG_FILE;
    }

    FieldScanner scanner(string);
    for( auto opt = scanner.next(); opt; opt = scanner.next() )
    {
        if( boost::algorithm::iequals(*opt,"coordinates") )
        {
            mergeopts |= NW_MERGEOPT_COORDS;
        }
        else if( boost::algorithm::iequals(*opt,"geoid") )
        {
            mergeopts |= NW_MERGEOPT_EXU;
        }
        else if( boost::algorithm::iequals(*opt,"classes") )
        {
            mergeopts |= NW_MERGEOPT_CLASSES | NW_MERGEOPT_ADDCLASSES;
        }
        else if( boost::algorithm::iequals(*opt,"existing_classes") )
        {
            mergeopts |= NW_MERGEOPT_CLASSES;
        }
        else if( boost::algorithm::iequals(*opt,"stations") )
        {
            mergeopts |= NW_MERGEOPT_ADDNEW;
        }
        else if( boost::algorithm::iequals(*opt,"epoch") )
        {
            auto epochstr = scanner.next();
            if( ! epochstr )
            {
                send_config_error( cfg,INVALID_DATA,
                        "Date missing in add_coordinate_file epoch option");
                sts=INVALID_DATA;
            }
            // The original fell through to parse_crdsys_epoch(NULL,...) here
            // (a latent null-deref, unrelated to this conversion) - skip the
            // call instead now that "missing" is representable directly.
            else if( ! parse_crdsys_epoch( *epochstr, mergedate ) )
            {
                char errmsg[100];
                sprintf(errmsg,"Invalid date %.20s in add_coordinate_file epoch",
                        std::string(*epochstr).c_str());
                send_config_error( cfg,INVALID_DATA,errmsg);
                sts=INVALID_DATA;
            }
        }
        else if( boost::algorithm::iequals(*opt,"from") )
        {
            break;
        }
        else
        {
            char errmsg[80];
            sprintf(errmsg,"Invalid add_coordinate_file option %.20s",std::string(*opt).c_str());
            send_config_error( cfg, INVALID_DATA, errmsg );
            return ABORT_CONFIG_FILE;
        }
    }
    if( ! mergeopts ) mergeopts = NW_MERGEOPT_ADDNEW;
    if( sts != OK ) return ABORT_CONFIG_FILE;
    return load_merge_coordinate_file( cfg, scanner.remainder(), value, len, mergeopts, mergedate );
}

int set_output_coordinate_file( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    std::string fname;
    if( !string.empty() && string[0] == '.' )
    {
        fname = command_file->root + std::string(string);
    }
    else
    {
        fname = build_filespec( get_config_directory(cfg), std::string(string), "" );
    }
    set_output_station_file( fname );
    return OK;
}

int load_offset_file( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    std::optional<std::string> filespec;
    int sts;

    FieldScanner scanner(string);
    auto filename = scanner.checkAndRecoverQuotedValue( true,
        std::vector<QuoteFollowOption>{QuoteFollowOption::Whitespace,QuoteFollowOption::End} );
    if( ! filename )
    {
        send_config_error( cfg, INVALID_DATA, "station_offset_file command requires a filename");
        return OK;
    }
    const std::string filenameStr(*filename);

    if( station_file ) filespec = find_relative_file( station_file->filespec, filenameStr, DFLTSTOFFS_EXT );
    if( ! filespec ) filespec = find_file( filenameStr, DFLTSTOFFS_EXT, get_config_directory(cfg), FF_TRYALL, "" );
    if(! filespec )
    {
        send_config_error( cfg, INVALID_DATA, "Cannot find station offset file");
        return OK;
    }
    sts=read_network_station_offsets( net, *filespec );
    if( sts != OK )
    {
        send_config_error( cfg, INVALID_DATA, "Errors reading station offset file");
    }
    return OK;
}


int load_data_file( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    char errmess[80];
    std::optional<std::string> options;
    std::optional<std::string> recode;
    int ftype;
    int fileid;
    double factor;

    FieldScanner scanner(string);
    auto fnameField = scanner.checkAndRecoverQuotedValue( true,
        std::vector<QuoteFollowOption>{QuoteFollowOption::Whitespace,QuoteFollowOption::End} );

    if( !fnameField )
    {
        send_config_error(cfg,MISSING_DATA,"Data file name missing");
        return OK;
    }
    const std::string fname(*fnameField);

    ftype = SNAP_FORMAT;
    factor = 1.0;

    for( auto formatField = scanner.next(); formatField; formatField = scanner.next() )
    {
        int readoptions=0;
        if( boost::algorithm::iequals(*formatField,"SNAP") )
        {
            ftype = SNAP_FORMAT;
        }
        else if ( boost::algorithm::iequals(*formatField,"GB") )
        {
            ftype = GB_FORMAT;
        }
        else if ( boost::algorithm::iequals(*formatField,"RECODE") )
        {
            auto recodefield = scanner.next();
            recode = recodefield ? std::optional<std::string>(std::string(*recodefield)) : std::nullopt;
        }
        else if ( boost::algorithm::iequals(*formatField,"ERROR_FACTOR") )
        {
            // Matches the old sscanf(format,"%lf",&factor)!=1 check - only a
            // valid leading number is required, no full-consumption check.
            auto factorField = scanner.next();
            auto value = factorField ? parse_leading<double>(*factorField) : std::nullopt;
            if( !value )
            {
                send_config_error( cfg, INVALID_DATA, "Invalid error factor for data file");
                return OK;
            }
            factor = *value;
        }
        else if ( boost::algorithm::iequals(*formatField,"CSV") )
        {
            ftype = CSV_FORMAT;
            readoptions=1;
        }
        else if ( boost::algorithm::iequals(*formatField,"SINEX") )
        {
            ftype = SINEX_FORMAT;
            readoptions=1;
        }
        else
        {
            sprintf(errmess,"Invalid format %.20s specified for data file",std::string(*formatField).c_str());
            send_config_error( cfg, INVALID_DATA,errmess);
            return OK;
        }

        if( readoptions )
        {
            /* Options are following fields containing '=' - the first
               field without one ends the options block and is left for
               the next iteration of this loop to reprocess as a format
               keyword. beforeOptions/afterOptions are both remainder()
               checkpoints into the same underlying text, so the verbatim
               options span between them is plain size arithmetic - no
               raw pointers needed. */
            const std::string_view beforeOptions = scanner.remainder();
            std::string_view afterOptions;
            while( true )
            {
                const std::string_view beforeField = scanner.remainder();
                auto field = scanner.next();
                if( !field ) { afterOptions = std::string_view(); break; }
                if( field->find('=') == std::string_view::npos )
                {
                    scanner = FieldScanner(beforeField);
                    afterOptions = beforeField;
                    break;
                }
                afterOptions = scanner.remainder();
            }
            const std::string_view optionsSpan =
                beforeOptions.substr( 0, beforeOptions.size() - afterOptions.size() );
            options = optionsSpan.empty() ? std::nullopt : std::optional<std::string>(std::string(optionsSpan));
        }
    }

    fileid=add_data_file( fname, ftype, options, recode, current_file_context() );

    if( factor != 1.0 )
    {
        void *obs_modifications=snap_obs_modifications( true );
        add_obs_modifications_datafile_factor(cfg,obs_modifications,fileid,survey_data_file_name(fileid),factor);
    }

    return OK;
}


int read_obs_modification_command( CFG_FILE *cfg, std::string_view string, void *, int, int code )
{
    double errval1=0.0;
    double errval2=0.0;
    void *obs_modifications;
    if( code == OBS_MOD_REWEIGHT )
    {
        int ok;
        input_string_def is(string);
        if( test_next_string_field(is.scanner,"offset_error") ) code=OBS_MOD_OFFSET_ERROR;
        else if( test_next_string_field(is.scanner,"centroid_error") ) code=OBS_MOD_CENTROID_ERROR;
        if( code != OBS_MOD_REWEIGHT )
        {
            ok=double_from_string(is.scanner,&errval1);
            if( ok != OK )
            {
                if( errval1 < 0.0 ) ok=INVALID_DATA;
            }
            auto saved=is.scanner.remainder();
            if( ok == OK && double_from_string(is.scanner,&errval2) == OK )
            {
                if( errval2 < 0.0 ) ok=INVALID_DATA;
            }
            else
            {
                errval2=errval1;
                is.scanner=FieldScanner(saved);
            }
            if( ok == OK )
            {
                if( test_next_string_field(is.scanner,"mm") )
                {
                    errval1 /= 1000.0;
                    errval2 /= 1000.0;
                }
                else if( ! test_next_string_field(is.scanner,"m") )
                {
                    ok=INVALID_DATA;
                }
            }
        }
        else
        {
            if( test_next_string_field(is.scanner,"by_set") )
            {
                code=OBS_MOD_REWEIGHT_SET;
            }
            test_next_string_field(is.scanner,"by");
            ok=double_from_string(is.scanner,&errval1);
            if( errval1 <= 0.0 ) ok=INVALID_DATA;
        }
        if( ok != OK )
        {
            send_config_error(cfg, INVALID_DATA, "Invalid or missing data in reweight_observations command");
            return OK;
        }
        string=unread_string(is);
    }
    else if( code == OBS_MOD_ANTENNA_OFFSET )
    {
        input_string_def is(string);
        if( double_from_string(is.scanner,&errval1) != OK || ! test_next_string_field(is.scanner,"m"))
        {
            send_config_error(cfg, INVALID_DATA, "Invalid or missing data in gps_antenna_height command");
            return OK;
        }
        string=unread_string(is);
    }
    obs_modifications=snap_obs_modifications( true );
    add_obs_modifications( cfg, obs_modifications, string, code, errval1, errval2 );
    return OK;
}


int read_classification_command( CFG_FILE *cfg, std::string_view string, void *, int, int )
{
    double errfct;
    int isdatafile;
    int action;
    int missing_error;
    void *obs_modifications;

    missing_error=INVALID_DATA;
    FieldScanner scanner(string);
    auto st = scanner.next();
    if( !st )
    {
        send_config_error( cfg, MISSING_DATA, "Name of the classification is missing");
        return OK;
    }

    const std::string_view classification = *st;
    isdatafile=boost::algorithm::iequals(classification,"data_file");

    errfct = -1.0;
    action=0;

    st = scanner.next();
    if( st )
    {
        if( boost::algorithm::iequals( *st, "reject" ) )
        {
            action |= OBS_MOD_REJECT;
        }
        else if( boost::algorithm::iequals( *st, "ignore" ) )
        {
            action |= OBS_MOD_IGNORE;
        }
        if( action ) st = scanner.next();
    }

    if( isdatafile && st && boost::algorithm::iequals(*st,"ignore_missing") )
    {
        missing_error=OK;
        st = scanner.next();
    }
    else if( isdatafile && st && boost::algorithm::iequals(*st,"warn_missing") )
    {
        missing_error=INFO_ERROR;
        st = scanner.next();
    }
    else if( isdatafile && st && boost::algorithm::iequals(*st,"fail_missing") )
    {
        st = scanner.next();
    }

    if( !st )
    {
        send_config_error( cfg, MISSING_DATA, "Value of classification is missing");
        return OK;
    }

    const std::string_view classvalue = *st;

    st = scanner.next();

    if( ! action )
    {
        if( !st )
        {
            send_config_error( cfg, MISSING_DATA,"Classification commands needs ignore, reject, or error_factor specified");
            return OK;
        }

        if( boost::algorithm::iequals( *st, "reject" ) )
        {
            action |= OBS_MOD_REJECT;
        }
        else if( boost::algorithm::iequals( *st, "ignore" ) )
        {
            action |= OBS_MOD_IGNORE;
        }
        if( action ) st = scanner.next();
    }

    if( st )
    {
        // Mirrors the original's short-circuit chain (_stricmp!=0 ||
        // (st=strtok(...))==NULL || sscanf(...)!=1 || errfct<=0.0 ||
        // strtok(...)!=NULL) step by step, since dereferencing an empty
        // optional at any stage would be UB if done via a single chained
        // expression instead.
        bool valid = boost::algorithm::iequals(*st,"error_factor");
        std::optional<std::string_view> factorField;
        std::optional<double> value;
        if( valid )
        {
            factorField = scanner.next();
            valid = factorField.has_value();
        }
        if( valid )
        {
            value = parse_leading<double>(*factorField);
            valid = value.has_value() && *value > 0.0;
        }
        if( valid )
        {
            valid = ! scanner.next().has_value();
        }
        if( ! valid )
        {
            send_config_error(cfg, INVALID_DATA, "Invalid or missing data in classification command");
            return OK;
        }
        errfct = *value;
        if( errfct != 1.0 ) action |= OBS_MOD_REWEIGHT;
    }

    if( ! action ) return OK;

    obs_modifications=snap_obs_modifications( true );

    add_obs_modifications_classification(cfg,obs_modifications,classification,classvalue,
            action,errfct,missing_error);

    return OK;
}

int read_recode_command( CFG_FILE *cfg, std::string_view string, void *, int, int )
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

int read_job_title_command( CFG_FILE *, const std::string_view string, void *, int, int )
{
    job_title = std::string( string.substr( 0, JOBTITLELEN-1 ) );
    return OK;
}

