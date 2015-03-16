#include "snapconfig.h"
/* Routines for managing memory allocation - provides a degree of
   debugging of memory errors, and also exception handling (so to speak)
   for failed memory requests.  Calls handle_error with a fatal error
   status.  This should ensure that the program stops!

   Requires stdio.h or something similar to define size_t.

   Extended version of check alloc.  Adds a linked list of memory handles
   to allow error reporting of file and line numbers and listing of unfreed
   memory. */

/*
   $Log: chkallcx.c,v $
   Revision 1.2  2004/04/22 02:35:23  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:53:45  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CHECK_MEMORY_ALLOCATIONS
#undef CHECK_MEMORY_ALLOCATIONS
#endif

#include "util/chkalloc.h"
#include "util/errdef.h"

#define MAGIC_NUMBER 0xA55A

typedef struct MemFile_s
{
    struct MemFile_s *next;
    char *fname;
    int count;
} MemFile;

typedef struct MemHandle_s
{
    unsigned magic;
    struct MemHandle_s *next;
    struct MemHandle_s *prev;
    MemFile *file;
    int line;
    size_t size;
    long id;
} MemHandle;

static MemFile *filelist = NULL;
static MemHandle *memlist = NULL;
static long memId = 0;
static long chk_nalloc = 0;
static long chk_nfree = 0;

static void add_mem_handle( MemHandle *mh )
{
    mh->next = memlist;
    mh->prev = NULL;
    memlist = mh;
    if( mh->next ) mh->next->prev = mh;
}

static void remove_mem_handle( MemHandle *mh )
{
    if( mh->next ) mh->next->prev = mh->prev;
    if( mh->prev )
    {
        mh->prev->next = mh->next;
    }
    else
    {
        memlist = mh->next;
    }
}

static MemFile *get_mem_file( char *fname )
{
    MemFile *mf;
    for( mf = filelist; mf; mf = mf->next )
    {
        if( strcmp(fname,mf->fname) == 0 )
        {
            mf->count++;
            return mf;
        }
    }
    mf = (MemFile *) malloc( sizeof(MemFile) + strlen(fname) + 1 );
    if( !mf ) return NULL;
    mf->fname = ((char *) mf) + sizeof( MemFile );
    strcpy( mf->fname, fname );
    mf->count = 1;
    mf->next = filelist;
    filelist = mf;
    return mf;
}

static void release_mem_file( MemFile *mf )
{
    if( !mf ) return;
    mf->count--;
    if( !mf->count )
    {
        MemFile **mfp;
        for( mfp = &filelist; *mfp; mfp = & (*mfp)->next )
        {
            if( *mfp == mf )
            {
                *mfp = mf->next;
                break;
            }
        }
        free( mf );
    }
}


static void report_error( int sts, char *msg, char *file, int line )
{
    char errmess[80];
    sprintf(errmess,"File %.60s: line %.4d",file ? file : "Unknown",line);
    handle_error( sts,msg,errmess);
}

static MemHandle *get_mem_handle( void *ptr, char *file, int line )
{
    MemHandle *mh;
    mh = (MemHandle *)( (char *)ptr - sizeof(MemHandle) );
    if( mh->magic != MAGIC_NUMBER )
    {
        report_error(INTERNAL_ERROR,"Bad magic number in memory handle",
                     file, line );
    }
    return mh;
}

#define MEM_FROM_HANDLE(mh) ((void *)((char *)mh + sizeof(MemHandle)))

void *check_malloc_x( size_t size, char *file, int line )
{
    MemHandle *mh;

    mh = (MemHandle *) malloc( size + sizeof(MemHandle) );
    if( mh == NULL )
    {
        handle_error( MEM_ALLOC_ERROR, NULL, NULL );
        return NULL;
    }

    mh->file = get_mem_file( file );
    mh->line = line;
    mh->size = size;
    mh->magic = MAGIC_NUMBER;
    mh->id = memId;
    memId++;
    add_mem_handle( mh );
    chk_nalloc++;
    return MEM_FROM_HANDLE( mh );
}


void *check_realloc_x( void *ptr, size_t size, char *file, int line )
{
    void *mem;
    MemHandle *mh;

    mem = check_malloc_x( size, file, line );
    if( !mem ) return NULL;
    if( !ptr ) return mem;

    mh = get_mem_handle( ptr, file, line );
    if( mh->size < size ) size = mh->size;
    if( size ) memcpy( mem, ptr, size );

    check_free_x( ptr, file, line );
    return mem;
}


void check_free_x( void *ptr, char *file, int line )
{
    MemHandle *mh;
    mh = get_mem_handle( ptr, file, line );
    mh->magic = 0L;
    release_mem_file( mh->file );
    remove_mem_handle( mh );
    free(mh);
    chk_nfree++;
}

long check_memory_allocation_counts_x( void )
{
    return chk_nalloc - chk_nfree;
}

void list_memory_allocations_x( FILE *out )
{
    MemHandle *mh;
    if( memlist )
    {
        fprintf(out,"\nOutstanding memory allocations\n");
        fprintf(out," Id   size   line   file\n");
        for( mh = memlist; mh; mh = mh->next )
        {
            fprintf(out,"%4ld  %5ld   %4d   %s\n",
                    mh->id, (long) mh->size, mh->line,
                    mh->file ? mh->file->fname : "Unknown" );
        }
        fprintf(out,"\n");
    }
}
