
/*
   $Log: avltree.h,v $
   Revision 1.2  2004/04/22 02:35:23  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:52:26  CHRIS
   Initial revision

*/

/* Header file for AVL tree routines */

#ifndef _AVLTREE_H
#define _AVLTREE_H

void *init_avltree( int copysize, int (*compare)() );
int term_avltree( void *a, int (*dispose)() );

int add_avlnode( void *a, void *value );
void *find_node( void *a, void *value );
int process_avlnodes( void *a, int (*function)() );


#endif /* _AVLTREE_H */
