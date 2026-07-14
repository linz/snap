#include "snapconfig.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
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
#include "util/chkalloc.h"
#include "util/classify.h"
#include "coordsys/coordsys.h"
#include "snap/filenames.h"
#include "snap/snapglob.h"
#include "snap/snapglob_bin.h"
#include "snap/stnadj.h"
#include "snap/survfile.h"
#include "snap/rftrndmp.h"
#include "snap/rftrans.h"
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

// --- Text dump mode ---
//
// Prints one value per line, as "label = value", so two dumps (e.g. one
// per platform) can be diffed with a tolerance-aware line comparison
// instead of a raw byte cmp - see run_test.py. Reuses the same reload_*
// calls and structs as the round-trip mode; this only adds printing.

// Same formatting dump_value below uses for the value+newline part, once
// its label prefix is written - factored out so sections with no per-field
// name to attach (dump_disk_fields_text, the OBSERVATIONS dump below) can
// call it directly, without a label. `long long`, not `long`: some fields
// printed through this (trgtdata::noteloc, survdata.h) are genuinely 64-bit
// (int64_t) - `long` is only 32 bits on Windows/MSVC (LLP64), so a `long`
// overload here would silently truncate a value that this dump exists
// specifically to compare byte-for-byte across platforms.
static void dump_bare_value( std::ostream &out, double value )
{
    out << std::setprecision(17) << value << "\n";
}

static void dump_bare_value( std::ostream &out, long long value )
{
    out << value << "\n";
}

static void dump_value( std::ostream &out, const std::string &label, double value )
{
    out << label << " = ";
    dump_bare_value( out, value );
}

static void dump_value( std::ostream &out, const std::string &label, long value )
{
    out << label << " = ";
    dump_bare_value( out, static_cast<long long>(value) );
}

static void dump_value( std::ostream &out, const std::string &label, const std::string &value )
{
    out << label << " = " << value << "\n";
}

// Shared by Network's station classifications and OBS_CLASSES - both
// are plain classifications structs, dumped via the same
// dump_classifications (util/classify.cpp).
static void dump_classifications_text( std::ostream &out, const std::string &section, const classifications &csf )
{
    for( int ic = 0; ic < csf.class_count; ic++ ) {
        const class_type *cl = csf.class_index[ic];
        const std::string p = section + "[" + std::to_string(ic) + "].";
        dump_value( out, p+"name", cl->name ? cl->name : "" );
        dump_value( out, p+"count", static_cast<long>(cl->count) );
        dump_value( out, p+"type", cl->type == ClassValueType::Int ? "Int" : "Char" );
        for( int iv = 0; iv < cl->count; iv++ ) {
            const class_value *cv = cl->value[iv];
            const std::string vp = p+"value["+std::to_string(iv)+"].";
            if( cl->type == ClassValueType::Int ) {
                dump_value( out, vp+"value", static_cast<long>(cv->value.value) );
            } else {
                dump_value( out, vp+"value", cv->value.name ? cv->value.name : "" );
            }
            dump_value( out, vp+"usage", static_cast<long>(cv->usage) );
            dump_value( out, vp+"error_factor", cv->error_factor );
        }
    }
}

// Field order mirrors dump_snap_globals/reload_snap_globals (snapglob.cpp) exactly.
static void dump_snap_globals_text( std::ostream &out )
{
    dump_value( out, "SNAP_GLOBALS.job_title", std::string(job_title) );
    dump_value( out, "SNAP_GLOBALS.run_time", std::string(run_time) );
    dump_value( out, "SNAP_GLOBALS.dimension", static_cast<long>(dimension) );
    dump_value( out, "SNAP_GLOBALS.program_mode", static_cast<long>(program_mode) );
    dump_value( out, "SNAP_GLOBALS.nobs", static_cast<long>(nobs) );
    dump_value( out, "SNAP_GLOBALS.nprm", static_cast<long>(nprm) );
    dump_value( out, "SNAP_GLOBALS.nschp", static_cast<long>(nschp) );
    dump_value( out, "SNAP_GLOBALS.ncon", static_cast<long>(ncon) );
    dump_value( out, "SNAP_GLOBALS.dof", static_cast<long>(dof) );
    dump_value( out, "SNAP_GLOBALS.ssr", ssr );
    dump_value( out, "SNAP_GLOBALS.seu", seu );
    dump_value( out, "SNAP_GLOBALS.iterations", static_cast<long>(iterations) );
    dump_value( out, "SNAP_GLOBALS.converged", static_cast<long>(converged) );
    dump_value( out, "SNAP_GLOBALS.apriori", static_cast<long>(apriori) );
    dump_value( out, "SNAP_GLOBALS.flag_level[0]", flag_level[0] );
    dump_value( out, "SNAP_GLOBALS.flag_level[1]", flag_level[1] );
    dump_value( out, "SNAP_GLOBALS.taumax[0]", static_cast<long>(taumax[0]) );
    dump_value( out, "SNAP_GLOBALS.taumax[1]", static_cast<long>(taumax[1]) );
    dump_value( out, "SNAP_GLOBALS.coord_precision", static_cast<long>(coord_precision) );
    dump_value( out, "SNAP_GLOBALS.have_obs_ids", static_cast<long>(have_obs_ids) );
    dump_value( out, "SNAP_GLOBALS.errconflim", static_cast<long>(errconflim) );
    dump_value( out, "SNAP_GLOBALS.errconfval", errconfval );
}

// Field order mirrors stn_adjustment (stnadj.h) and dump_stnadj_flags (stnadj.cpp).
static void dump_stnadj_text( std::ostream &out )
{
    const int nstns = number_of_stations( net );
    for( int istn = 1; istn <= nstns; istn++ ) {
        const stn_adjustment *sa = stnadj( stnptr( istn ) );
        const std::string p = "STNADJ[" + std::to_string(istn) + "].";
        dump_value( out, p+"initELat", sa->initELat );
        dump_value( out, p+"initELon", sa->initELon );
        dump_value( out, p+"initOHgt", sa->initOHgt );
        dump_value( out, p+"hrowno", static_cast<long>(sa->hrowno) );
        dump_value( out, p+"vrowno", static_cast<long>(sa->vrowno) );
        dump_value( out, p+"nobsprm", static_cast<long>(sa->nobsprm) );
        dump_value( out, p+"obscount", static_cast<long>(sa->obscount) );
        dump_value( out, p+"idcol", static_cast<long>(sa->idcol) );
        dump_value( out, p+"herror", static_cast<double>(sa->herror) );
        dump_value( out, p+"verror", static_cast<double>(sa->verror) );
        dump_value( out, p+"flag.adj_h", static_cast<long>(sa->flag.adj_h) );
        dump_value( out, p+"flag.adj_v", static_cast<long>(sa->flag.adj_v) );
        dump_value( out, p+"flag.float_h", static_cast<long>(sa->flag.float_h) );
        dump_value( out, p+"flag.float_v", static_cast<long>(sa->flag.float_v) );
        dump_value( out, p+"flag.observed", static_cast<long>(sa->flag.observed) );
        dump_value( out, p+"flag.ignored", static_cast<long>(sa->flag.ignored) );
        dump_value( out, p+"flag.rejected", static_cast<long>(sa->flag.rejected) );
        dump_value( out, p+"flag.autoreject", static_cast<long>(sa->flag.autoreject) );
        dump_value( out, p+"flag.noreorder", static_cast<long>(sa->flag.noreorder) );
        dump_value( out, p+"flag.auto_h", static_cast<long>(sa->flag.auto_h) );
        dump_value( out, p+"flag.auto_v", static_cast<long>(sa->flag.auto_v) );
    }
}

// Walks `table` via for_each_disk_field (util/binfile.h) - the same table
// write_station_fixed_width/write_rftrans_fixed_width/write_param_fixed_width
// use to write real bytes - printing one value per line instead, with no
// per-field label: a diff between two dumps is triaged by counting lines
// from the last "=== section ===" marker and cross-checking the table/struct
// source, the same way a raw byte `cmp` is triaged by offset today.
//
// The lambda below is the text *action* passed to for_each_disk_field.
// write_disk_field (binfile.h) is the binary action passed for the exact
// same loop, from write_station_fixed_width/write_rftrans_fixed_width/
// write_param_fixed_width - it never touches a newline, since raw bytes have
// no line structure. This one always appends "\n" after formatting a value,
// unconditionally, on every call - that's the entire difference between the
// binary and text paths through for_each_disk_field.
template<class T>
static void dump_disk_fields_text( std::ostream &out, const T &base, const DiskField *table, size_t table_size )
{
    for_each_disk_field( base, table, table_size,
        [&out]( FieldKind kind, auto value ) {
            if( kind == FieldKind::Float64 ) dump_bare_value( out, static_cast<double>(value) );
            else dump_bare_value( out, static_cast<long long>(value) );
        } );
}

// Field order mirrors write_station_fixed_width's STATION_DISK_FIELDS
// (netstns1.cpp) plus dump_station's trailing classval/Name (also
// netstns1.cpp) - same order the real on-disk format writes them in.
static void dump_station_text( std::ostream &out, const std::string &section, const station *st )
{
    out << "=== " << section << " ===\n";
    dump_disk_fields_text( out, *st, STATION_DISK_FIELDS, STATION_DISK_FIELD_COUNT );
    for( int i = 0; i < st->nclass; i++ ) out << static_cast<long>(st->classval[i]) << "\n";
    out << (st->Name ? st->Name : "") << "\n";
}

// Field order mirrors dump_network (networkd.cpp): name/crdsysdef/topocentre/
// options, then stnclasses, then the station list itself.
static void dump_network_text( std::ostream &out )
{
    dump_value( out, "Network.name", net->name ? net->name : "" );
    dump_value( out, "Network.crdsysdef", net->crdsysdef ? net->crdsysdef : "" );
    dump_value( out, "Network.topolat", net->topolat );
    dump_value( out, "Network.topolon", net->topolon );
    dump_value( out, "Network.got_topocentre", static_cast<long>(net->got_topocentre) );
    dump_value( out, "Network.options", static_cast<long>(net->options) );
    dump_value( out, "Network.orderclsid", static_cast<long>(net->orderclsid) );
    dump_classifications_text( out, "Network.stnclasses", net->stnclasses );
    const int nstns = number_of_stations( net );
    for( int istn = 1; istn <= nstns; istn++ ) {
        dump_station_text( out, "Network.station["+std::to_string(istn)+"]", stnptr( istn ) );
    }
}

// Field order mirrors dump_filenames (survfile.cpp): format, name, subtype,
// recodefile, then context_definition(context). name/recodefile go through
// portable_path (fileutil.cpp) the same way dump_filepath does - the exact
// conversion the DATA_FILES cross-platform fix (13d7ee04) added - so a
// regression there would show up here, not just in a raw byte cmp.
static void dump_filenames_text( std::ostream &out )
{
    const int nfiles = survey_data_file_count();
    for( int i = 0; i < nfiles; i++ ) {
        const survey_data_file *sd = survey_data_file_ptr( i );
        out << "=== DATA_FILES[" << i << "] ===\n";
        out << sd->format << "\n";
        out << portable_path( sd->name ? sd->name : "" ) << "\n";
        out << (sd->subtype ? sd->subtype : "") << "\n";
        out << portable_path( sd->recodefile ? sd->recodefile : "" ) << "\n";
        const char *context_def = context_definition( sd->context );
        out << (context_def ? context_def : "") << "\n";
        check_free( (void*)context_def );
    }
}

// Field order mirrors write_rftrans_fixed_width's RFTRANS_DISK_FIELDS
// (rftrndmp.cpp), then the 12 bitfields in the same declared order
// pack_rftrans_flags packs them in (rftrndmp.cpp) - not part of the table,
// since bitfields have no address for offsetof to take - then trailing
// name, matching dump_rftransformations' on-disk write order.
static void dump_rftransformations_text( std::ostream &out )
{
    const int nrf = rftrans_count();
    for( int irf = 1; irf <= nrf; irf++ ) {
        const rfTransformation *rf = rftrans_from_id( irf );
        out << "=== RFTRANSFORMATIONS[" << irf << "] ===\n";
        dump_disk_fields_text( out, *rf, RFTRANS_DISK_FIELDS, RFTRANS_DISK_FIELD_COUNT );
        out << rf->istopo << "\n" << rf->isiers << "\n" << rf->userates << "\n" << rf->usetrans << "\n"
            << rf->localoriginok << "\n" << rf->localorigin << "\n" << rf->calctrans << "\n"
            << rf->calcrot << "\n" << rf->calcscale << "\n" << rf->calctransrate << "\n"
            << rf->calcrotrate << "\n" << rf->calcscalerate << "\n";
        out << (rf->name ? rf->name : "") << "\n";
    }
}

// Field order mirrors write_param_fixed_width's PARAM_DISK_FIELDS
// (genparam.cpp), then trailing name, matching dump_parameters' on-disk
// write order.
static void dump_parameters_text( std::ostream &out )
{
    const int nparam = param_count();
    for( int pid = 1; pid <= nparam; pid++ ) {
        const param *p = param_from_id( pid );
        out << "=== MISCPARAMS[" << pid << "] ===\n";
        dump_disk_fields_text( out, *p, PARAM_DISK_FIELDS, PARAM_DISK_FIELD_COUNT );
        out << (p->name ? p->name : "") << "\n";
    }
}

static void dump_station_covariances_text( std::ostream &out, const StationCovariances &cvr )
{
    for( size_t i = 0; i < cvr.rows.size(); i++ ) {
        for( size_t j = 0; j < cvr.rows[i].size(); j++ ) {
            dump_value( out, "STATION_COVARIANCES["+std::to_string(i)+"]["+std::to_string(j)+"]", cvr.rows[i][j] );
        }
    }
}

static void dump_relative_covariances_text( std::ostream &out, const RelativeCovariances &rc )
{
    for( size_t i = 0; i < rc.entries.size(); i++ ) {
        const std::string p = "STATION_RELATIVE_COVARIANCES[" + std::to_string(i) + "].";
        dump_value( out, p+"from", static_cast<long>(rc.entries[i].from) );
        dump_value( out, p+"to", static_cast<long>(rc.entries[i].to) );
        for( size_t j = 0; j < rc.entries[i].cvr.size(); j++ ) {
            dump_value( out, p+"cvr["+std::to_string(j)+"]", rc.entries[i].cvr[j] );
        }
    }
}

// Covers both banded (CHOLESKI_DECOMPOSITION) and dense (FULL_COVARIANCE)
// matrices - dense is just banded with every row's col fixed at 0, so one
// function handles both by reading each row's real populated band as-is.
static void dump_bltmatrix_text( std::ostream &out, const std::string &section, const bltmatrix *blt )
{
    for( int i = 0; i < blt->nrow; i++ ) {
        const bltrow &row = blt->row[i];
        for( int j = row.col; j <= i; j++ ) {
            const std::string label = section+"["+std::to_string(i)+"]["+std::to_string(j)+"]";
            dump_value( out, label, row.address[j - row.col] );
        }
    }
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

// Field order mirrors trgtdata's declared order (survdata.h) - the shared
// prefix every obsdata/vecdata/pntdata record starts with. No on-disk
// field table exists for this (unlike SURVDATA_DISK_FIELDS): trgtdata and
// its variants are part of OBSERVATIONS' raw fixed-stride blob, not the
// fixed-width survdata header, so there's nothing to reuse here the way
// dump_disk_fields_text reuses SURVDATA_DISK_FIELDS.
static void dump_trgtdata_text( std::ostream &out, const trgtdata &tgt )
{
    dump_bare_value( out, static_cast<long long>(tgt.to) );
    dump_bare_value( out, tgt.tohgt );
    dump_bare_value( out, static_cast<long long>(tgt.type) );
    dump_bare_value( out, static_cast<long long>(tgt.obsid) );
    dump_bare_value( out, static_cast<long long>(tgt.id) );
    dump_bare_value( out, static_cast<long long>(tgt.lineno) );
    dump_bare_value( out, static_cast<long long>(tgt.nclass) );
    dump_bare_value( out, static_cast<long long>(tgt.iclass) );
    dump_bare_value( out, static_cast<long long>(tgt.nsyserr) );
    dump_bare_value( out, static_cast<long long>(tgt.isyserr) );
    dump_bare_value( out, static_cast<long long>(tgt.unused) );
    dump_bare_value( out, static_cast<long long>(tgt.noteloc) );
    dump_bare_value( out, tgt.errfct );
}

// obsdata's own fields beyond the shared trgtdata prefix (survdata.h).
static void dump_obsdata_text( std::ostream &out, const obsdata &od )
{
    dump_trgtdata_text( out, od.tgt );
    dump_bare_value( out, od.value );
    dump_bare_value( out, od.error );
    dump_bare_value( out, od.calc );
    dump_bare_value( out, od.calcerr );
    dump_bare_value( out, od.residual );
    dump_bare_value( out, od.reserr );
    dump_bare_value( out, od.sres );
    dump_bare_value( out, static_cast<long long>(od.refcoef) );
    dump_bare_value( out, static_cast<long long>(od.prm_id) );
}

// vecdata's own fields beyond the shared trgtdata prefix (survdata.h).
static void dump_vecdata_text( std::ostream &out, const vecdata &vd )
{
    dump_trgtdata_text( out, vd.tgt );
    for( int i = 0; i < 3; i++ ) dump_bare_value( out, vd.vector[i] );
    for( int i = 0; i < 3; i++ ) dump_bare_value( out, vd.calc[i] );
    for( int i = 0; i < 3; i++ ) dump_bare_value( out, vd.residual[i] );
    // Labeled, unlike every other field here: vsres divides a residual by
    // a Cholesky-decomposed covariance pivot (vector_standardised_residual,
    // gpscvr.cpp) - a near-singular pivot amplifies ordinary cross-compiler
    // rounding noise well past this dump's default tolerance, without the
    // underlying computation being wrong. compare_dump.py widens the
    // tolerance specifically for this label; it needs a label to do that
    // at all, since a bare value carries no way to single it out.
    dump_value( out, "vsres", vd.vsres );
    dump_bare_value( out, static_cast<long long>(vd.rank) );
}

// pntdata's own fields beyond the shared trgtdata prefix (survdata.h).
static void dump_pntdata_text( std::ostream &out, const pntdata &pd )
{
    dump_trgtdata_text( out, pd.tgt );
    dump_bare_value( out, pd.value );
    dump_bare_value( out, pd.error );
    dump_bare_value( out, pd.calc );
    dump_bare_value( out, pd.calcerr );
    dump_bare_value( out, pd.residual );
    dump_bare_value( out, pd.reserr );
    dump_bare_value( out, pd.sres );
}

static void dump_classdata_text( std::ostream &out, const classdata &cd )
{
    dump_bare_value( out, static_cast<long long>(cd.class_id) );
    dump_bare_value( out, static_cast<long long>(cd.name_id) );
}

static void dump_syserrdata_text( std::ostream &out, const syserrdata &se )
{
    dump_bare_value( out, static_cast<long long>(se.prm_id) );
    dump_bare_value( out, se.influence );
}

// Reloads OBSERVATIONS record-by-record from `in` (same get_bindata loop as
// copy_observations, read-only here) and prints each as text, in the same
// order as the actual on-disk record layout (reset_survdata_pointers,
// bindata.cpp): the fixed-width header (SURVDATA_DISK_FIELDS), then the raw
// obs array (dispatched on sd->format), then classdata, then syserrdata,
// then - only when ncvr>0, vector data only - the three covariance
// matrices (cvr/calccvr/rescvr), each a flat packed lower-triangular array
// of ncvr*(ncvr+1)/2 doubles (util/symmatrx.h), dumped in that same flat
// order rather than via its 2D Lij indexing macro, since no 2D position
// needs preserving without per-field labels. NOTEDATA records (save_note,
// snap/notedata.cpp) are plain text, not struct fields: one flag byte
// (' ' if this note continues the previous one, '\n' if it starts a new
// one), then the note text verbatim, then a trailing '\n' and a NUL -
// bd->size is nch+3.
static void dump_observations_text( std::ostream &out, BINARY_FILE *in )
{
    if( find_section( in, "OBSERVATIONS" ) != OK ) {
        fail( "Missing section: OBSERVATIONS" );
    }

    bindata_file = in->f;
    const BindataPtr bd( create_bindata() );
    int irec = 0;
    for( ;; ) {
        const int sts = get_bindata( ANYDATATYPE, bd.get() );
        if( sts == NO_MORE_DATA ) {
            break;
        }
        irec++;

        if( bd->bintype != SURVDATA ) {
            out << "=== OBSERVATIONS[" << irec << "] (NOTEDATA) ===\n";
            const auto *note = static_cast<const unsigned char *>( bd->data );
            dump_bare_value( out, static_cast<long long>(note[0]) );
            const int64_t nch = bd->size - 3;
            out << std::string( reinterpret_cast<const char *>(note + 1), static_cast<size_t>(nch) ) << "\n";
            continue;
        }

        const survdata *sd = static_cast<const survdata *>( bd->data );
        out << "=== OBSERVATIONS[" << irec << "] ===\n";
        dump_disk_fields_text( out, *sd, SURVDATA_DISK_FIELDS, SURVDATA_DISK_FIELD_COUNT );
        for( int i = 0; i < sd->nobs; i++ ) {
            switch( sd->format ) {
            case SD_OBSDATA: dump_obsdata_text( out, sd->obs.odata[i] ); break;
            case SD_VECDATA: dump_vecdata_text( out, sd->obs.vdata[i] ); break;
            case SD_PNTDATA: dump_pntdata_text( out, sd->obs.pdata[i] ); break;
            }
        }
        for( int i = 0; i < sd->nclass; i++ ) dump_classdata_text( out, sd->clsf[i] );
        for( int i = 0; i < sd->nsyserr; i++ ) dump_syserrdata_text( out, sd->syserr[i] );
        if( sd->ncvr > 0 ) {
            const int n = sd->ncvr * (sd->ncvr + 1) / 2;
            for( int i = 0; i < n; i++ ) dump_bare_value( out, sd->cvr[i] );
            for( int i = 0; i < n; i++ ) dump_bare_value( out, sd->calccvr[i] );
            for( int i = 0; i < n; i++ ) dump_bare_value( out, sd->rescvr[i] );
        }
    }
}

// Everything reload_almost_everything() populates, other than the globals
// (snap globals, stations, filenames, obs classes, rftransformations,
// parameters) - those live in process-wide state, already reloaded by
// the reload_* calls below by the time this returns.
struct ReloadedState
{
    StationCovariances stn_cvr;
    RelativeCovariances rel_cvr;
    BltMatrixPtr choleski;
    BltMatrixPtr full_covariance;
};

// Reloads every section except OBSERVATIONS (hence "almost"). Round-trip
// mode reloads and rewrites OBSERVATIONS together via copy_observations,
// since it needs both `in` and `out` open simultaneously -
// dump_observations_text (only reading `in`) doesn't share that
// constraint, so it's called separately by its own caller instead of
// through here.
static ReloadedState reload_almost_everything( BINARY_FILE *in )
{
    if( reload_snap_globals( in ) != OK ) {
        fail( "Cannot reload section: SNAP_GLOBALS" );
    }
    if( reload_stations( in ) != OK ) {
        fail( "Cannot reload section: STNADJ" );
    }

    ReloadedState state;
    reload_station_covariances_raw( in, state.stn_cvr );
    reload_relative_covariances_raw( in, state.rel_cvr );

    if( reload_filenames( in ) != OK ) {
        fail( "Cannot reload section: DATA_FILES" );
    }
    if( reload_obs_classes( in ) != OK ) {
        fail( "Cannot reload section: OBS_CLASSES" );
    }
    if( reload_rftransformations( in ) != OK ) {
        fail( "Cannot reload section: RFTRANSFORMATIONS" );
    }
    if( reload_parameters( in ) != OK ) {
        fail( "Cannot reload section: MISCPARAMS" );
    }

    if( find_section( in, "CHOLESKI_DECOMPOSITION" ) != OK ) {
        fail( "Missing section: CHOLESKI_DECOMPOSITION" );
    }
    bltmatrix *choleski_raw = nullptr;
    reload_bltmatrix( &choleski_raw, in->f );
    state.choleski.reset( choleski_raw );
    check_end_section( in );

    if( find_section( in, "FULL_COVARIANCE" ) != OK ) {
        fail( "Missing section: FULL_COVARIANCE" );
    }
    bltmatrix *full_covariance_raw = nullptr;
    reload_bltmatrix_dense( &full_covariance_raw, in->f );
    state.full_covariance.reset( full_covariance_raw );
    check_end_section( in );

    return state;
}

static int run_roundtrip( const char *input_path, const char *output_path )
{
    BinaryFilePtr in( open_binary_file( const_cast<char *>(input_path), BINFILE_SIGNATURE ).file );
    if( !in ) {
        fail( std::string( "Cannot open " ) + input_path );
    }

    ReloadedState state = reload_almost_everything( in.get() );

    BinaryFilePtr out( create_binary_file( const_cast<char *>(output_path), BINFILE_SIGNATURE ) );
    if( !out ) {
        fail( std::string( "Cannot create " ) + output_path );
    }

    // Write order must match snap's own dump order exactly (snapmain.cpp),
    // for the output to be byte-identical to a real snap-produced .bin.
    copy_observations( in.get(), out.get() );
    in.reset();

    create_section( out.get(), "CHOLESKI_DECOMPOSITION" );
    dump_bltmatrix( state.choleski.get(), out->f );
    end_section( out.get() );

    create_section( out.get(), "FULL_COVARIANCE" );
    dump_bltmatrix_dense( state.full_covariance.get(), out->f );
    end_section( out.get() );

    dump_snap_globals( out.get() );
    dump_stations( out.get() );
    dump_station_covariances_raw( out.get(), state.stn_cvr );
    dump_relative_covariances_raw( out.get(), state.rel_cvr );
    dump_filenames( out.get() );
    dump_obs_classes( out.get() );
    dump_rftransformations( out.get() );
    dump_parameters( out.get() );

    return 0;
}

// --dump mode: reloads a .bin file and prints it as text (see the "Text
// dump mode" section above), for a tolerance-aware comparison across two
// files - a raw byte cmp can't tell a real regression from ordinary
// floating-point differences between compilers.
static int run_dump( const char *input_path, const char *output_path )
{
    BinaryFilePtr in( open_binary_file( const_cast<char *>(input_path), BINFILE_SIGNATURE ).file );
    if( !in ) {
        fail( std::string( "Cannot open " ) + input_path );
    }

    ReloadedState state = reload_almost_everything( in.get() );

    std::ofstream out( output_path );
    if( !out ) {
        fail( std::string( "Cannot create " ) + output_path );
    }

    dump_observations_text( out, in.get() );
    in.reset();

    dump_snap_globals_text( out );
    dump_network_text( out );
    dump_stnadj_text( out );
    dump_station_covariances_text( out, state.stn_cvr );
    dump_relative_covariances_text( out, state.rel_cvr );
    dump_filenames_text( out );
    dump_classifications_text( out, "OBS_CLASSES", obs_classes );
    dump_rftransformations_text( out );
    dump_parameters_text( out );
    dump_bltmatrix_text( out, "CHOLESKI_DECOMPOSITION", state.choleski.get() );
    dump_bltmatrix_text( out, "FULL_COVARIANCE", state.full_covariance.get() );

    return 0;
}

int main( int argc, char *argv[] )
{
    init_snap_globals();
    install_default_projections();
    install_default_crdsys_file();
    // dump_filenames (survfile.cpp) reads each file's context_definition(),
    // which needs a base file_context established via push_file_context -
    // normally done from the command file's directory (snapglob.cpp), but
    // this tool never reads a command file. "." matches running from the
    // same directory as the input .bin file, which is the expected usage.
    push_file_context( "." );

    try {
        if( argc == 4 && std::string(argv[1]) == "--dump" ) {
            return run_dump( argv[2], argv[3] );
        }
        if( argc == 3 ) {
            return run_roundtrip( argv[1], argv[2] );
        }
        fail( "Syntax: binroundtrip input.bin output.bin\n"
              "        binroundtrip --dump input.bin output.txt" );
    } catch( const std::exception &e ) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
