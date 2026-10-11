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
#include <string>
#include <string_view>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_src.h"

using boost::numeric_cast;

struct crdsys_list_item
{
    long id = 0;
    std::string code;
    std::string desc;
    const crdsys_source_def *source = nullptr;
};

typedef std::vector<crdsys_list_item> crdsys_list;

static crdsys_list rflist;
static crdsys_list ellist;
static crdsys_list cslist;
static crdsys_list hrslist;


static const crdsys_source_def *cur_source = nullptr;
static int registered = 0;
static int update = -1;

/* These routines are intended to be called indirectly when a list
   of coordinate sources is deleted. */

static int delete_crdsys_lists( void * )
{
    rflist.clear();
    ellist.clear();
    cslist.clear();
    hrslist.clear();
    registered = 0;
    return 0;
}


static void register_lists( void )
{
    if( registered ) return;
    crdsys_source_def csd;
    csd.delsource = delete_crdsys_lists;
    register_crdsys_source( csd );
}


static void add_crdsys_item( int type, long id, std::string_view code, std::string_view desc )
{
    crdsys_list *list;
    switch( type )
    {
    case CS_REF_FRAME: list = &rflist; break;
    case CS_ELLIPSOID: list = &ellist; break;
    case CS_COORDSYS: list = &cslist; break;
    case CS_VDATUM: list = &hrslist; break;
    default: return;
    }

    for( const crdsys_list_item &item : *list )
    {
        if( boost::iequals( item.code, code ) ) return;
    }

    list->push_back( { id, boost::to_upper_copy( std::string( code ) ), std::string( desc ), cur_source } );
}

static void make_crdsys_lists( void )
{

    /* Is current list up to date? */
    /* This is a bit simplistic - it assumes that the list of sources is
       modified in such a way the source pointer is changed every time the
       list is changed */

    if( crdsys_source_update() == update) return;

    rflist.clear();
    ellist.clear();
    cslist.clear();
    hrslist.clear();

    for( const crdsys_source_def &source : crdsys_sources() )
    {
        if( ! source.getcodes ) continue;
        cur_source = &source;
        (*source.getcodes)( source.data, add_crdsys_item );
    }
    cur_source = nullptr;

    register_lists();

    update = crdsys_source_update();
}

static const crdsys_list_item *crdsys_item_at( const crdsys_list &cl, int item )
{
    make_crdsys_lists();
    if( item < 0 || numeric_cast<size_t>( item ) >= cl.size() ) return nullptr;
    return &cl[item];
}

static const std::string &crdsys_item_code( const crdsys_list &cl, int item )
{
    static const std::string none;
    const crdsys_list_item *cli = crdsys_item_at( cl, item );
    return cli ? cli->code : none;
}

static const std::string &crdsys_item_desc( const crdsys_list &cl, int item )
{
    static const std::string none;
    const crdsys_list_item *cli = crdsys_item_at( cl, item );
    return cli ? cli->desc : none;
}

int ref_frame_list_count( void )
{
    make_crdsys_lists();
    return numeric_cast<int>( rflist.size() );
}

const std::string &ref_frame_list_code( int item )
{
    return crdsys_item_code( rflist, item );
}

const std::string &ref_frame_list_desc( int item )
{
    return crdsys_item_desc( rflist, item );
}

ref_frame * ref_frame_from_list( int item )
{
    ref_frame *rf = nullptr;
    const crdsys_list_item *cli = crdsys_item_at( rflist, item );
    if( cli )
    {
        (*cli->source->getrf)( cli->source->data, cli->id, cli->code, &rf );
    }
    return rf;
}

int ellipsoid_list_count( void )
{
    make_crdsys_lists();
    return numeric_cast<int>( ellist.size() );
}

const std::string &ellipsoid_list_code( int item )
{
    return crdsys_item_code( ellist, item );
}

const std::string &ellipsoid_list_desc( int item )
{
    return crdsys_item_desc( ellist, item );
}

ellipsoid * ellipsoid_from_list( int item )
{
    ellipsoid *el = nullptr;
    const crdsys_list_item *cli = crdsys_item_at( ellist, item );
    if( cli )
    {
        (*cli->source->getel)( cli->source->data, cli->id, cli->code, &el );
    }
    return el;
}

int coordsys_list_count( void )
{
    make_crdsys_lists();
    return numeric_cast<int>( cslist.size() );
}

const std::string &coordsys_list_code( int item )
{
    return crdsys_item_code( cslist, item );
}

const std::string &coordsys_list_desc( int item )
{
    return crdsys_item_desc( cslist, item );
}

coordsys * coordsys_from_list( int item )
{
    coordsys *cs = nullptr;
    const crdsys_list_item *cli = crdsys_item_at( cslist, item );
    if( cli )
    {
        (*cli->source->getcs)( cli->source->data, cli->id, cli->code, &cs );
    }
    return cs;
}

int vdatum_list_count( void )
{
    make_crdsys_lists();
    return numeric_cast<int>( hrslist.size() );
}

const std::string &vdatum_list_code( int item )
{
    return crdsys_item_code( hrslist, item );
}

const std::string &vdatum_list_desc( int item )
{
    return crdsys_item_desc( hrslist, item );
}

vdatum * vdatum_from_list( int item )
{
    vdatum *cs = nullptr;
    const crdsys_list_item *cli = crdsys_item_at( hrslist, item );
    if( cli )
    {
        (*cli->source->gethrs)( cli->source->data, cli->id, cli->code, &cs );
    }
    return cs;
}
