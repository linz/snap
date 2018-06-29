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

#define DUMP_BIN(x,b)   fwrite(&x,sizeof(x),1,b->f)
#define RELOAD_BIN(x,b) fread(&x,sizeof(x),1,b->f)

#define DUMP_BINI16(x,b)   { int16_t dumpval=(int16_t) x; DUMP_BIN(dumpval,b); }
#define DUMP_BINI32(x,b)   { int32_t dumpval=(int32_t) x; DUMP_BIN(dumpval,b); }
#define DUMP_BINI64(x,b)   { int64_t dumpval=(int64_t) x; DUMP_BIN(dumpval,b); }

#define RELOAD_BINI16(x,b)   { int16_t dumpval; RELOAD_BIN(dumpval,b); x=dumpval; }
#define RELOAD_BINI32(x,b)   { int32_t dumpval; RELOAD_BIN(dumpval,b); x=dumpval; }
#define RELOAD_BINI64(x,b)   { int64_t dumpval; RELOAD_BIN(dumpval,b); x=dumpval; }
#endif
