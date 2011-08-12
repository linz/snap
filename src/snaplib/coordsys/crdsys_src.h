
/*
   $Log: crdsysdf.h,v $
   Revision 1.1  1995/12/22 16:30:33  CHRIS
   Initial revision

*/

#ifndef CRDSYSDF_H_RCSID
#define CRDSYSDF_H_RCSID "$Id: crdsysdf.h,v 1.1 1995/12/22 16:30:33 CHRIS Exp $"
#endif
/* crdsysdf.h: Header file defining structures and procedures for loading
   coordinate system definitions.  Routines getrf, getel, getcs should
   return OK if the code is found and loaded, MISSING_DATA if it the code
   is not found, and some other error code if there is an error loading
   the code.  (*getcodes) should call (*addfunc) for each code defined
   by the coordinate system source.

   When a list is formed each item can have an associated long id.
   The get.. functions can use this if required.  When get functions are
   not called via a list this will be unavailable, and the id is set to
   CS_ID_UNAVAILABLE (-1) */

#ifndef CRDSYSDF_H
#define CRDSYSDF_H

#ifndef _COORDSYS_H
#include "coordsys/coordsys.h"
#endif

#define CS_ID_UNAVAILABLE -1

typedef struct csd_s
{
    struct csd_s *next;
    void *data;
    int (*getrf)( void *data, long id, const char *code, ref_frame **rf );
    int (*getel)( void *data, long id, const char *code, ellipsoid **el );
    int (*getcs)( void *data, long id, const char *code, coordsys  **cs );
    int (*getcodes)( void *data, void (*addfunc)( int type, long id, const char *code, const char *desc ) );
    int (*delsource)( void *data );
} crdsys_source_def;

int crdsys_source_update( void );
crdsys_source_def *crdsys_sources( void );
void register_crdsys_source( crdsys_source_def *src );

#define CS_ID_UNAVAILABLE -1

#endif




