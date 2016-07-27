#ifndef _BINDATA_H
#define _BINDATA_H

/*
   $Log: bindata.h,v $
   Revision 1.1  1995/12/22 17:39:32  CHRIS
   Initial revision

*/

#ifndef BINDATA_H_RCSID
#define BINDATA_H_RCSID "$Id: bindata.h,v 1.1 1995/12/22 17:39:32 CHRIS Exp $"
#endif

/* The following structure holds indexes data types */

#include <stdint.h>

typedef struct
{
    int64_t loc;      /* Location of structure on the file */
    long size;      /* The size of the data element */
    long allocsize; /* The space allocated */
    int  bintype;  /* The binary data format - see enum below */
    void *data; /* Pointer to the data structure */
} bindata;


/* Types of binary data format */

enum { SURVDATA,       /* Survey data */
       NOTEDATA,       /* Note to be copied to output file */
       ENDDATA,        /* Marks the end of data in the file */
       NOBINDATATYPES
     };


int init_bindata( FILE *f  ) ;
void end_bindata( void );
int64_t write_bindata_header( long size, int type );
int read_bindata_header( long *size, int *type );

void init_get_bindata(int64_t loc );
int get_bindata( bindata *b );
void update_bindata( bindata *b );

bindata *create_bindata( void );
void delete_bindata( bindata *b );

long save_survdata( survdata *sd );
long save_survdata_subset( survdata *sd, int iobs, int type );

#ifdef BINDATA_C
#define SCOPE
#else
#define SCOPE extern
#endif

SCOPE FILE *bindata_file;
SCOPE long nbindata;

#undef SCOPE

#endif


