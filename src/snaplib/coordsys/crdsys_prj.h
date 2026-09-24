#ifndef CRDSYSPJ_H
#define CRDSYSPJ_H

/*
   $Log: crdsys_prj.h,v $
   Revision 1.1  1995/12/22 16:46:32  CHRIS
   Initial revision

*/

#ifndef COORDSYS_H
#include "coordsys/coordsys.h"
#endif

#ifndef PARAMDEF_H
#include "coordsys/paramdef.h"
#endif

#include <string>

/* Definition of a projection.  This is defined in two components.  First is
	a projection type, such as Transverse Mercator, or NZMG.  This defines a
	set of function pointers used to process the projection.  These functions
	take as a parameter a void * pointer which holds the actual definition of
	the projection.
	*/

/// Always fully-formed once constructed (no default constructor, all
/// fields const) - never copied or destroyed once registered
/// (register_projection_type() either adopts a newly-constructed
/// instance into the permanent process-lifetime list, or deletes a
/// duplicate - see crdsys_prj_register.cpp), so no copy constructor or
/// destructor is declared here at all; the implicit ones are never
/// exercised in practice, but std::string's own destructor still runs
/// correctly if one ever is.
struct projection_type
{
    projection_type( std::string code, std::string name, int size,
                      param_def *params, int nparams,
                      void *(*create)( void ),
                      void (*destroy)( void *data ),
                      int (*copy)( void *trgt, void *src ),
                      int (*bind_ellipsoid)( void *data, ellipsoid *el ),
                      int (*identical)( void *data1, void *data2 ),
                      int (*geog_to_proj)( void *data, double lon, double lat, double *east, double *north ),
                      int (*proj_to_geog)( void *data, double east, double north, double *lon, double *lat ),
                      int (*calc_sf_cv)( void *data, double lon, double lat, double *sf, double *cv ) );
    projection_type( const projection_type& ) = delete;

    const std::string code;  ///< Code for the projection type, eg TM, NZMG, LCC
    const std::string name;  ///< Name of the type, eg Transverse Mercator
    const int size;            ///< Byte size of the per-instance opaque data blob, used when create/destroy/copy aren't supplied

    param_def * const params; ///< List of parameters, owned by the file that registered this type
    const int nparams;          ///< Number of entries in params

    void *(* const create)(   /* Function to allocate and initialise the projection */
        void );

    void (* const destroy)(
        void *data );

    int (* const copy)(       /* Copy the definition */
        void *trgt,
        void *src );

    int (* const bind_ellipsoid)(  /* Associate an ellipsoid with the projection */
        void *data,
        ellipsoid *el );

    int (* const identical)(       /* Compare two copies, return 1 or 0 */
        void *data1,
        void *data2 );

    int (* const geog_to_proj)(    /* Conversion routines */
        void *data,
        double lon,
        double lat,
        double *east,
        double *north );

    int (* const proj_to_geog)(
        void *data,
        double east,
        double north,
        double *lon,
        double *lat );

    int (* const calc_sf_cv)(      /* Calc scale factor and convergence */
        void *data,
        double lon,
        double lat,
        double *sf,
        double *cv );

};

#endif
