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

#include "network/networkb.h"
#include "snap/stnadj.h"
#include "snap/snapglob.h"
#include "snap/snapcsvstn.h"
#include "util/chkalloc.h"
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
char *station_filename = NULL;
char *station_filespec = NULL;
char *output_station_filespec = NULL;
int station_filetype = STN_FORMAT_SNAP;
char *station_fileoptions = 0;

char *geoid_file = 0;
char overwrite_geoid = 0;
int geoid_error_level = WARNING_ERROR;

static void delete_stn_adjustment( station *st )
{
    if( st && st->hook ) check_free( st->hook );
}

static void create_stn_adjustment( station *st )
{
    stn_adjustment *sa;
    delete_stn_adjustment( st );
    sa = (stn_adjustment *) check_malloc( sizeof(stn_adjustment) );
    sa->initELat = st->ELat;
    sa->initELon = st->ELon;
    sa->initOHgt = st->OHgt;
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
    st->hook=(void *) sa;
}

void set_stnadj_init_network( void )
{
    set_network_initstn_func( 0, create_stn_adjustment, delete_stn_adjustment );
}

static void clear_stnadj_globals( void )
{
    void *obsmod;
    if( net ) delete_network( net );
    obsmod=snap_obs_modifications( false );

    if( obsmod ) set_obs_modifications_network( obsmod, NULL );
    if( stnrecode ) delete_stn_recode_map( stnrecode );
    if( station_filename ) check_free( station_filename );
    if( station_filespec ) check_free( station_filespec );
    if( station_fileoptions ) check_free( station_fileoptions );
    net = NULL;
    stnrecode = NULL;
    station_filename = NULL;
    station_filespec = NULL;
    station_fileoptions = NULL;
}

void set_output_station_file( const char *fname )
{
    if( output_station_filespec ) check_free( output_station_filespec );
    output_station_filespec=copy_string(fname);
}



int read_station_file( const char *fname, const char *base_dir, int format, const char *options, int mergeopts )
{
    int nch, sts;
    network *stndata;
    char *stnfile=0;

    if( ! net ) clear_stnadj_globals();

    nch = strlen( fname ) + (base_dir ? strlen(base_dir) : 0) + 1;
    stnfile = (char *) check_malloc( nch );
    build_filespec( stnfile, nch, base_dir, fname, NULL );
    if( !file_exists(stnfile ) ) strcpy( stnfile, fname );
    if( options ) station_fileoptions = copy_string( options );

    stndata = new_network();
    switch( format )
    {
    case STN_FORMAT_SNAP:
        sts = read_network( stndata, stnfile, 0 );
        break;
    case STN_FORMAT_GB:
        sts = read_network( stndata, stnfile, NW_READOPT_GBFORMAT );
        break;
    case STN_FORMAT_CSV:
        sts = load_snap_csv_stations( stndata, stnfile, station_fileoptions );
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
            void *obsmod=snap_obs_modifications( false );
            station_filename = copy_string( fname );
            station_filespec = copy_string( stnfile );
            if( ! output_station_filespec )
            {
                nch=path_len(station_filespec,1);
                output_station_filespec = (char *) check_malloc(nch+strlen(NEWSTNFILE_EXT)+1);
                memcpy(output_station_filespec,station_filespec,nch);
                strcpy(output_station_filespec+nch,NEWSTNFILE_EXT);
            }
            net=stndata;
            if( obsmod ) set_obs_modifications_network( obsmod, net );
        }
        else
        {
            if( ! mergeopts ) mergeopts = NW_MERGEOPT_ADDNEW;
            merge_network( net, stndata, mergeopts, 0 );
            delete_network(stndata);
        }
    }
    else
    {
        delete_network(stndata);
        clear_stnadj_globals();
    }

    check_free( stnfile );

    return sts;
}


/* Routine to write a station file */

static int skip_rejected;

static int check_rejected( station *st )
{
    if( !skip_rejected ) return 1;
    if( stnadj(st)->flag.rejected ) return 0;
    return 1;
}

int write_station_file( const char *prog, const char *fname, const char *ver, const char *rtime,
                        int coord_precision, char rejected )
{
    char comment[256];

    if( station_filetype != STN_FORMAT_SNAP )
    {
        handle_error( INVALID_DATA, "Can only write SNAP format coordinate files", NO_MESSAGE );
        return INVALID_DATA;
    }

    if( ! fname ) fname=output_station_filespec;
    if( ! ver ) ver=PROGRAM_VERSION;
    if( ! rtime ) rtime=get_date(0);

    if( ! fname )
    {
        handle_error( INVALID_DATA, "Coordinate file not written as no filename defined", NO_MESSAGE );
        return INVALID_DATA;
    }

    skip_rejected = !rejected;

    if( prog )
    {
        sprintf(comment,"Updated by %s version %s at %s",prog,ver,rtime);
    }
    else
    {
        sprintf(comment,"Updated at %s",rtime);
    }

    int sts=write_network( net, fname, comment, coord_precision,
                          check_rejected );
    if( sts == OK )
    {
        record_filename( fname, "output_station_coordinate" );
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
    if( output_station_filespec ) check_free( output_station_filespec );
    output_station_filespec = NULL;
}
