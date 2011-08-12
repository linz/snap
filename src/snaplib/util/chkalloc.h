#ifndef _CHKALLOC_H
#define _CHKALLOC_H

/*
   $Log: chkalloc.h,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:55:19  CHRIS
   Initial revision

*/

#ifndef CHKALLOC_H_RCSID
#define CHKALLOC_H_RCSID "$Id: chkalloc.h,v 1.2 2004/04/22 02:35:24 ccrook Exp $"
#endif

void *check_malloc( size_t size );
void *check_realloc( void *ptr, size_t size );
void check_free( void *ptr );
long check_memory_allocation_counts();


void *check_malloc_x( size_t size, char *file, int line );
void *check_realloc_x( void *ptr, size_t size, char *file, int line );
void check_free_x( void *ptr, char *file, int line );
long check_memory_allocation_counts_x();
void list_memory_allocations_x( FILE *out );

#ifdef CHECK_MEMORY_ALLOCATIONS
#define check_malloc(x) check_malloc_x((x),__FILE__,__LINE__)
#define check_realloc(x,s) check_realloc_x((x),(s),__FILE__,__LINE__)
#define check_free(x) check_free_x((x),__FILE__,__LINE__)
#define check_memory_allocation_counts check_memory_allocation_counts_x
#define list_memory_allocations list_memory_allocations_x
#else
#define list_memory_allocations(x)
#endif

#endif
