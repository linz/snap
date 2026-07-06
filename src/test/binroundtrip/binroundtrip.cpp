#include "snapconfig.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>

#include "util/errdef.h"
#include "util/binfile.h"
#include "util/bltmatrx.h"
#include "util/fileutil.h"
#include "coordsys/coordsys.h"
#include "snap/filenames.h"
#include "snap/snapglob.h"
#include "snap/snapglob_bin.h"
#include "snap/stnadj.h"
#include "snap/survfile.h"
#include "snap/rftrndmp.h"
#include "snap/genparam.h"
#include "snap/bindata.h"
#include "network/network.h"

// binroundtrip: reloads every section of a .bin file into real
// in-memory objects, using this codebase's own reload_* functions.
// Then it re-dumps every section with the matching dump_* functions.
//
// This exists to prove the .bin format round-trips byte-for-byte on
// either compiler. See BINFILE_FORMAT.md.
//
// It deliberately does NOT call snap's own dump_station_covariances,
// dump_relative_covariances, dump_choleski_decomposition, or
// dump_covariance_matrix. Those read from live adjustment-engine state
// (lsq_get_params, lsq_get_covariance_matrix, lsq_get_decomposition)
// that nothing here repopulates. Instead this tool reads and writes
// the raw on-disk records for those sections directly.
//
// Every section is required. STATION_COVARIANCES,
// STATION_RELATIVE_COVARIANCES, CHOLESKI_DECOMPOSITION, and
// FULL_COVARIANCE are optional in a real snap-produced .bin file, but
// this tool's whole purpose is to verify a test job that turns every
// one of those options on - so a missing section here is a test
// failure, not something to skip quietly.

[[noreturn]] static void fail( const std::string &message )
{
    throw std::runtime_error( message );
}

struct BinaryFileCloser { void operator()( BINARY_FILE *b ) const { if( b ) { close_binary_file( b ); } } };
using BinaryFilePtr = std::unique_ptr<BINARY_FILE, BinaryFileCloser>;

struct BindataDeleter { void operator()( bindata *b ) const { delete_bindata( b ); } };
using BindataPtr = std::unique_ptr<bindata, BindataDeleter>;

struct BltMatrixDeleter { void operator()( bltmatrix *blt ) const { delete_bltmatrix( blt ); } };
using BltMatrixPtr = std::unique_ptr<bltmatrix, BltMatrixDeleter>;

struct StationCovariances
{
    std::vector<std::array<double, 6>> rows;
};

static void reload_station_covariances_raw( BINARY_FILE *b, StationCovariances &cvr )
{
    if( find_section( b, "STATION_COVARIANCES" ) != OK ) {
        fail( "Missing section: STATION_COVARIANCES" );
    }
    const int nstn = number_of_stations( net );
    cvr.rows.resize( nstn );
    for( auto &row : cvr.rows ) {
        for( double &v : row ) {
            read_raw( b->f, v );
        }
    }
    check_end_section( b );
}

static void dump_station_covariances_raw( BINARY_FILE *b, const StationCovariances &cvr )
{
    create_section( b, "STATION_COVARIANCES" );
    for( const auto &row : cvr.rows ) {
        for( const double v : row ) {
            write_raw( b->f, v );
        }
    }
    end_section( b );
}

struct RelativeCovariances
{
    struct Entry { int from; int to; std::array<double, 6> cvr; };
    std::vector<Entry> entries;
};

static void reload_relative_covariances_raw( BINARY_FILE *b, RelativeCovariances &rc )
{
    if( find_section( b, "STATION_RELATIVE_COVARIANCES" ) != OK ) {
        fail( "Missing section: STATION_RELATIVE_COVARIANCES" );
    }
    for( ;; ) {
        int32_t from;
        read_raw( b->f, from );
        if( from < 0 ) {
            break;
        }
        RelativeCovariances::Entry e;
        e.from = from;
        read_raw_as<int32_t>( b->f, e.to );
        for( double &v : e.cvr ) {
            read_raw( b->f, v );
        }
        rc.entries.push_back( e );
    }
    check_end_section( b );
}

static void dump_relative_covariances_raw( BINARY_FILE *b, const RelativeCovariances &rc )
{
    create_section( b, "STATION_RELATIVE_COVARIANCES" );
    for( const auto &e : rc.entries ) {
        write_raw_as<int32_t>( b->f, e.from );
        write_raw_as<int32_t>( b->f, e.to );
        for( const double v : e.cvr ) {
            write_raw( b->f, v );
        }
    }
    write_raw_as<int32_t>( b->f, -1 );
    end_section( b );
}

// Reloads every OBSERVATIONS record from `in`, immediately re-writing
// each one to `out`. bindata_file (snap/bindata.h) is a single shared
// global FILE*. Flipping it between in->f and out->f around each
// record avoids needing a durable copy of survdata's internal
// pointers, which reset_survdata_pointers only wires up correctly
// relative to the one buffer get_bindata just populated.
static void copy_observations( BINARY_FILE *in, BINARY_FILE *out )
{
    if( find_section( in, "OBSERVATIONS" ) != OK ) {
        fail( "Missing section: OBSERVATIONS" );
    }

    create_section( out, "OBSERVATIONS" );
    init_bindata( out->f );

    const BindataPtr bd( create_bindata() );
    for( ;; ) {
        bindata_file = in->f;
        const int sts = get_bindata( ANYDATATYPE, bd.get() );
        if( sts == NO_MORE_DATA ) {
            break;
        }

        bindata_file = out->f;
        if( bd->bintype == SURVDATA ) {
            save_survdata( static_cast<survdata *>( bd->data ) );
        } else {
            write_bindata_header( bd->size, NOTEDATA );
            fwrite( bd->data, bd->size, 1, bindata_file );
        }
    }

    bindata_file = out->f;
    end_bindata();
    end_section( out );
}

static int run( int argc, char *argv[] )
{
    if( argc != 3 ) {
        fail( "Syntax: binroundtrip input.bin output.bin" );
    }

    init_snap_globals();
    install_default_projections();
    install_default_crdsys_file();
    // dump_filenames (survfile.cpp) reads each file's context_definition(),
    // which needs a base file_context established via push_file_context -
    // normally done from the command file's directory (snapglob.cpp), but
    // this tool never reads a command file. "." matches running from the
    // same directory as the input .bin file, which is the expected usage.
    push_file_context( "." );

    BinaryFilePtr in( open_binary_file( argv[1], BINFILE_SIGNATURE ) );
    if( !in ) {
        fail( std::string( "Cannot open " ) + argv[1] );
    }

    if( reload_snap_globals( in.get() ) != OK ) {
        fail( "Cannot reload section: SNAP_GLOBALS" );
    }
    if( reload_stations( in.get() ) != OK ) {
        fail( "Cannot reload section: STNADJ" );
    }

    StationCovariances stn_cvr;
    reload_station_covariances_raw( in.get(), stn_cvr );

    RelativeCovariances rel_cvr;
    reload_relative_covariances_raw( in.get(), rel_cvr );

    if( reload_filenames( in.get() ) != OK ) {
        fail( "Cannot reload section: DATA_FILES" );
    }
    if( reload_obs_classes( in.get() ) != OK ) {
        fail( "Cannot reload section: OBS_CLASSES" );
    }
    if( reload_rftransformations( in.get() ) != OK ) {
        fail( "Cannot reload section: RFTRANSFORMATIONS" );
    }
    if( reload_parameters( in.get() ) != OK ) {
        fail( "Cannot reload section: MISCPARAMS" );
    }

    if( find_section( in.get(), "CHOLESKI_DECOMPOSITION" ) != OK ) {
        fail( "Missing section: CHOLESKI_DECOMPOSITION" );
    }
    bltmatrix *choleski_raw = nullptr;
    reload_bltmatrix( &choleski_raw, in->f );
    const BltMatrixPtr choleski( choleski_raw );
    check_end_section( in.get() );

    if( find_section( in.get(), "FULL_COVARIANCE" ) != OK ) {
        fail( "Missing section: FULL_COVARIANCE" );
    }
    bltmatrix *full_covariance_raw = nullptr;
    reload_bltmatrix_dense( &full_covariance_raw, in->f );
    const BltMatrixPtr full_covariance( full_covariance_raw );
    check_end_section( in.get() );

    BinaryFilePtr out( create_binary_file( argv[2], BINFILE_SIGNATURE ) );
    if( !out ) {
        fail( std::string( "Cannot create " ) + argv[2] );
    }

    // Write order must match snap's own dump order exactly (snapmain.cpp),
    // for the output to be byte-identical to a real snap-produced .bin.
    copy_observations( in.get(), out.get() );
    in.reset();

    create_section( out.get(), "CHOLESKI_DECOMPOSITION" );
    dump_bltmatrix( choleski.get(), out->f );
    end_section( out.get() );

    create_section( out.get(), "FULL_COVARIANCE" );
    dump_bltmatrix_dense( full_covariance.get(), out->f );
    end_section( out.get() );

    dump_snap_globals( out.get() );
    dump_stations( out.get() );
    dump_station_covariances_raw( out.get(), stn_cvr );
    dump_relative_covariances_raw( out.get(), rel_cvr );
    dump_filenames( out.get() );
    dump_obs_classes( out.get() );
    dump_rftransformations( out.get() );
    dump_parameters( out.get() );

    return 0;
}

int main( int argc, char *argv[] )
{
    try {
        return run( argc, argv );
    } catch( const std::exception &e ) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
