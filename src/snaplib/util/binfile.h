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

#ifndef _ERRDEF_H
#include "util/errdef.h"
#endif

typedef struct
{
    FILE *f;
    int64_t start;
    int64_t section_start;
    int32_t section_version;
    int32_t bf_version;
    char sigchar;
} BINARY_FILE;


BINARY_FILE *create_binary_file( char *fname, const char *header );
BINARY_FILE *open_binary_file( char *fname, const char *header );
void close_binary_file( BINARY_FILE *bin );
void create_section_ex( BINARY_FILE *bin, const char *section, long version );
void create_section( BINARY_FILE *bin, const char *section );
void end_section( BINARY_FILE *bin );
int find_section( BINARY_FILE *bin, const char *section );
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
// rfTransformation, and param (bindata.cpp/rftrndmp.cpp/genparam.cpp). Each
// struct's own table type, write/read functions, and layout-contiguity check
// stay separate - rfTransformation needs an extra per-entry array count and a
// manual bitfield pack the other two don't, so a single shared table/loop would
// force that complexity onto structs that don't need it. What genuinely doesn't
// vary by struct is the disk-width/alignment of each kind and how to round an
// offset up to the next field's alignment - shared here so three copies of the
// same arithmetic can't drift from each other.
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

#endif
