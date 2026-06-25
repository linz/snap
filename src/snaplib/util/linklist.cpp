#include "snapconfig.h"
/* Routines to create, manage, and delete a linked list of items */

/* Chris Crook, March 1990 */

/*
   $Log: linklist.c,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:53:09  CHRIS
   Initial revision

*/

#include <stdio.h>    /* For definition of NULL */
#include <string.h>
#include <stdlib.h>

#include "util/chkalloc.h"
#include "util/linklist.h"
#include "util/errdef.h"

typedef struct element_s
{
    struct element_s *next;   /* Next member of list */
    void *item;                /* Pointer to data item */
} element;

typedef struct list_s
{
    element *first;  /* First element of the list */
    element *last;   /* Last element of the list */
    element *curr;   /* Current element - used to return elements */
    element *prev;   /* Previous element - used for deletions */
    char hold_ptr;   /* If true, returns last element again */
    int nelement;    /* Count of the number of elements in the list */
    int copysize;    /* If non-zero, then items supplied to the
				routine are copied, otherwise only the
				pointer to the item is saved */
} llist;

/* Basic routines to create a list, add an element to the list,
   and delete the list.  The routine create_list returns a handle
   which is used by all other routines to identify the list.

   The error handler is a function which takes the following parameters:
      1) Integer error status (as defined in errdef.h)
      2) Error message (void *)
      3) Qualifier to error message (void *)
   Both the message parameters may be NULL

*/


#define LIST ((llist *)list)

static int init_list_pointers(llist *list)
{
    list->first = NULL;
    list->last = NULL;
    list->curr = NULL;
    list->prev = NULL;
    list->hold_ptr = 0;
    list->nelement = 0;
    return OK;
}

void *create_list(int copysize)
{
    llist *newlist;
    newlist = (llist *) check_malloc( sizeof(llist) );
    newlist->copysize = copysize;
    init_list_pointers( newlist );
    return (void *) newlist;
}

/* Return a count of items in the list */

int list_count( void *list )
{
    return ((llist *)list)->nelement;
}

/* add_to_list - parameters are the list handle, and a pointer to the
   item to add.  Returns the pointer to the item, or if it fails to
   allocate sufficient space, returns NULL */

/* Creates a copy of a list item if required */

static void *make_list_item(void *item, int size)
{
    void *copy;

    if (size<=0) return item;
    copy = check_malloc( size );
    if( item != NULL) memcpy(copy,item,size);
    return copy;
}

void *add_to_list(void *list, void *item)
{
    llist *lst;
    element *elt;

    lst = (llist *) list;
    item = make_list_item( item, lst->copysize );

    elt = (element *) check_malloc( sizeof(element) );

    lst->prev = lst->last;
    lst->curr = elt;
    if (lst->first==NULL)
        lst->first = elt;
    else
        lst->last->next = elt;
    lst->last = elt;
    lst->nelement++;
    elt->item = item;
    elt->next = NULL;
    return item;
}

/* Dispose of the list - parameters are the list handle, and optionally
   the address of a routine to dispose of a list item. If the items
   have been copied into the list, then free() is called automatically */

/* Clear list clears out the list element, but leaves the list handle
   intact.  Free list disposes of the handle as well */

int clear_list(void *list, void (*dispose)( void *item ) )
{
    llist *lst;
    element *elt,*nxt;

    lst = (llist *) list;
    elt = lst->first;
    while (elt)
    {
        nxt = elt->next;
        if(dispose != NULL) (*dispose)(elt->item);
        if(lst->copysize > 0) check_free( elt->item );
        check_free(elt);
        elt = nxt;
    }
    init_list_pointers( lst );
    return 0;
}

int free_list(void *list, void (*dispose)( void *item ) )
{
    clear_list( list, dispose );
    check_free(list);
    return 0;
}

/* Routine to find an item in the list - sets current and previous
   node pointers and returns a pointer to the matching node
   if the item is found, returns the NULL otherwise */

static element *find_item(llist *lst,
                          void *item)
{
    element *elt,*prv;

    /* Check we are not already on it */

    if (lst->curr && lst->curr->item == item) return lst->curr;

    /* Otherwise hunt through the list for the element */

    for (elt=lst->first, prv=NULL;
            elt && elt->item != item;
            prv=elt, elt = elt->next);
    if(elt)
    {
        lst->curr = elt;
        lst->prev = prv;
        lst->hold_ptr = 0;
    }
    return elt;
}

/* Routines to insert and delete items from the list. Routines return
   the pointer to the item if successful and the item has not been
   released from memory, or NULL if they fail or free the item
   explicitly.

   After del_list_item  the next_list_item routine will return the
   item following that deleted.

   After ins_list_item  the next_list_item routine will returnt the
   item following that inserted. */

void *del_list_item(void *list,
                    void *item)
{
    llist *lst;
    element *elt;

    if (item==NULL) return NULL;
    lst= (llist *) list;
    if ( find_item(lst,item)==NULL ) return NULL;

    elt = lst->curr;
    if(lst->prev) lst->prev->next=elt->next;
    else lst->first = elt->next;
    if(lst->last == elt) lst->last = lst->prev;
    lst->curr = elt->next;
    lst->hold_ptr = 1;
    lst->nelement--;
    if (lst->copysize > 0) { check_free(elt->item); item=NULL; }
    check_free( elt );
    return item;
}

void *ins_list_item(void *list,
                    void *item,
                    void *before)
{
    llist *lst;
    element *elt;

    if (before==NULL) return add_to_list( list, item );

    lst= (llist *) list;
    item = make_list_item( item, lst->copysize );
    if ( find_item(lst,before)==NULL ) return NULL;

    elt = (element *) check_malloc( sizeof(element) );

    elt->item = item;
    elt->next = lst->curr;
    if (lst->prev) lst->prev->next = elt;
    else lst->first=elt;
    lst->curr = elt;
    lst->hold_ptr = 0;
    lst->nelement++;
    return item;
}

/* Routine to sequentially return elements of the list */
/* reset_list_pointer initiallizes the current item pointer and returns the
   number of elements in the list, and next_list_item
   returns items sequentially until the list is exhausted, after which
   it returns NULL. Note that the current item pointer may be
   disturbed if other list routines are called.

   hold_list_pointer may be used to cause the next_list_item to
   return the same value on the next call (assuming the pointer
   is not disturbed by other routines). */

int reset_list_pointer(void *list)
{
    llist *lst;

    lst = (llist *) list;
    lst->curr = lst->prev = NULL;
    lst->hold_ptr = 0;
    return lst->nelement;
}

int hold_list_pointer(void *list)
{

    ((llist *) (list)) -> hold_ptr = 1;
    return 0;
}

/* Incomplete implementation - serves the purpose of SNAP */

void *save_list_pointer( void *list )
{
    return (void *) ( ((llist *) list) -> prev);
}

void restore_list_pointer( void *list, void *saved )
{
    llist *lst;
    lst = (llist *) list;

    lst->prev = (element *) saved;
    lst->curr = lst->prev ? lst->prev->next : lst->first;
    lst->hold_ptr = 0;
}



void *next_list_item(void *list)
{
    llist *lst;
    lst = (llist *) list;

    if (lst->curr)
    {
        if (!lst->hold_ptr)
        {
            lst->prev = lst->curr;
            lst->curr = lst->curr->next;
        }
    }
    else if (lst->prev==NULL)
        lst->curr = lst->first;

    lst->hold_ptr = 0;
    return lst->curr ? lst->curr->item : NULL;
}

/* Routine to apply a user specified routine to all items of the list.
   The routine must be defined as int action( item ) void *item;
   The return value from the routine is ignored */

int process_list(void *list,
                 int (*action)( void *data) )
{
    element *elt;

    for (elt=((llist *)list)->first; elt !=NULL; elt=elt->next)
        (*action)(elt->item);
    return 0;
}

/* Routine to search a list for an item matching a specified value. Requires
   a user to supply a routine compare defined as
     int compare ( item, value ) void *item; void *value;
   which returns 0 if the item value matches that specified, and a non-zero
   value otherwise. Returns a pointer to the matching item, or NULL
   if no match is found */

void *search_list(void *list,
                  int (*compare)(void *item, void *value),
                  void *value)
{
    llist *lst;
    element *elt,*prv;

    lst = (llist *) list;
    for (elt=lst->first, prv=NULL;
            elt !=NULL && (*compare)(elt->item,value) !=0;
            prv=elt, elt=elt->next);
    lst->curr = elt;
    lst->prev = prv;
    lst->hold_ptr = 0;
    return elt ? elt->item : NULL;
}

/* Routine to sort a linked list by crude insertion - potentially slow,
   especially if the list is already sorted. */

int sort_list(void *list,
              int (*compare)( void *item1, void *item2 ) )

{
    llist *lst;
    element *end_sorted;
    element *last_entry;
    element *new_entry;
    element *new_next;
    element *new_prev;

    lst = (llist *) list;
    if( lst->nelement < 2 ) return OK;

    end_sorted = last_entry = lst->first;
    while ( (new_entry = end_sorted->next) != NULL )
    {

        /* To int cut for partially sorted lists, compare the new item
        to add to the list with the last entry, and if it is greater (or
         equal) then start the search with that entry, rather than at
         the beginning of the list */

        if ( (*compare)(new_entry->item,last_entry->item) >= 0 )
        {
            new_prev = last_entry;
            new_next = new_prev->next;
        }
        else
        {
            new_prev = NULL;
            new_next = lst->first;
        }

        while ( new_prev != end_sorted &&
                (*compare)(new_entry->item,new_next->item) >=0 )
        {
            new_prev = new_next;
            new_next = new_prev->next;
        }

        if( new_prev == end_sorted )
            end_sorted = new_entry;
        else
        {
            end_sorted->next = new_entry->next;
            if( new_prev == NULL )
                lst->first = new_entry;
            else
                new_prev->next = new_entry;
            new_entry->next = new_next;
        }
        last_entry = new_entry;
    }

    lst->last = end_sorted;
    lst->curr = lst->prev = NULL;
    lst->hold_ptr = 0;
    return OK;
}


