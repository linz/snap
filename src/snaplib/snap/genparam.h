#ifndef _GENPARAM_H
#define _GENPARAM_H

/*
   $Log: genparam.h,v $
   Revision 1.1  1995/12/22 17:44:34  CHRIS
   Initial revision

*/

#ifndef _BINFILE_H
#include "util/binfile.h"
#endif

// hash..identical (everything but `name`) are dumped to the .bin file via a
// fixed-width table, PARAM_DISK_FIELDS in genparam.cpp - adding, removing, or
// resizing a field here without updating that table silently desyncs the
// on-disk format from the struct. genparam.cpp's compile-time
// param_disk_fields_contiguous() check catches most such drift, but can't catch
// a field added and never added to the table at all.
typedef struct param_s
{
    char *name;
    unsigned int hash;
    double value;
    double covar;
    int rowno;
    unsigned char flags;
    int identical;
} param;

// The fixed-width on-disk layout of every field above except `name` (a
// pointer) - see genparam.cpp, where this table is defined and checked at
// compile time against param's actual memory layout. Exposed here, rather
// than kept file-local, so a caller elsewhere can walk the same fields via
// for_each_disk_field (util/binfile.h) without re-listing them by hand.
// `extern` (plain C++ external linkage, unrelated to `extern "C"`) is
// required because a `static` array at file scope is only visible within
// its own translation unit - this declares "a definition exists
// elsewhere," letting genparam.cpp's one real array be linked from here.
extern const DiskField PARAM_DISK_FIELDS[];
extern const size_t PARAM_DISK_FIELD_COUNT;

#define PRM_ADJUST 0x01  /* Flags that parameter is to be adjusted */
#define PRM_USED   0x02  /* Flags that data have been used... */
#define PRM_LISTED 0x04  /* Flags that parameters have already been listed */
#define PRM_ROWSET 0x04  /* Flags that the parameter rownumber is set by other code */

/* Note: the numbers defined for PRM_ correspond to indexes in the
   coefprefix and coefpfxlen arrays in genparam.c */

#define PRM_REFCOEF 0
#define PRM_DISTSF  1
#define PRM_BRNGREF 2
#define PRM_SYSERR  3

#define DEFAULT_REFCOEF 0.075

void set_default_refcoef( double value );

int define_param( const char *name, double value, int adjust );
int find_param( const char *name );
void   flag_param_used( int p);
void   flag_param_listed( int p );
double param_value( int p );
void update_param_value( int p, double v, double var );
const char *param_name( int p );
int param_rowno( int p );
int identical_param( int p );
int param_count( void );
param * param_from_id( int pid );
int sorted_param_id( int n );

void define_param_value( int p, double value, int adjust );
void wildcard_param_value( char *name, double value, int adjust );
void define_param_match( int p1, int p2 );
void wildcard_param_match( char *name, int p );

int init_param_rowno( int nextprm );
int find_param_row( int row, char *name, int nlen );

void clear_param_list( void );
void dump_parameters( BINARY_FILE *b );
int reload_parameters( BINARY_FILE *b );

int get_param( int type, const char *name, int create );
const char *param_type_name( int type, int pid );
void configure_param( int type, const char *refcoef, double value, int adjust );
void configure_param_match( int type, const char *coef1, const char *coef2 );

#endif
