#include "snapconfig.h"
/*
   $Log: netstns1.c,v $
   Revision 1.1  1995/12/22 17:31:54  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstddef>

#include "network/network.h"
#include "network/stnoffset.h"
#include "util/dstring.h"
#include "util/chkalloc.h"
#include "util/binfile.h"

// Single source of truth for the fixed-width on-disk station layout.
// Excludes the four trailing pointers: classval, Name, ts, hook.
// Each is already handled separately below. classval is a raw int
// array sized by nclass. Name goes through dump_string/reload_string.
// ts goes through dump_station_offset/reload_station_offset. hook is
// a void pointer to scratch space defined at runtime, so there's
// nothing meaningful to write for it - it's never serialized at all.
//
// All four pointers sit contiguously at the end of the struct, after
// every field tracked here. Unlike rftrndmp.cpp's table, there's no
// interior gap to skip when checking contiguity below.
//
// station.id is genuinely read from disk and relied on. It isn't just
// recomputed. reload_station_list (netlist1.cpp) uses it to preserve
// gaps in station numbering across a reload, via sl_add_station_at_id
// (netlist.cpp). So it's a normal tracked field here, not something to
// special-case or omit.
//
// rTopo/rGrav are `rotmat` (util/geodetic.h): a plain 4-double struct
// with no padding (cslt, snlt, csln, snln, all doubles, nothing else).
// Like rftrndmp.cpp's tmat/invtmat, it's safe to treat as a flat
// Float64 array via sizeof(rotmat)/sizeof(double).
//
// Code gets Int8, matching plain `char` (signed by default on every
// compiler this project targets). rftrndmp.cpp makes the same choice
// for calcPrm/prmUsed, for the same reason: matching the in-memory
// type's signedness means neither direction ever narrows. Contrast
// hash/flags in genparam.cpp, which are genuinely unsigned in memory,
// so get UInt32/UInt8 instead.
//
// This table must stay in sync with station's declared fields. See the
// matching note at network.h next to the struct. Adding, removing, or
// resizing a field in one place without the other silently desyncs the
// on-disk format from the struct.
// station_disk_fields_contiguous() below verifies this at compile time.
//
// Uses DiskField (util/binfile.h) and has external linkage via the
// `extern` declarations in network.h - see the comment there.
constexpr DiskField STATION_DISK_FIELDS[] = {
    { FieldKind::Int8,    offsetof(station, Code),  sizeof(station::Code) / sizeof(station::Code[0]) },
    { FieldKind::Int32,   offsetof(station, id),    1 },
    { FieldKind::Float64, offsetof(station, ELat),  1 },
    { FieldKind::Float64, offsetof(station, ELon),  1 },
    { FieldKind::Float64, offsetof(station, OHgt),  1 },
    { FieldKind::Float64, offsetof(station, GXi),   1 },
    { FieldKind::Float64, offsetof(station, GEta),  1 },
    { FieldKind::Float64, offsetof(station, GUnd),  1 },
    { FieldKind::Float64, offsetof(station, XYZ),   sizeof(station::XYZ) / sizeof(double) },
    { FieldKind::Float64, offsetof(station, rTopo), sizeof(station::rTopo) / sizeof(double) },
    { FieldKind::Float64, offsetof(station, rGrav), sizeof(station::rGrav) / sizeof(double) },
    { FieldKind::Float64, offsetof(station, dNdLt), 1 },
    { FieldKind::Float64, offsetof(station, dEdLn), 1 },
    { FieldKind::Int32,   offsetof(station, nclass), 1 },
};
constexpr size_t STATION_DISK_FIELD_COUNT = sizeof(STATION_DISK_FIELDS) / sizeof(STATION_DISK_FIELDS[0]);

// Verifies STATION_DISK_FIELDS has no gap relative to station's actual
// memory layout. Uses the same rounded-up-to-next-alignment check as
// survdata_disk_fields_contiguous() in bindata.cpp. Every consecutive
// pair in this table is checked - there's no interior exclusion to
// skip, unlike rftrndmp.cpp's table. The last tracked field (nclass)
// must, by the same rule, be immediately followed by the first
// excluded member (classval).
static constexpr bool station_disk_fields_contiguous()
{
    for( size_t i = 0; i + 1 < STATION_DISK_FIELD_COUNT; ++i )
    {
        const DiskField &field = STATION_DISK_FIELDS[i];
        const size_t end = field.offset + field_in_memory_size(field.kind) * field.count;
        const size_t expected_next = round_up(end, field_in_memory_alignment(STATION_DISK_FIELDS[i+1].kind));
        if( expected_next != STATION_DISK_FIELDS[i+1].offset ) return false;
    }
    const DiskField &last = STATION_DISK_FIELDS[STATION_DISK_FIELD_COUNT-1];
    const size_t last_end = last.offset + field_in_memory_size(last.kind) * last.count;
    const size_t expected_classval = round_up(last_end, alignof(decltype(station::classval)));
    return expected_classval == offsetof(station, classval);
}
// Runs entirely at compile time, same as survdata_disk_fields_contiguous()
// in bindata.cpp. Costs nothing in the compiled binary either way.
static_assert(station_disk_fields_contiguous(),
    "STATION_DISK_FIELDS has a gap relative to station's actual layout - a field "
    "was likely added, removed, or reordered in network.h without updating this table");

// Writes STATION_DISK_FIELDS in table order through the fixed-width disk-cast
// templates from binfile.h. classval/Name/ts/hook are handled separately
// (see the table comment above) - together, this covers every field of
// station.
static void write_station_fixed_width( const station &st, FILE *f )
{
    for_each_disk_field( st, STATION_DISK_FIELDS, STATION_DISK_FIELD_COUNT,
        [f]( FieldKind kind, auto value ) { write_disk_field( f, kind, value ); } );
}

// Mirrors write_station_fixed_width. Same table, same iteration order,
// reading into each field in place instead of writing.
static void read_station_fixed_width( FILE *f, station &st )
{
    for_each_disk_field_mutable( st, STATION_DISK_FIELDS, STATION_DISK_FIELD_COUNT,
        [f]( FieldKind kind, auto &value ) { read_disk_field( f, kind, value ); } );
}

/* Procedure to dump the station coordinates to a binary file */
/* Flags are dumped separately to improve binary file compatibility
   between different compilers */


// Writes the data st->ts points to (a stn_offset, cast from void*).
// The pointer itself is never written. On reload, reload_station_offset
// rebuilds a fresh stn_offset and points st->ts at that instead.
static void dump_station_offset( station *st, FILE *f )
{
    stn_offset *sto=(stn_offset *)(st->ts);
    int ncomp=0;
    if( sto )
    {
        for( stn_offset_comp *comp=sto->components; comp; comp=comp->next ) ncomp++;
    }
    fwrite( &ncomp, sizeof(int), 1, f );
    if( ! ncomp ) return;
    fwrite( &(sto->isdeformation), sizeof(int), 1, f );
    for( stn_offset_comp *comp=sto->components; comp; comp=comp->next )
    {
        fwrite(&(comp->mode),sizeof(int),1,f);
        fwrite(&(comp->isxyz),sizeof(int),1,f);
        fwrite(&(comp->ntspoints),sizeof(int),1,f);
        fwrite(&(comp->basepoint),sizeof(stn_tspoint),1,f);
        if( comp->ntspoints )
        {
            fwrite(comp->tspoints,sizeof(stn_tspoint),comp->ntspoints,f);
        }
    }
}

// Mirrors dump_station_offset: reads the same data back and rebuilds
// a fresh stn_offset, then points st->ts at it via
// add_stn_offset_comp_to_station below.
static void reload_station_offset( station *st, FILE *f )
{
    int ncomp;
    int isdef;
    fread( &ncomp, sizeof(int), 1, f );
    if( ncomp == 0 ) return;
    fread( &isdef, sizeof(int), 1, f );
    while( ncomp-- )
    {
        int mode, isxyz, ntspoints;
        stn_offset_comp *sto;
        fread(&mode,sizeof(int),1,f);
        fread(&isxyz,sizeof(int),1,f);
        fread(&ntspoints,sizeof(int),1,f);
        sto=create_stn_offset_comp(mode,isxyz,ntspoints);
        fread(&(sto->basepoint),sizeof(stn_tspoint),1,f);
        if( ntspoints > 0 )
        {
            fread(sto->tspoints,sizeof(stn_tspoint),ntspoints,f);
        }
        add_stn_offset_comp_to_station( st, sto, isdef );
    }
}

void dump_station( station *st, FILE *f )
{
    write_station_fixed_width( *st, f );
    if( st->nclass > 0 ) fwrite( st->classval, sizeof(int), st->nclass, f );
    dump_station_offset( st, f );  // handle ts
    dump_string( st->Name, f );
}

station *reload_station( FILE *f )
{
    station *st;
    int nclass;
    st = new_station();
    read_station_fixed_width( f, *st );
    nclass = st->nclass;
    if( nclass > 0 )
    {
        st->nclass = 0;
        st->classval = 0;
        init_station_classes( st, nclass );
        fread( st->classval, sizeof(int), nclass,f );
    }
    reload_station_offset( st, f );  // reconstruct ts
    st->Name = reload_string( f );
    return st;
}

