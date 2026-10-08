#include "snapconfig.h"
/*
   $Log: stnadj.c,v $
   Revision 1.1  1995/12/22 17:49:40  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <sstream>
#include <filesystem>

#include "network/networkb.h"
#include "snap/stnadj.h"
#include "snap/snapglob.h"
#include "snap/snapcsvstn.h"
#include "util/dstring.h"
#include "util/binfile.h"
#include "util/fileutil.h"
#include "util/filelist.h"
#include "util/get_date.h"
#include "util/errdef.h"
#include "util/getversion.h"

/* Global variables holding the definition of the network */

network *net = NULL;
stn_recode_map *stnrecode = NULL;
std::optional<StationFile> station_file;
std::string output_station_filespec;
int station_filetype = STN_FORMAT_SNAP;

std::optional<std::string> geoid_file;
char overwrite_geoid = 0;
int geoid_error_level = WARNING_ERROR;

static void delete_stn_adjustment( station *st )
{
    if( st ) delete static_cast<stn_adjustment *>( st->hook );
}

static void create_stn_adjustment( station *st )
{
    delete_stn_adjustment( st );
    stn_adjustment *sa = new stn_adjustment;
    sa->initELat = st->ELat;
    sa->initELon = st->ELon;
    sa->initOHgt = st->OHgt;
    sa->hrowno = 0;
    sa->vrowno = 0;
    sa->nobsprm = 0;
    sa->idcol = 0;
    sa->herror = 0.0;
    sa->verror = 0.0;
    sa->obscount = 0;
    sa->flag.adj_h = 1;
    sa->flag.adj_v = 1;
    sa->flag.float_h = 0;
    sa->flag.float_v = 0;
    sa->flag.observed = 0;
    sa->flag.ignored = 0;
    sa->flag.rejected = 0;
    sa->flag.autoreject = 0;
    sa->flag.noreorder = 0;
    sa->flag.auto_h = 0;
    sa->flag.auto_v = 0;
    st->hook=sa;
}

void set_stnadj_init_network( void )
{
    set_network_initstn_func( 0, create_stn_adjustment, delete_stn_adjustment );
}

static void clear_stnadj_globals( void )
{
    if( net ) delete_network( net );
    void *const obsmod=snap_obs_modifications( false );

    if( obsmod ) set_obs_modifications_network( obsmod, nullptr );
    if( stnrecode ) delete_stn_recode_map( stnrecode );
    net = nullptr;
    stnrecode = nullptr;
    station_file.reset();
}

void set_output_station_file( const std::string &fname )
{
    output_station_filespec = fname;
}



int read_station_file( const std::string &fname, const std::string &base_dir, const int format, const std::string &options, int mergeopts, const double mergedate )
{
    int sts;

    if( ! net ) clear_stnadj_globals();

    std::string stnfile = build_filespec( base_dir, fname, "" );
    if( !path_exists(stnfile ) ) stnfile = fname;

    network *const stndata = new_network();
    switch( format )
    {
    case STN_FORMAT_SNAP:
        sts = read_network( stndata, stnfile, 0 );
        break;
    case STN_FORMAT_GB:
        sts = read_network( stndata, stnfile, NW_READOPT_GBFORMAT );
        break;
    case STN_FORMAT_CSV:
        sts = load_snap_csv_stations( stndata, stnfile, options );
        break;
    default:
        handle_error( INVALID_DATA, "Invalid station file format specified", NO_MESSAGE );
        sts=INVALID_DATA;
        break;
    }


    if( sts == OK )
    {
        calculate_network_coordsys_geoid( stndata, INFO_ERROR ); 
        if( ! net )
        {
            void *const obsmod=snap_obs_modifications( false );
            station_file.emplace( StationFile{ fname, stnfile } );
            if( output_station_filespec.empty() )
            {
                output_station_filespec = std::filesystem::path( stnfile ).replace_extension( NEWSTNFILE_EXT ).string();
            }
            net=stndata;
            if( obsmod ) set_obs_modifications_network( obsmod, net );
        }
        else
        {
            if( ! mergeopts ) mergeopts = NW_MERGEOPT_ADDNEW;
            sts=merge_network( net, stndata, mergeopts, mergedate, 0 );
            delete_network(stndata);
        }
    }
    else
    {
        delete_network(stndata);
        clear_stnadj_globals();
    }

    return sts;
}


/* Routine to write a station file */

static bool skip_rejected;

static int check_rejected( station *st )
{
    if( !skip_rejected ) return 1;
    if( stnadj(st)->flag.rejected ) return 0;
    return 1;
}

int write_station_file( const std::optional<std::string> &prog, const std::optional<std::string> &fname,
                        const std::optional<std::string> &ver, const std::optional<std::string> &rtime,
                        const int coord_precision, const bool rejected )
{
    if( station_filetype != STN_FORMAT_SNAP )
    {
        handle_error( INVALID_DATA, "Can only write SNAP format coordinate files", NO_MESSAGE );
        return INVALID_DATA;
    }

    const std::string filename = fname ? *fname : output_station_filespec;
    const std::string version = ver ? *ver : PROGRAM_VERSION;
    const std::string run_time_text = rtime ? *rtime : get_date();

    if( filename.empty() )
    {
        handle_error( INVALID_DATA, "Coordinate file not written as no filename defined", NO_MESSAGE );
        return INVALID_DATA;
    }

    skip_rejected = !rejected;

    std::ostringstream comment;
    if( prog )
    {
        comment << "Updated by " << *prog << " version " << version << " at " << run_time_text;
    }
    else
    {
        comment << "Updated at " << run_time_text;
    }

    const int sts=write_network( net, filename, comment.str(), coord_precision,
                          check_rejected );
    if( sts == OK )
    {
        record_filename( filename, "output_station_coordinate" );
    }
    return sts;
}


/* Procedure to dump the station coordinates to a binary file */

static void dump_stnadj_flags( stn_adjustment *st, FILE *f )
{
    unsigned int flag;
    flag = 0;
    if( st->flag.adj_h )      flag += 1;
    if( st->flag.adj_v )      flag += 2;
    if( st->flag.float_h )    flag += 4;
    if( st->flag.float_v)     flag += 8;
    if( st->flag.observed )   flag += 16;
    if( st->flag.ignored )    flag += 32;
    if( st->flag.rejected )   flag += 64;
    if( st->flag.autoreject ) flag += 128;
    if( st->flag.noreorder  ) flag += 256;
    if( st->flag.auto_h  ) flag += 512;
    if( st->flag.auto_v  ) flag += 1024;
    fwrite( &flag, sizeof(flag), 1, f );
}

static void reload_stnadj_flags( stn_adjustment *st, FILE *f )
{
    unsigned int flag;
    fread( &flag, sizeof(flag), 1, f );
    st->flag.adj_h       = flag & 1  ? 1 : 0;
    st->flag.adj_v       = flag & 2  ? 1 : 0;
    st->flag.float_h     = flag & 4  ? 1 : 0;
    st->flag.float_v     = flag & 8  ? 1 : 0;
    st->flag.observed    = flag & 16 ? 1 : 0;
    st->flag.ignored     = flag & 32 ? 1 : 0;
    st->flag.rejected    = flag & 64 ? 1 : 0;
    st->flag.autoreject  = flag & 128 ? 1 : 0;
    st->flag.noreorder   = flag & 256 ? 1 : 0;
    st->flag.auto_h      = flag & 512 ? 1 : 0;
    st->flag.auto_v      = flag & 1024 ? 1 : 0;
}


void reset_stnadj_initial_coords( void )
{
    station *st;
    stn_adjustment *sa;
    int istn;
    for( istn = 0; istn++ < number_of_stations( net ); )
    {
        st=stnptr(istn);
        sa = stnadj(st);
        sa->initELat = st->ELat;
        sa->initELon = st->ELon;
        sa->initOHgt = st->OHgt;
    }
}

void dump_stations( BINARY_FILE *b )
{
    stn_adjustment *st;
    int istn;

    dump_network_to_bin( net, b );
    create_section(b,"STNADJ");

    /* Dump the station definitions */

    for( istn = 0; istn++ < number_of_stations( net ); )
    {
        st = stnadj(stnptr(istn));
        fwrite(st,sizeof(*st)-sizeof(st->flag),1,b->f);
        dump_stnadj_flags( st, b->f );
    }

    end_section( b );
}

int reload_stations( BINARY_FILE *b )
{
    station *st;
    stn_adjustment *sa;
    int istn, nstns, sts;

    /* Restore the critical static information */

    clear_stnadj_globals();
    net = new_network();
    sts = reload_network_from_bin( net, b );

    if( sts != OK ) return sts;

    if(find_section(b,"STNADJ") != OK ) return MISSING_DATA;

    /* Reload the station definitions */

    nstns = number_of_stations( net );
    for( istn = 0; istn++ < nstns; )
    {
        st = stnptr(istn);
        create_stn_adjustment( st );
        sa = stnadj(stnptr(istn));
        fread(sa,sizeof(stn_adjustment)-sizeof(sa->flag),1,b->f);
        reload_stnadj_flags( sa, b->f );
    }

    set_network_initstn_func( net, create_stn_adjustment, delete_stn_adjustment );
    return check_end_section( b );
}

void unload_stations( void )
{
    clear_stnadj_globals();
    output_station_filespec.clear();
}
