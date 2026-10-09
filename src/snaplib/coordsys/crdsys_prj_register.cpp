#include "snapconfig.h"
/* Management of a list of coordinate types */

/*
   $Log: crdsys_prj.c,v $
   Revision 1.1  1995/12/22 16:43:40  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <boost/algorithm/string/predicate.hpp>

#include "coordsys/crdsys_prj.h"

/// A registered projection_type is permanent for the life of the process
/// - never copied or freed (see projection_type's own doc comment) - so
/// this list only ever grows, and prj_type is an owned pointer with no
/// corresponding delete anywhere, by design, not oversight. next/prj_type
/// are never reassigned once a node is linked in - this is a
/// prepend-only list, nodes are never removed or reordered.
struct prj_list
{
    prj_list( prj_list *next, projection_type *prj_type ) :
        next( next ), prj_type( prj_type )
    {}
    prj_list( const prj_list& ) = delete;

    prj_list * const next;
    projection_type * const prj_type;
};

static prj_list *prj_types = nullptr;

projection_type::projection_type( std::string code_, std::string name_, int size_,
                                   param_def *params_, int nparams_,
                                   void *(*create_)( void ),
                                   void (*destroy_)( void *data ),
                                   int (*copy_)( void *trgt, void *src ),
                                   int (*bind_ellipsoid_)( void *data, ellipsoid *el ),
                                   int (*identical_)( void *data1, void *data2 ),
                                   int (*geog_to_proj_)( void *data, double lon, double lat, double *east, double *north ),
                                   int (*proj_to_geog_)( void *data, double east, double north, double *lon, double *lat ),
                                   int (*calc_sf_cv_)( void *data, double lon, double lat, double *sf, double *cv ) ) :
    code( std::move(code_) ), name( std::move(name_) ), size( size_ ),
    params( params_ ), nparams( nparams_ ),
    create( create_ ), destroy( destroy_ ), copy( copy_ ),
    bind_ellipsoid( bind_ellipsoid_ ), identical( identical_ ),
    geog_to_proj( geog_to_proj_ ), proj_to_geog( proj_to_geog_ ), calc_sf_cv( calc_sf_cv_ )
{
}

projection_type *find_projection_type( const std::string &code )
{
    prj_list *pt;
    for( pt = prj_types; pt; pt = pt->next )
    {
        if( boost::algorithm::iequals( code, pt->prj_type->code ) ) return pt->prj_type;
    }
    return nullptr;
}

projection_type *register_projection_type( projection_type *prj )
{
    projection_type *pt = find_projection_type( prj->code );
    if( pt )
    {
        delete prj;
        return pt;
    }
    prj_types = new prj_list( prj_types, prj );
    return prj;
}



