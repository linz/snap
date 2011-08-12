#ifndef CRDSYSPJ_H
#define CRDSYSPJ_H

/*
   $Log: crdsys_prj.h,v $
   Revision 1.1  1995/12/22 16:46:32  CHRIS
   Initial revision

*/

#ifndef CRDSYSPJ_H_RCSID
#define CRDSYSPJ_H_RCSID "$Id: crdsys_prj.h,v 1.1 1995/12/22 16:46:32 CHRIS Exp $"
#endif

#ifndef COORDSYS_H
#include "coordsys/coordsys.h"
#endif

#ifndef PARAMDEF_H
#include "coordsys/paramdef.h"
#endif

/* Definition of a projection.  This is defined in two components.  First is
	a projection type, such as Transverse Mercator, or NZMG.  This defines a
	set of function pointers used to process the projection.  These functions
	take as a parameter a void * pointer which holds the actual definition of
	the projection.
	*/

struct projection_type_s
{
    char *code;        /* Code for the projection type, eg TM, NZMG, LCC */
    char *name;        /* Name of the type, eg Transverse Mercator */
    int size;

    param_def *params;  /* List of parameters */
    int nparams;

    void *(*create)(   /* Function to allocate and initiallize the projection */
        void );

    void (*destroy)(
        void *data );

    int (*copy)(       /* Copy the definition */
        void *trgt,
        void *src );

    int (*bind_ellipsoid)(  /* Associate an ellipsoid with the projection */
        void *data,
        ellipsoid *el );

    int (*identical)(       /* Compare two copies, return 1 or 0 */
        void *data1,
        void *data2 );

    int (*geog_to_proj)(    /* Conversion routines */
        void *data,
        double lon,
        double lat,
        double *east,
        double *north );

    int (*proj_to_geog)(
        void *data,
        double east,
        double north,
        double *lon,
        double *lat );

    int (*calc_sf_cv)(      /* Calc scale factor and convergence */
        void *data,
        double lon,
        double lat,
        double *sf,
        double *cv );

};

#endif
