#include "snapconfig.h"
/* classify.c: Manages the classification of observations into user
   defined classes, such as instrument, weather, session, or whatever.

   Each classification is defined by an index.

   Within each classification are a set of values (names) such as
   WEATHER
      GOOD
      BAD

   INSTRUMENT
      T2
      DI20

   And so on.  These names are assigned an index within the
   classification.

   Observations have associated with them a value for each classification
   that applies to them.
*/

/*
   $Log: classify.c,v $
   Revision 1.1  1995/12/22 17:40:45  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <charconv>
#include "util/snapctype.h"

#include "util/binfile.h"
#include "util/dstring.h"
#include "util/classify.h"

#include "util/errdef.h"

#include <boost/numeric/conversion/cast.hpp>
using boost::numeric_cast;

#ifdef DEBUG

#define CHECK_CLASS_ID( id ) check_class_id( this, id )
#define CHECK_VALUE_ID( cid, id ) check_value_id( this, cid, id )

static void check_class_id( const classifications *csf, int id )
{
    if( id <= 0 || id > numeric_cast<int>(csf->class_index.size()) )
    {
        handle_error( INTERNAL_ERROR, "Out of range classification id specified",NO_MESSAGE);
    }
}

static void check_value_id( const classifications *csf, int cid, int id )
{
    check_class_id( csf, cid );
    cid--;
    if( csf->class_index[cid]->type == ClassValueType::Int ) return;
    int cmax = numeric_cast<int>(csf->class_index[cid]->value.size());
    if( id < 0 || id >= cmax )
    {
        handle_error( INTERNAL_ERROR, "Out of range class value id specified", NO_MESSAGE );
    }
}

#else
#define CHECK_CLASS_ID(id)
#define CHECK_VALUE_ID(cid,id)
#endif


static void clean_name( std::string &name )
{
    for( char &c : name ) { if( ISSPACE(c) ) c = '_'; }
}

class_value *class_type::add_value( std::variant<std::string,int> new_value )
{
    value.emplace_back( std::move(new_value) );
    return &value.back();
}

int class_type::value_id( const std::string &value_name, int create )
{
    if( type == ClassValueType::Int )
    {
        int id = 0;
        auto result = std::from_chars( value_name.data(), value_name.data()+value_name.size(), id );
        if( result.ec != std::errc() )
        {
            std::string errmess = "Invalid value " + value_name.substr(0,10) +
                                   " for integer class " + name.substr(0,20);
            (void) errmess;
        }
        return id;
    };

    int cmax = numeric_cast<int>(value.size());
    for( int i=0; i<cmax; i++ )
    {
        if( ismatch(std::get<std::string>(value[i].value).c_str(), value_name.c_str()) ) return i;
    }
    if( ! create ) return CLASS_VALUE_NOT_DEFINED;

    std::string cleaned = value_name;
    clean_name( cleaned );
    add_value( std::move(cleaned) );
    return numeric_cast<int>(value.size())-1;
}

class_value *class_type::find_value( int value_id, int create )
{
    if( type == ClassValueType::Char )
    {
        return &value[value_id];
    }

    for( auto &cv : value )
    {
        if( std::get<int>(cv.value) == value_id ) return &cv;
    }

    if( ! create ) return NULL;

    return add_value( value_id );
}

int classifications::find_or_create_id( const std::string &name, ClassValueType type, int create )
{
    int class_count = numeric_cast<int>(class_index.size());
    for( int i = 0; i<class_count; i++ )
    {
        if( ismatch(name.c_str(), class_index[i]->name.c_str()) ) return i+1;
    }
    if( ! create ) return 0;

    std::string cleaned = name;
    clean_name( cleaned );
    auto ct = std::make_unique<class_type>( std::move(cleaned), type );
    /* Set up the default classification */
    if( type == ClassValueType::Char ) ct->value_id( "Default", 1 );

    class_index.push_back( std::move(ct) );
    return numeric_cast<int>(class_index.size());
}

int classifications::id( const std::string &name, int create )
{
    return find_or_create_id( name, ClassValueType::Char, create );
}

int classification_id_integer( classifications *csf, const std::string &name, int create )
{
    return csf->find_or_create_id( name, ClassValueType::Int, create );
}

std::string classifications::name( int id ) const
{
    CHECK_CLASS_ID( id );
    return class_index[id-1]->name;
}


int classifications::count() const
{
    return numeric_cast<int>(class_index.size());
}

void classifications::set_default_value( int class_id, const std::string &dflt )
{
    CHECK_CLASS_ID(class_id);
    class_id--;
    class_type *ct = class_index[class_id].get();

    if( ct->type != ClassValueType::Char ) return;
    if( ! ct->value.empty() )
    {
        ct->value[0].value = dflt;
    }
}

int classifications::value_id( int class_id, const std::string &value, int create )
{
    CHECK_CLASS_ID(class_id);
    class_id--;

    return class_index[class_id]->value_id( value, create );
}

std::string classifications::value_name( int class_id, int value_id ) const
{
    CHECK_CLASS_ID(class_id);
    class_type *ct = class_index[class_id-1].get();
    if( ct->type == ClassValueType::Int )
    {
        return std::to_string(value_id);
    }
    CHECK_VALUE_ID( class_id, value_id );
    return std::get<std::string>(ct->value[value_id].value);
}

int classifications::value_count( int class_id ) const
{
    CHECK_CLASS_ID(class_id);
    class_type *ct = class_index[class_id-1].get();
    if( ct->type == ClassValueType::Int ) return 0;

    return numeric_cast<int>(ct->value.size());
}

void classifications::set_value_flag( int class_id, int value_id, unsigned char flagbit )
{
    CHECK_VALUE_ID( class_id, value_id );
    class_value *cv = class_index[class_id-1]->find_value(value_id,1);
    cv->usage |= flagbit;
}

void classifications::set_value_error_factor( int class_id, int value_id, double error_factor )
{
    CHECK_VALUE_ID( class_id, value_id );
    class_value *cv = class_index[class_id-1]->find_value(value_id,1);
    cv->error_factor = error_factor;
}

double classifications::value_error_factor( int class_id, int value_id )
{
    CHECK_VALUE_ID( class_id, value_id );
    class_value *cv = class_index[class_id-1]->find_value(value_id,0);
    return cv ? cv->error_factor : 1.0;
}

unsigned char classifications::value_usage( int class_id, int value_id )
{
    CHECK_VALUE_ID( class_id, value_id );
    class_value *cv = class_index[class_id-1]->find_value(value_id,0);
    return cv ? cv->usage : 0;
}



/* Dump and reload classifications */

void classifications::dump( FILE *f ) const
{
    write_raw( f, numeric_cast<int>(class_index.size()) );
    for( auto &cl : class_index )
    {
        dump_string( cl->name, f );
        write_raw( f, numeric_cast<int>(cl->value.size()) );
        write_raw( f, cl->type );
        for( auto &cv : cl->value )
        {
            if( cl->type == ClassValueType::Int )
            {
                write_raw( f, std::get<int>(cv.value) );
            }
            else
            {
                dump_string( std::get<std::string>(cv.value), f );
            }
            write_raw( f, cv.usage );
            write_raw( f, cv.error_factor );
        }
    }
}

int classifications::reload( FILE *f )
{
    int class_count;
    read_raw( f, class_count );
    for( int ic = 0; ic<class_count; ic++ )
    {
        std::string name = reload_string( f );
        int value_count;
        ClassValueType type;
        read_raw( f, value_count );
        read_raw( f, type );
        auto cl = std::make_unique<class_type>( std::move(name), type );
        for( int iv = 0; iv < value_count; iv++ )
        {
            class_value *cv;
            if( type == ClassValueType::Int )
            {
                int v;
                read_raw( f, v );
                cv = cl->add_value( v );
            }
            else
            {
                std::string vname = reload_string( f );
                cv = cl->add_value( std::move(vname) );
            }
            read_raw( f, cv->usage );
            read_raw( f, cv->error_factor );
        }
        class_index.push_back( std::move(cl) );
    }
    return OK;
}
