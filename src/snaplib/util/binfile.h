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

#ifndef BINFILE_H_RCSID
#define BINFILE_H_RCSID "$Id: binfile.h,v 1.3 2004/04/22 02:35:23 ccrook Exp $"
#endif

/* Define a binary file containing a list of binary sections */

#ifndef _ERRDEF_H
#include "util/errdef.h"
#endif

typedef struct
{
    FILE *f;
    long bf_version;
    long start;
    long section_start;
    long section_version;
    char sigchar;
} BINARY_FILE;


BINARY_FILE *create_binary_file( char *fname, char *header );
BINARY_FILE *open_binary_file( char *fname, char *header );
void close_binary_file( BINARY_FILE *bin );
void create_section_ex( BINARY_FILE *bin, char *section, long version );
void create_section( BINARY_FILE *bin, char *section );
void end_section( BINARY_FILE *bin );
int find_section( BINARY_FILE *bin, char *section );
int check_end_section( BINARY_FILE *bin );

#define DUMP_BIN(x,b)   fwrite(&x,sizeof(x),1,b->f)
#define RELOAD_BIN(x,b) fread(&x,sizeof(x),1,b->f)

#endif
