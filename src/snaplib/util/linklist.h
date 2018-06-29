#ifndef _LINKLIST_H
#define _LINKLIST_H

/*
   $Log: linklist.h,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:53:26  CHRIS
   Initial revision

*/

#ifndef _ERRDEF_H
#include "util/errdef.h"
#endif

void *create_list(int copysize);
void *add_to_list(void *list, void *item);
int clear_list(void *list, void (*dispose)( void *item) );
int free_list(void *list, void (*dispose)( void *item ) );
int list_count( void *list );
void *del_list_item(void *list, void *item);
void *ins_list_item(void *list, void *item, void *before);
int reset_list_pointer(void *list);
void *save_list_pointer( void *list );
void restore_list_pointer( void *list, void *saved );
int hold_list_pointer(void *list);
void *next_list_item(void *list);
int process_list(void *list, int (*action)( void *item ) );
void *search_list(void *list, int (*compare)( void *item, void *value ),
                  void *value);
int sort_list(void *list, int (*compare)( void *item1, void *item2 ) );

/* Note save_list_pointer and restore_list_pointer are incomplete
   implementations for the purpose of SNAP only */

/* Useful definitions - variations on NULL */

#define NEW_ITEM ((char *) 0)
#define NO_ACTION ((void (*)(void *item)) 0)
#endif
