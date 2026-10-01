#ifndef _BINFILE_H
#define _BINFILE_H

/*
   $Log: binfile.h,v $
   Revision 1.3  2004/04/22 02:35:23  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  1998/06/03 22:56:43  ccrook
   Added support for binary file version and section versions to facilitate building upwardly
   compatible binary files.

   Revision 1.1  1995/12/22 18:53:26  CHRIS
   Initial revision

*/

/* Define a binary file containing a list of binary sections */

#include <stdint.h>
#include <limits>
#include <stdexcept>
#include <string>

#ifndef _ERRDEF_H
#include "util/errdef.h"
#endif

struct BINARY_FILE
{
    FILE *f;
    int64_t start;
    int64_t section_start;
    int32_t section_version;
    int32_t bf_version;
    char sigchar;
};


enum class BinFileOpenResult { NotFound, InvalidVersion, Ok };

struct BinFileOpenOutcome
{
    BINARY_FILE *file;
    BinFileOpenResult result;
};

/// Creates a new binary file at fname, writing header as its on-disk
/// signature immediately followed by a fixed trailer, then invalidating
/// its first byte until close_binary_file() completes it - see
/// close_binary_file().
/// \return the new file's handle, or nullptr if fname could not be opened
///         for writing.
BINARY_FILE *create_binary_file(
    const std::string &fname,       ///< path of the file to create
    const std::string &header );    ///< the signature identifying this file's format/version, e.g. "SNAP binary file version 3.0"

/// Opens an existing binary file at fname for reading, verifying its
/// stored signature matches header exactly, including the fixed trailer
/// create_binary_file() wrote after it.
/// \return the opened file and why it succeeded or failed - see
///         BinFileOpenResult.
BinFileOpenOutcome open_binary_file(
    const std::string &fname,       ///< path of the file to open
    const std::string &header );    ///< the expected signature, e.g. "SNAP binary file version 3.0"

/// Finalizes and closes bin, completing its signature (see
/// create_binary_file()) if it was opened for writing, then frees bin -
/// bin must not be used again after this call.
void close_binary_file( BINARY_FILE *bin );

/// Starts a new named section in bin, ending whichever section (if any)
/// is currently open first (see end_section()). Records version as the
/// section's own version, if bin's own format version supports
/// per-section versioning (bin->bf_version > 0).
void create_section_ex(
    BINARY_FILE *bin,
    const std::string &section,     ///< the section's name, e.g. "STATION_COVARIANCES"
    long version );                 ///< this section's version, ignored if bin->bf_version is 0

/// A thin wrapper over create_section_ex() for the common case of an
/// unversioned section (version 0).
void create_section(
    BINARY_FILE *bin,
    const std::string &section );   ///< the section's name, e.g. "STATION_COVARIANCES"

/// Ends whichever section is currently open in bin, if any - writes the
/// end-of-section marker and back-patches the section's recorded length.
/// A no-op if no section is currently open (e.g. bin was only opened for
/// reading).
void end_section( BINARY_FILE *bin );

/// Searches bin for a section named section, leaving the file positioned
/// to read its content (and bin->section_version updated) if found.
/// \return OK if found, MISSING_DATA otherwise.
int find_section(
    BINARY_FILE *bin,
    const std::string &section );   ///< the section's name to search for, e.g. "STATION_COVARIANCES"

/// Checks whether bin's current file position is immediately followed by
/// the end-of-section marker create_section_ex()/end_section() write -
/// consumes it if so, otherwise rewinds bin back to its original position.
/// \return OK if the marker was found, INVALID_DATA otherwise.
int check_end_section( BINARY_FILE *bin );

template<class T> inline void write_raw( FILE *f, const T &x ) { fwrite(&x, sizeof(T), 1, f); }
template<class T> inline void read_raw( FILE *f, T &x )        { fread(&x, sizeof(T), 1, f); }

template<class Disk, class T> inline void write_raw_as( FILE *f, const T &x )
{ const Disk d = static_cast<Disk>(x); write_raw(f, d); }
template<class Disk, class T> inline void read_raw_as( FILE *f, T &x )
{ Disk d; read_raw(f, d); x = static_cast<T>(d); }

// These BINARY_FILE-taking wrappers are a thin `b->f` unwrap around the FILE*-based
// functions above, nothing more - BINARY_FILE (above) is a plain C struct with public
// members, so this isn't encapsulation/access-control (any call site could reach into
// ->f directly, same as the DUMP_BIN/RELOAD_BIN macros they replace already did). The
// actual trade-off is just where the ->f unwrap happens: once here, vs repeated at
// every call site. Kept for now for that convenience and to match the existing
// call-site vocabulary (create_section(b, ...), etc.) - not because inlining
// write_raw_as<Disk>(b->f, x) at call sites would be wrong.
template<class T> inline void dump_bin( BINARY_FILE *b, const T &x )   { write_raw(b->f, x); }
template<class T> inline void reload_bin( BINARY_FILE *b, T &x )       { read_raw(b->f, x); }

template<class Disk, class T> inline void dump_bin_fixed( BINARY_FILE *b, const T &x )   { write_raw_as<Disk>(b->f, x); }
template<class Disk, class T> inline void reload_bin_fixed( BINARY_FILE *b, T &x )       { read_raw_as<Disk>(b->f, x); }

// long -> int32_t is the one narrowing in this file format where the source type
// (8 bytes on Linux/LP64) genuinely has more range than the disk representation
// (4 bytes) - every other write_raw_as<Disk> use elsewhere is a same-width
// reinterpretation (int/unsigned int/unsigned char are already 4/4/1 bytes on
// every target here) where a value can never fail to fit. Kept as a dedicated,
// non-template function rather than a generic checked write_raw_as, because a
// generic numeric_limits<Disk> bounds check would misfire on the same-width,
// different-signedness cases elsewhere (e.g. an unsigned char written as int8_t -
// a legitimate value like 200 is in-range for the source type but out of int8_t's
// signed range, even though it round-trips correctly via modular truncation).
// Throws std::overflow_error if x doesn't fit in int32_t, rather than silently
// truncating it - a caller passing a genuinely >4-byte value wants to find out,
// not get a corrupt .bin file.
inline void write_raw_long32( FILE *f, const long x )
{
    if (x < std::numeric_limits<int32_t>::min() || x > std::numeric_limits<int32_t>::max())
        throw std::overflow_error("value exceeds int32_t range while writing .bin file");
    write_raw_as<int32_t>(f, x);
}
inline void read_raw_long32( FILE *f, long &x )
{
    read_raw_as<int32_t>(f, x);   // int32_t -> long always widens safely, no check needed
}

inline void dump_bin_long32( BINARY_FILE *b, const long x )   { write_raw_long32(b->f, x); }
inline void reload_bin_long32( BINARY_FILE *b, long &x )      { read_raw_long32(b->f, x); }

// Shared vocabulary for the table-driven fixed-width dumps of survdata,
// rfTransformation, station, and param. Each struct's own table, layout-
// contiguity check, and any per-struct extras (a trailing bitfield pack, a
// struct's own excluded pointer fields) stay separate - those genuinely vary
// by struct. What doesn't vary is the disk-width/alignment of each kind, how
// to round an offset up to the next field's alignment, and walking a table to
// fetch each field's current value - shared here (FieldKind/round_up below,
// DiskField/for_each_disk_field further down) so those copies can't drift
// from each other, and so any caller that needs to walk the same fields for a
// different purpose (e.g. a text dump for diffing) reuses the exact same
// table instead of re-listing fields by hand.
enum class FieldKind { Int32, UInt32, Int8, UInt8, Float64 };

// In-memory size of the field's declared type. Fixed-width and identical across
// every target compiler here, so this doubles as each kind's on-disk width too.
inline constexpr size_t field_in_memory_size(const FieldKind kind)
{
    switch (kind)
    {
    case FieldKind::Float64: return sizeof(double);
    case FieldKind::Int8:
    case FieldKind::UInt8:   return sizeof(int8_t);
    default:                 return sizeof(int32_t);
    }
}
// Required alignment of the field's declared type - used to predict where a
// compiler will place the *next* struct member, including any padding.
inline constexpr size_t field_in_memory_alignment(const FieldKind kind)
{
    switch (kind)
    {
    case FieldKind::Float64: return alignof(double);
    case FieldKind::Int8:
    case FieldKind::UInt8:   return alignof(int8_t);
    default:                 return alignof(int32_t);
    }
}
// Rounds `value` up to the next multiple of `alignment` - the same rule a
// compiler uses to place a struct member after the one before it.
inline constexpr size_t round_up(const size_t value, const size_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

// A single table entry: `count` scalars of `kind`, starting at `offset` bytes
// into the struct. Every table-driven struct's own field-table type (station's
// StationDiskField, rfTransformation's RfTransDiskField, param's
// ParamDiskField, ...) has always had exactly this shape - unified here so
// for_each_disk_field (below) can walk any of them the same way.
struct DiskField { FieldKind kind; size_t offset; size_t count; };

// Calls `action(kind, value)` once per scalar element declared by `table`,
// with `value` read from `base`'s memory as that field's real in-memory type
// (char/unsigned char/unsigned int/double/int). This is the part of a
// table-driven dump that never varies by struct; what happens with each
// value - write raw bytes, print as text, anything else - is decided
// entirely by `action`, so a second caller can walk the exact same table for
// a different purpose without re-listing fields by hand.
//
// `action` is any callable shaped `action(FieldKind, value)` - in practice a
// generic lambda (`[]( FieldKind kind, auto value ) {...}`):
// - `Action` is a template parameter, not a function pointer or
//   std::function, so the compiler sees the concrete lambda at each call
//   site and inlines it straight into this loop - no indirect call, no
//   std::function heap allocation.
// - The lambda's own `auto value` parameter makes its call operator a
//   template too, so one lambda body handles every FieldKind's differently-
//   typed value (char/unsigned char/unsigned int/double/int) without the
//   caller writing five overloads or type-erasing to void* - write_disk_field
//   (below) is exactly one such lambda body, reused across every kind.
// - A lambda captures exactly the state its caller needs (a FILE* to write
//   to, an ostream to print to) with no separate named functor struct to
//   declare just to carry that one piece of state.
// Put together, this is why the same table can drive two unrelated outputs -
// e.g. binary bytes via write_disk_field's action and human-readable text
// via a distinct action defined elsewhere - just by passing a different
// lambda in; for_each_disk_field's own loop and switch never change.
//
// `table`/`table_size` stay a pointer+size pair (rather than a bound array
// reference) because the real table is defined once, in one translation
// unit, next to its own layout-contiguity check - a caller elsewhere sees it
// only through an `extern` declaration with no compile-time-visible bound.
template<class T, class Action>
inline void for_each_disk_field( const T &base, const DiskField *table, size_t table_size, Action action )
{
    const char *b = reinterpret_cast<const char*>(&base);
    for( size_t t = 0; t < table_size; ++t )
    {
        const DiskField &field = table[t];
        for( size_t i = 0; i < field.count; ++i )
        {
            switch( field.kind )
            {
            case FieldKind::Int8:    action( field.kind, reinterpret_cast<const char*>(b+field.offset)[i] ); break;
            case FieldKind::UInt8:   action( field.kind, reinterpret_cast<const unsigned char*>(b+field.offset)[i] ); break;
            case FieldKind::UInt32:  action( field.kind, reinterpret_cast<const unsigned int*>(b+field.offset)[i] ); break;
            case FieldKind::Float64: action( field.kind, reinterpret_cast<const double*>(b+field.offset)[i] ); break;
            default:                 action( field.kind, reinterpret_cast<const int*>(b+field.offset)[i] ); break;
            }
        }
    }
}

// The binary-write action for_each_disk_field callers use: pins the on-disk
// width by `kind`, same as every write_*_fixed_width used to do independently
// in its own repeated switch.
template<class T>
inline void write_disk_field( FILE *f, FieldKind kind, const T &value )
{
    switch( kind )
    {
    case FieldKind::Int8:    write_raw_as<int8_t>(f, value); break;
    case FieldKind::UInt8:   write_raw_as<uint8_t>(f, value); break;
    case FieldKind::UInt32:  write_raw_as<uint32_t>(f, value); break;
    case FieldKind::Float64: write_raw(f, value); break;
    default:                 write_raw_as<int32_t>(f, value); break;
    }
}

// Mutable counterpart to for_each_disk_field: calls action(kind, ref) once
// per scalar element declared by `table`, with `ref` a reference straight
// into `base`'s memory, so an action can write into it (read_disk_field,
// below) instead of only observing it. Same table, same iteration order, so
// a read_*_fixed_width built on this stays paired with its
// write_*_fixed_width automatically.
template<class T, class Action>
inline void for_each_disk_field_mutable( T &base, const DiskField *table, size_t table_size, Action action )
{
    char *b = reinterpret_cast<char*>(&base);
    for( size_t t = 0; t < table_size; ++t )
    {
        const DiskField &field = table[t];
        for( size_t i = 0; i < field.count; ++i )
        {
            switch( field.kind )
            {
            case FieldKind::Int8:    action( field.kind, reinterpret_cast<char*>(b+field.offset)[i] ); break;
            case FieldKind::UInt8:   action( field.kind, reinterpret_cast<unsigned char*>(b+field.offset)[i] ); break;
            case FieldKind::UInt32:  action( field.kind, reinterpret_cast<unsigned int*>(b+field.offset)[i] ); break;
            case FieldKind::Float64: action( field.kind, reinterpret_cast<double*>(b+field.offset)[i] ); break;
            default:                 action( field.kind, reinterpret_cast<int*>(b+field.offset)[i] ); break;
            }
        }
    }
}

// The binary-read action for_each_disk_field_mutable callers use: mirrors
// write_disk_field's on-disk width by `kind`.
template<class T>
inline void read_disk_field( FILE *f, FieldKind kind, T &value )
{
    switch( kind )
    {
    case FieldKind::Int8:    read_raw_as<int8_t>(f, value); break;
    case FieldKind::UInt8:   read_raw_as<uint8_t>(f, value); break;
    case FieldKind::UInt32:  read_raw_as<uint32_t>(f, value); break;
    case FieldKind::Float64: read_raw(f, value); break;
    default:                 read_raw_as<int32_t>(f, value); break;
    }
}

#endif
