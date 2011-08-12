#include "snapconfig.h"
/* Routines for managing memory allocation - provides a degree of
   debugging of memory errors, and also exception handling (so to speak)
   for failed memory requests.  Calls handle_error with a fatal error
   status.  This should ensure that the program stops!

   Requires stdio.h or something similar to define size_t.

   If CHECK_ALLOC is defined, and also handles some memory allocation errors */

/*
   $Log: chkalloc.c,v $
   Revision 1.2  2004/04/22 02:35:23  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:54:37  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>

#ifdef CHECK_MEMORY_ALLOCATIONS
#undef CHECK_MEMORY_ALLOCATIONS
#endif

#include "util/chkalloc.h"
#include "util/errdef.h"

#ifdef CHECK_ALLOC
#define MAGIC_NUMBER 0xA55A
#define MAGIC_SPACE (sizeof(unsigned))
#define OFFSET_SIZE( size )  {size += MAGIC_SPACE;}
#define OFFSET_PTR( ptr ) { *(unsigned *)ptr = MAGIC_NUMBER; \
static char rcsid[]="$Id: chkalloc.c,v 1.2 2004/04/22 02:35:23 ccrook Exp $";

ptr = (void *)((char *)ptr + MAGIC_SPACE);
}
#define RESET_PTR( ptr ) { check_magic_number( ptr ); \
			   ptr = (void *)((char *)ptr - MAGIC_SPACE); \
			   *(unsigned *)ptr = 0; \
			   }

#define INCREMENT_COUNT(x) x++
long chk_nalloc = 0;
                  long chk_nfree = 0;
#else
#define OFFSET_SIZE( size )
#define OFFSET_PTR( ptr )
#define RESET_PTR( ptr )
#define INCREMENT_COUNT( x )
#endif

#ifdef CHECK_ALLOC

                                   static void check_magic_number( void *ptr )
{
    unsigned check;
    check = *(unsigned *)((char *)ptr - MAGIC_SPACE);
    if( check == MAGIC_NUMBER ) return;
    handle_error( INTERNAL_ERROR,"Bad magic number on memory block",NO_MESSAGE);
}
#endif

void *check_malloc( size_t size )
{
    void *mem;

    OFFSET_SIZE( size );

    mem = malloc( size );
    if( mem == NULL )
    {
        handle_error( MEM_ALLOC_ERROR, NULL, NULL );
    }

    OFFSET_PTR( mem );
    INCREMENT_COUNT( chk_nalloc );
    return mem;
}

void *check_realloc( void *ptr, size_t size )
{
    void *mem;

#ifdef CHECKREALLOC
    unsigned long check;
    if( ptr ) check = * (unsigned long *)ptr;
#endif
    if( !ptr ) return check_malloc( size );

    RESET_PTR( ptr );
    OFFSET_SIZE( size );
    mem = realloc( ptr, size );
    if( mem == NULL )
    {
        handle_error( MEM_ALLOC_ERROR, NULL, NULL );
    }
    OFFSET_PTR( mem );

#ifdef CHECKREALLOC
    if( * (unsigned long *) mem != check )
    {
        handle_error( INTERNAL_ERROR, "Reallocation bug encountered in check_realloc",
                      NO_MESSAGE);
    }
#endif
    return mem;
}


void check_free( void *ptr )
{

    if( ! ptr ) return;

    RESET_PTR( ptr );
    INCREMENT_COUNT( chk_nfree );

    free( ptr );
}

long check_memory_allocation_counts()
{
#ifdef CHECK_MALLOC
    return chk_nalloc - chk_nfree;
#else
    return 0L;
#endif
}
