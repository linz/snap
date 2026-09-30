
/*
   $Log: crdsysdf.h,v $
   Revision 1.1  1995/12/22 16:30:33  CHRIS
   Initial revision

*/

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

#include <string>
#include <string_view>
#include <optional>

#ifndef _COORDSYS_H
#include "coordsys/coordsys.h"
#endif

#define CS_ID_UNAVAILABLE -1

struct crdsys_source_def
{
    struct crdsys_source_def *next;
    void *data;
    /// Searches this one coordinate system source (data) for filename+extension,
    /// e.g. relative to the source's own definition file. nullptr if this source
    /// doesn't support file lookup (e.g. crdsys_src_lists.cpp's in-memory source).
    /// Returns nullopt if this source doesn't have the file.
    std::optional<std::string> (*getcsfile)( void *data, const std::string &filename, const std::string &extension );
    int (*getrf)( void *data, long id, std::string_view code, ref_frame **rf );
    int (*getel)( void *data, long id, std::string_view code, ellipsoid **el );
    int (*getcs)( void *data, long id, std::string_view code, coordsys  **cs );
    int (*gethrs)( void *data, long id, std::string_view code, vdatum  **hrs );
    int (*getnotes)( void *data, int type, const char *code, void *sink, int (*puttext)(const char *note, void *sink ));
    int (*getcodes)( void *data, void (*addfunc)( int type, long id, const char *code, const char *desc ) );
    int (*delsource)( void *data );
};

int crdsys_source_update( void );
crdsys_source_def *crdsys_sources( void );
void register_crdsys_source( crdsys_source_def *src );

#define CS_ID_UNAVAILABLE -1

#endif




