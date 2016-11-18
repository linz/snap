#include "snapconfig.h"
/* crdsysd1.c: Routines to manage a list of coordinate system codes and
   sources.  The routines return a count of coordinate systems, codes and
   names by index, and the ability to load coordinate system components
   by index */

/*
   $Log: crdsysd1.c,v $
   Revision 1.1  1995/12/22 16:29:18  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_src.h"
#include "util/dstring.h"
#include "util/linklist.h"
#include "util/chkalloc.h"

typedef struct
{
    long id;
    char *code;
    char *desc;
    crdsys_source_def *source;
} crdsys_list_item;

typedef struct
{
    void *list;
    crdsys_list_item **indx;
    int count;
} crdsys_list;

static crdsys_list rflist = { NULL, NULL, 0 };
static crdsys_list ellist = { NULL, NULL, 0 };
static crdsys_list cslist = { NULL, NULL, 0 };
static crdsys_list hrslist = { NULL, NULL, 0 };


static crdsys_source_def *cur_source;
static int registered = 0;
static int update = -1;

static void delete_crdsys_list_item( void *item )
{
    crdsys_list_item *csli;
    csli = (crdsys_list_item *) item;
    check_free( csli->code );
    check_free( csli->desc );
}

static void init_build_list( crdsys_list *cl )
{
    if( cl->list )
    {
        clear_list( cl->list, delete_crdsys_list_item );
    }
    else
    {
        cl->list = create_list( sizeof( crdsys_list_item ));
    }
    if( cl->indx ) check_free( cl->indx );
    cl->indx = NULL;
    cl->count = 0;
}

static void term_build_list( crdsys_list *cl )
{
    int i;
    cl->count = list_count( cl->list );
    if( cl->count == 0 )
    {
        cl->indx=nullptr;
    }
    else
    {
        cl->indx = (crdsys_list_item **) check_malloc( cl->count * sizeof( crdsys_list_item * ) );
        reset_list_pointer( cl->list );
        for( i=0; i<cl->count; i++ )
        {
            cl->indx[i] = (crdsys_list_item *) next_list_item( cl->list );
        }
    }
}

/* These routines are intended to be called indirectly when a list
   of coordinate sources is deleted. */

static void delete_crdsys_list( crdsys_list *cl )
{
    if( cl->indx ) check_free( cl->indx );
    if( cl->list ) free_list( cl->list, delete_crdsys_list_item );
    cl->indx = NULL;
    cl->list = NULL;
    cl->count = 0;
}

// #pragma warning( disable : 4100)

static int delete_crdsys_lists( void *dummy )
{
    delete_crdsys_list( &rflist );
    delete_crdsys_list( &ellist );
    delete_crdsys_list( &cslist );
    delete_crdsys_list( &hrslist );
    registered = 0;
    return 0;
}


static void register_lists( void )
{
    crdsys_source_def csd;
    if( registered ) return;
    csd.data = NULL;
    csd.getel = NULL;
    csd.getrf = NULL;
    csd.getcs = NULL;
    csd.gethrs = NULL;
    csd.getnotes = NULL;
    csd.getcodes = NULL;
    csd.delsource = delete_crdsys_lists;
    register_crdsys_source( &csd );
}


static void add_crdsys_item( int type, long id, const char *code, const char *desc )
{
    void *list;
    crdsys_list_item *item;
    switch( type )
    {
    case CS_REF_FRAME: list = rflist.list; break;
    case CS_ELLIPSOID: list = ellist.list; break;
    case CS_COORDSYS: list = cslist.list; break;
    case CS_VDATUM: list = hrslist.list; break;
    default: return;
    }

    reset_list_pointer( list );
    while( NULL != (item = (crdsys_list_item *) next_list_item( list )) )
    {
        if( _stricmp( item->code, code ) == 0 ) return;
    }

    item = (crdsys_list_item *) add_to_list( list, NEW_ITEM );

    item->id = id;
    item->code = copy_string( code );
    _strupr( item->code );
    item->desc = copy_string( desc );
    item->source = cur_source;
}

static void make_crdsys_lists( void )
{

    /* Is current list up to date? */
    /* This is a bit simplistic - it assumes that the list of sources is
       modified in such a way the source pointer is changed every time the
       list is changed */

    if( crdsys_source_update() == update) return;

    init_build_list( &rflist );
    init_build_list( &ellist );
    init_build_list( &cslist );
    init_build_list( &hrslist );

    for( cur_source = crdsys_sources();
            cur_source;
            cur_source = cur_source->next ) if( cur_source->getcodes )
        {
            (*cur_source->getcodes)( cur_source->data, add_crdsys_item );
        }

    term_build_list( &rflist );
    term_build_list( &ellist );
    term_build_list( &cslist );
    term_build_list( &hrslist );

    register_lists();

    update = crdsys_source_update();
}

static crdsys_list_item *crdsys_item_at( crdsys_list *cl, int item )
{
    make_crdsys_lists();
    if( item < 0 || item >= cl->count ) return NULL;
    return cl->indx[item];
}

static const char * crdsys_item_code( crdsys_list *cl, int item )
{
    crdsys_list_item *cli;
    cli = crdsys_item_at( cl, item );
    return cli ? cli->code : NULL;
}

static const char * crdsys_item_desc( crdsys_list *cl, int item )
{
    crdsys_list_item *cli;
    cli = crdsys_item_at( cl, item );
    return cli ? cli->desc : NULL;
}

int ref_frame_list_count( void )
{
    make_crdsys_lists();
    return rflist.count;
}

const char *ref_frame_list_code( int item )
{
    return crdsys_item_code( &rflist, item );
}

const char *ref_frame_list_desc( int item )
{
    return crdsys_item_desc( &rflist, item );
}

ref_frame * ref_frame_from_list( int item )
{
    ref_frame *rf;
    crdsys_list_item *cli;
    cli = crdsys_item_at( &rflist, item );
    if( cli == NULL )
    {
        rf = NULL;
    }
    else
    {
        (*cli->source->getrf)( cli->source->data, cli->id, cli->code, &rf );
    }
    return rf;
}

int ellipsoid_list_count( void )
{
    make_crdsys_lists();
    return ellist.count;
}

const char *ellipsoid_list_code( int item )
{
    return crdsys_item_code( &ellist, item );
}

const char *ellipsoid_list_desc( int item )
{
    return crdsys_item_desc( &ellist, item );
}

ellipsoid * ellipsoid_from_list( int item )
{
    crdsys_list_item *cli;
    ellipsoid *el;
    cli = crdsys_item_at( &ellist, item );
    if( cli == NULL )
    {
        el = NULL;
    }
    else
    {
        (*cli->source->getel)( cli->source->data, cli->id, cli->code, &el );
    }
    return el;
}

int coordsys_list_count( void )
{
    make_crdsys_lists();
    return cslist.count;
}

const char *coordsys_list_code( int item )
{
    return crdsys_item_code( &cslist, item );
}

const char *coordsys_list_desc( int item )
{
    return crdsys_item_desc( &cslist, item );
}

coordsys * coordsys_from_list( int item )
{
    crdsys_list_item *cli;
    coordsys *cs;
    cli = crdsys_item_at( &cslist, item );
    if( cli == NULL )
    {
        cs = NULL;
    }
    else
    {
        (*cli->source->getcs)( cli->source->data, cli->id, cli->code, &cs );
    }
    return cs;
}

int vdatum_list_count( void )
{
    make_crdsys_lists();
    return hrslist.count;
}

const char *vdatum_list_code( int item )
{
    return crdsys_item_code( &hrslist, item );
}

const char *vdatum_list_desc( int item )
{
    return crdsys_item_desc( &hrslist, item );
}

vdatum * vdatum_from_list( int item )
{
    crdsys_list_item *cli;
    vdatum *cs;
    cli = crdsys_item_at( &hrslist, item );
    if( cli == NULL )
    {
        cs = NULL;
    }
    else
    {
        (*cli->source->gethrs)( cli->source->data, cli->id, cli->code, &cs );
    }
    return cs;
}
