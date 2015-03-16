#include "snapconfig.h"
/* Routines to manage an AVL tree structure */

/*
   $Log: avltree.c,v $
   Revision 1.2  2004/04/22 02:35:23  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:51:18  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/errdef.h"
#include "util/avltree.h"


typedef enum { BALANCED, LEFT_TALLER, RIGHT_TALLER } balance_t;
enum { NO_CHANGE, IS_TALLER };

#define VALIDTREE 0xA8F1	         /* Arbitrary number */


typedef struct node_s
{
    void *value;             /* Pointer to node value */
    struct node_s *left;     /* Pointer to left and right branches */
    struct node_s *right;
    balance_t balance;	 /* Definition of balance of the tree */
} avlnode_t;

typedef struct
{
    unsigned valid;		 /* Magic number for validity check */
    struct node_s *root;	 /* Root of the tree */
    int (*compare)(void *, void *);	 /* Comparison function - takes two pointers */
    int copysize;		 /* If non-zero then inserted values are copied */
} avltree_t;


/*--------------------------------------------------------------------*/
/* Initiallize an AVL tree structure ....                             */
/* Allocate and initiallize the handle.                               */

void *init_avltree( int copysize, int (*compare)(void *, void *) )
{
    avltree_t *a;

    a = (avltree_t *) malloc( sizeof(avltree_t) );

    if( a == NULL )
    {
        handle_error(MEM_ALLOC_ERROR,"Unable to allocate sufficient memory",NULL);
        return (void *) NULL;
    }

    a->root = (avlnode_t *) NULL;
    a->compare = compare;
    a->copysize = copysize;
    a->valid = VALIDTREE;

    return (void *) a;
}


/*--------------------------------------------------------------------*/
/* Validity check...                                                  */

static int isvalid( void *a )
{
    int ok;
    ok = ((avltree_t *)a)->valid == VALIDTREE;
    if( !ok )
    {
        handle_error(FATAL_ERROR,
                     "Invalid handle passed to AVL tree routines","");
    }
    return ok;
}

/*--------------------------------------------------------------------*/
/* Code to insert a value into the tree                               */


#define IS_TALLER INCONSISTENT_DATA
#define NO_CHANGE OK

static avlnode_t *make_node( avltree_t *a, void *value)
{
    void *copy;
    avlnode_t *node;

    if( a->copysize )
    {
        copy = (void *) malloc( a->copysize );
        if( !copy )
        {
            handle_error(MEM_ALLOC_ERROR,"Unable to allocate sufficient memory",NULL);
            return (avlnode_t *) NULL;
        }
        memcpy( copy, value, a->copysize );
    }
    else
    {
        copy = value;
    }

    node = (avlnode_t *) malloc( sizeof( avlnode_t ) );
    if( node == NULL )
    {
        handle_error(MEM_ALLOC_ERROR,"Unable to allocate sufficient memory",NULL);
        if( a->copysize ) free( copy );
        return (avlnode_t *) NULL;
    }

    node->value = copy;
    node->left = node->right = (avlnode_t *) NULL;
    node->balance = BALANCED;

    return node;
}

static int free_node( avltree_t *a, avlnode_t *node)
{

    if( a->copysize ) free( node->value );
    free( node );
    return OK;
}


/*--------------------------------------*/
/* Raise the left hand branch of a root */

static int raise_left( avlnode_t **root )
{
    avlnode_t *newroot;

    /* Raise the left branch up to the root */

    newroot=(*root)->left;
    (*root)->left = (*root)->left->right;
    newroot->right = (*root);
    (*root) = newroot;
    return OK;
}


/* Raise the left hand branch of a root */

static int raise_right( avlnode_t **root )
{
    avlnode_t *newroot;

    /* Raise the left branch up to the root */

    newroot=(*root)->right;
    (*root)->right = (*root)->right->left;
    newroot->left = (*root);
    (*root) = newroot;
    return OK;
}


/* Bring the left hand side up to the root - sorting out balances etc
   on the way. */

static int promote_left( avlnode_t **root )
{
    avlnode_t *newroot;

    /* Sort out the balances */

    newroot = (*root)->left;

    /* If the right hand side of the new root is taller than the left,
       this needs to be fixed before the root can be promoted */

    switch ( newroot->balance )
    {

    case LEFT_TALLER:
        newroot->balance = (*root)->balance = BALANCED;
        raise_left( root );
        break;

    case BALANCED:  		/* Shouldn't be able to get here */
        break;

    case RIGHT_TALLER:
        switch( newroot->right->balance )
        {
        case LEFT_TALLER:
            newroot->balance = BALANCED;
            (*root)->balance = RIGHT_TALLER;
            break;
        case BALANCED:
            newroot->balance = BALANCED;
            (*root)->balance = BALANCED;
            break;
        case RIGHT_TALLER:
            newroot->balance = LEFT_TALLER;
            (*root)->balance = BALANCED;
            break;
        }
        newroot->right->balance = BALANCED;
        raise_right( &(*root)->left );
        raise_left( root );
        break;
    }
    return OK;
}



/* Bring the left hand side up to the root - sorting out balances etc
   on the way. */


static int promote_right( avlnode_t **root )
{
    avlnode_t *newroot;

    /* Sort out the balances */

    newroot = (*root)->right;

    /* If the right hand side of the new root is taller than the left,
       this needs to be fixed before the root can be promoted */

    switch ( newroot->balance )
    {

    case RIGHT_TALLER:
        newroot->balance = (*root)->balance = BALANCED;
        raise_right( root );
        break;

    case BALANCED:  		/* Shouldn't be able to get here */
        break;

    case LEFT_TALLER:
        switch( newroot->left->balance )
        {
        case RIGHT_TALLER:
            newroot->balance = BALANCED;
            (*root)->balance = LEFT_TALLER;
            break;
        case BALANCED:
            newroot->balance = BALANCED;
            (*root)->balance = BALANCED;
            break;
        case LEFT_TALLER:
            newroot->balance = RIGHT_TALLER;
            (*root)->balance = BALANCED;
            break;
        }
        newroot->left->balance = BALANCED;
        raise_left( &(*root)->right );
        raise_right( root );
        break;
    }
    return OK;
}



static int insert_node( avlnode_t **root, avlnode_t *newnode, int (*compare)( void *, void *))
{
    int sts;
    int cmp;

    /* Have we reached a leaf? */

    if( *root == NULL )
    {
        *root = newnode;
        return IS_TALLER;
    }

    /* Determine whether to add to left or right branch */

    cmp = compare((*root)->value, newnode->value);
    if( cmp == 0 ) return INVALID_DATA;

    if( cmp < 0 )
    {

        /* Add to the right branch if greater than node value */

        sts = insert_node(&(*root)->right,newnode,compare);

        /* If the tree gets taller, then we must modify the balance, and
        possibly rebalance the tree */

        if( sts == IS_TALLER )
        {

            switch( (*root)->balance )
            {
            case BALANCED:     (*root)->balance = RIGHT_TALLER; break;
            case LEFT_TALLER:  (*root)->balance = BALANCED; sts = NO_CHANGE; break;
            case RIGHT_TALLER: promote_right( root ); sts = NO_CHANGE; break;
            }
        }
    }

    /* Add to the left branch if less than the node value */

    else
    {

        sts = insert_node(&(*root)->left,newnode,compare);

        /* If the tree gets taller, then we must modify the balance, and
        possibly rebalance the tree */

        if( sts == IS_TALLER )
        {

            switch( (*root)->balance )
            {
            case BALANCED:     (*root)->balance = LEFT_TALLER; break;
            case RIGHT_TALLER: (*root)->balance = BALANCED; sts = NO_CHANGE; break;
            case LEFT_TALLER:  promote_left( root ); sts = NO_CHANGE; break;
            }
        }
    }

    return sts;
}



int add_avlnode( void *a, void *value )
{
    int sts;
    avlnode_t *node;

    if( !isvalid(a) ) return FATAL_ERROR;

    node = make_node( (avltree_t *) a, value );
    if( node == NULL ) return MEM_ALLOC_ERROR;


    sts = insert_node( &((avltree_t *)a)->root, node, ((avltree_t *)a)->compare);
    if( sts == IS_TALLER ) sts = OK;

    if( sts != OK )
    {
        free_node( (avltree_t *) a, node );
    }

    return sts;
}


/*---------------------------------------------------------*/
/* Routine to seek a node in the AVL tree                  */


void *find_node( void *a, void *value )
{
    avlnode_t *node;
    int (*compare)(void *, void *);
    int cmp;

    if( !isvalid(a) ) return (void *) NULL;

    node = ((avltree_t *)a)->root;
    compare = ((avltree_t *)a)->compare;

    while( node )
    {
        cmp = (*compare)( node->value, value );
        if( cmp == 0 ) break;
        if( cmp < 0 ) node = node->right; else node = node->left;
    }

    return node ? node->value : (void *) NULL;
}


/*---------------------------------------------------------*/
/* Routine to process AVL nodes in sequence                */


static int process_node( avlnode_t *node, int (*function)(void *))
{

    if( !node ) return OK;

    process_node( node->left, function );

    (*function)(node->value);

    process_node( node->right, function );
    return OK;
}


int process_avlnodes( void *a, int (*function)(void *))
{

    if( !isvalid(a) ) return FATAL_ERROR;

    process_node( ((avltree_t *)a)->root, function );
    return OK;
}


/*------------------------------------------------------------*/
/* Routines to dispose of an avltree                          */


static int free_tree( avltree_t *a, avlnode_t *node)
{

    if( !node ) return OK;
    free_tree( a, node->left );
    free_tree( a, node->right );
    free_node( a, node );
    return OK;
}


int term_avltree( void *a, int (*dispose)())
{

    if( !isvalid(a) ) return FATAL_ERROR;

    if( dispose ) process_avlnodes( a, dispose );

    free_tree( (avltree_t *)a, ((avltree_t *)a)->root );
    ((avltree_t *)a)->valid = 0;
    free( a );
    return OK;
}
