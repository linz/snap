#include "snapconfig.h"
/*
   $Log: rftrndmp.c,v $
   Revision 1.1  1995/12/22 17:47:24  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <cstddef>

#include "util/errdef.h"
#include "util/binfile.h"
#include "snap/rftrndmp.h"
#include "snap/rftrans.h"
#include "util/dstring.h"

// Single source of truth for the fixed-width on-disk rfTransformation layout,
// excluding `name` (handled separately via dump_string/reload_string, since it's
// a pointer) and the 12 `unsigned x:1` bitfields (packed into one uint16_t below -
// bitfields have no address for offsetof to take, so they can't join this table).
// Order here matches rftrans.h's declared field order for readability, though
// both write/read functions just iterate this same table so it isn't load-bearing.
//
// The trailing matrices (tmat, invtmat, tmatrate, invtmatrate, dtmatdrot, toporot,
// invtoporot) are genuinely part of the file format, not just recomputable derived
// state: setup_rftrans()/setup_rftrans_list() (rftrans.cpp) are only ever called by
// snap itself, before it dumps a .bin file - snaplist and snapplot both reload a
// .bin file and read these matrices straight off the loaded struct (via
// rftrans_tmat/rftrans_invtmat) with no intervening setup call, so the disk-stored
// values are load-bearing for those two programs. (setup_rftrans also leaves
// tmatrate/invtmatrate untouched when !userates, which would matter if this were
// being recomputed on load - it isn't, here, but that's a second, independent
// reason not to rely on a recompute-on-load path for those two fields specifically.)
//
// NOTE: this table must stay in sync with rfTransformation's declared fields (see
// the matching note at rftrans.h next to the struct) - adding/removing/resizing a
// field in one place without the other silently desyncs the on-disk format from
// the struct. rftrans_disk_fields_contiguous() below verifies this at compile time
// for every consecutive pair except the two deliberate exclusions above.
//
// Uses DiskField (util/binfile.h) and has external linkage via the
// `extern` declarations in rftrans.h - see the comment there.
constexpr DiskField RFTRANS_DISK_FIELDS[] = {
    { FieldKind::Int32,   offsetof(rfTransformation, id),         1 },
    { FieldKind::Float64, offsetof(rfTransformation, refepoch),   1 },
    { FieldKind::Float64, offsetof(rfTransformation, prm),        sizeof(rfTransformation::prm) / sizeof(rfTransformation::prm[0]) },
    { FieldKind::Float64, offsetof(rfTransformation, prmCvr),     sizeof(rfTransformation::prmCvr) / sizeof(rfTransformation::prmCvr[0]) },
    { FieldKind::Int8,    offsetof(rfTransformation, calcPrm),    sizeof(rfTransformation::calcPrm) / sizeof(rfTransformation::calcPrm[0]) },
    { FieldKind::Int32,   offsetof(rfTransformation, usage),      1 },
    { FieldKind::Int32,   offsetof(rfTransformation, prmId),      sizeof(rfTransformation::prmId) / sizeof(rfTransformation::prmId[0]) },
    { FieldKind::Int32,   offsetof(rfTransformation, origintype), 1 },
    { FieldKind::Int8,    offsetof(rfTransformation, prmUsed),    sizeof(rfTransformation::prmUsed) / sizeof(rfTransformation::prmUsed[0]) },
    { FieldKind::Float64, offsetof(rfTransformation, origin),     sizeof(rfTransformation::origin) / sizeof(rfTransformation::origin[0]) },
    { FieldKind::Float64, offsetof(rfTransformation, trans),      sizeof(rfTransformation::trans) / sizeof(rfTransformation::trans[0]) },
    { FieldKind::Float64, offsetof(rfTransformation, transrate),  sizeof(rfTransformation::transrate) / sizeof(rfTransformation::transrate[0]) },
    { FieldKind::Float64, offsetof(rfTransformation, tmat),        sizeof(rfTransformation::tmat) / sizeof(double) },
    { FieldKind::Float64, offsetof(rfTransformation, invtmat),     sizeof(rfTransformation::invtmat) / sizeof(double) },
    { FieldKind::Float64, offsetof(rfTransformation, tmatrate),    sizeof(rfTransformation::tmatrate) / sizeof(double) },
    { FieldKind::Float64, offsetof(rfTransformation, invtmatrate), sizeof(rfTransformation::invtmatrate) / sizeof(double) },
    { FieldKind::Float64, offsetof(rfTransformation, dtmatdrot),   sizeof(rfTransformation::dtmatdrot) / sizeof(double) },
    { FieldKind::Float64, offsetof(rfTransformation, toporot),     sizeof(rfTransformation::toporot) / sizeof(double) },
    { FieldKind::Float64, offsetof(rfTransformation, invtoporot),  sizeof(rfTransformation::invtoporot) / sizeof(double) },
};
constexpr size_t RFTRANS_DISK_FIELD_COUNT = sizeof(RFTRANS_DISK_FIELDS) / sizeof(RFTRANS_DISK_FIELDS[0]);

// Verifies RFTRANS_DISK_FIELDS has no gap relative to rfTransformation's actual
// memory layout - the same rounded-up-to-next-alignment check bindata.cpp uses for
// survdata - except at the two boundaries that are gaps by design rather than by
// mistake: id -> refepoch (skips `name`, a pointer) and prmId -> origintype (skips
// the 12 bitfields). Those two adjacent pairs are deliberately not checked; every
// other pair is, and the last tracked field (invtoporot) must, by the same rule,
// reach exactly the end of the struct (there's nothing tracked after it).
static constexpr bool rftrans_disk_fields_contiguous()
{
    for( size_t i = 0; i + 1 < RFTRANS_DISK_FIELD_COUNT; ++i )
    {
        const DiskField &field = RFTRANS_DISK_FIELDS[i];
        if( field.offset == offsetof(rfTransformation, id) ) continue;
        if( field.offset == offsetof(rfTransformation, prmId) ) continue;

        const size_t end = field.offset + field_in_memory_size(field.kind) * field.count;
        const size_t expected_next = round_up(end, field_in_memory_alignment(RFTRANS_DISK_FIELDS[i+1].kind));
        if( expected_next != RFTRANS_DISK_FIELDS[i+1].offset ) return false;
    }
    const DiskField &last = RFTRANS_DISK_FIELDS[RFTRANS_DISK_FIELD_COUNT-1];
    const size_t last_end = last.offset + field_in_memory_size(last.kind) * last.count;
    return round_up(last_end, alignof(rfTransformation)) == sizeof(rfTransformation);
}
// Runs entirely at compile time, same as survdata_disk_fields_contiguous() in
// bindata.cpp - costs nothing in the compiled binary either way.
static_assert(rftrans_disk_fields_contiguous(),
    "RFTRANS_DISK_FIELDS has a gap relative to rfTransformation's actual layout, other "
    "than the two known/deliberate exclusions for `name` and the bitfields - a field "
    "was likely added, removed, or reordered in rftrans.h without updating this table");

// Packs/unpacks the 12 bitfields into a single uint16_t disk representation, in
// declared order (istopo = bit 0 through calcscalerate = bit 11). This is the one
// part of the format RFTRANS_DISK_FIELDS can't cover or verify - see the notes
// above the table and at the bitfield declarations in rftrans.h: adding a 13th
// bitfield there gives no compiler error if only one side (pack/unpack) is updated
// to match.
static uint16_t pack_rftrans_flags( const rfTransformation &rf )
{
    return static_cast<uint16_t>(
               (rf.istopo         << 0)  | (rf.isiers         << 1)  |
               (rf.userates       << 2)  | (rf.usetrans       << 3)  |
               (rf.localoriginok  << 4)  | (rf.localorigin    << 5)  |
               (rf.calctrans      << 6)  | (rf.calcrot        << 7)  |
               (rf.calcscale      << 8)  | (rf.calctransrate  << 9)  |
               (rf.calcrotrate    << 10) | (rf.calcscalerate  << 11) );
}
static void unpack_rftrans_flags( const uint16_t flags, rfTransformation &rf )
{
    rf.istopo        = (flags >> 0)  & 1;
    rf.isiers        = (flags >> 1)  & 1;
    rf.userates      = (flags >> 2)  & 1;
    rf.usetrans      = (flags >> 3)  & 1;
    rf.localoriginok = (flags >> 4)  & 1;
    rf.localorigin   = (flags >> 5)  & 1;
    rf.calctrans     = (flags >> 6)  & 1;
    rf.calcrot       = (flags >> 7)  & 1;
    rf.calcscale     = (flags >> 8)  & 1;
    rf.calctransrate = (flags >> 9)  & 1;
    rf.calcrotrate   = (flags >> 10) & 1;
    rf.calcscalerate = (flags >> 11) & 1;
}

// Writes RFTRANS_DISK_FIELDS in table order through the fixed-width disk-cast
// templates from binfile.h, then the packed bitfield uint16_t. Together with
// dump_string(rf->name, ...) (kept as a separate call at the existing call site,
// since name is a pointer and out of scope for this fixed-width table), this
// covers every field of rfTransformation.
static void write_rftrans_fixed_width( const rfTransformation &rf, FILE *f )
{
    for_each_disk_field( rf, RFTRANS_DISK_FIELDS, RFTRANS_DISK_FIELD_COUNT,
        [f]( FieldKind kind, auto value ) { write_disk_field( f, kind, value ); } );
    write_raw(f, pack_rftrans_flags(rf));
}

// Mirrors write_rftrans_fixed_width: same table, same iteration order, reading
// into *reinterpret_cast<...*>(base+field.offset)[i] instead of writing, then
// unpacking the trailing uint16_t back into the 12 named bitfields.
static void read_rftrans_fixed_width( FILE *f, rfTransformation &rf )
{
    char *base = reinterpret_cast<char*>(&rf);
    for( const auto &field : RFTRANS_DISK_FIELDS )
    {
        for( size_t i = 0; i < field.count; ++i )
        {
            switch( field.kind )
            {
            case FieldKind::Int8:    read_raw_as<int8_t>(f, reinterpret_cast<char*>(base+field.offset)[i]); break;
            case FieldKind::Float64: read_raw(f, reinterpret_cast<double*>(base+field.offset)[i]); break;
            default:                 read_raw_as<int32_t>(f, reinterpret_cast<int*>(base+field.offset)[i]); break;
            }
        }
    }
    uint16_t flags;
    read_raw(f, flags);
    unpack_rftrans_flags(flags, rf);
}


void dump_rftransformations( BINARY_FILE *b )
{
    int nrf;
    rfTransformation *rf;
    int irf;

    create_section(b,"RFTRANSFORMATIONS");
    nrf = rftrans_count();

    fwrite( &nrf, sizeof(nrf), 1, b->f );

    /* Dump the reference frame definitions */

    for( irf = 0; irf++ < nrf; )
    {
        rf = rftrans_from_id( irf );
        write_rftrans_fixed_width( *rf, b->f );
        dump_string( rf->name, b->f );
    }

    end_section( b );
}



int reload_rftransformations( BINARY_FILE *b )
{
    int nrf;
    int irf;
    rfTransformation *rf;

    if(find_section(b,"RFTRANSFORMATIONS") != OK ) return MISSING_DATA;

    /* Dump the station definitions */

    if( fread(&nrf, sizeof(nrf), 1, b->f ) != 1 || nrf < 0 ) return INVALID_DATA;

    clear_rftrans_list();
    for( irf = 0; irf++ < nrf;  )
    {
        rf = new_rftrans();
        read_rftrans_fixed_width( b->f, *rf );
        rf->name = reload_string( b->f );
        if( !rf->name ) return INVALID_DATA;
    }
    return check_end_section( b );
}
