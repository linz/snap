#ifndef _GEOID_H
#define _GEOID_H

/*
   $Log: geoid.h,v $
   Revision 1.4  1998/05/21 03:56:12  ccrook
   Minor bug fixes

   Revision 1.3  1996/05/20 14:02:51  CHRIS
   Added function get_geoid_model.

   Revision 1.2  1996/05/17 22:25:49  CHRIS
   Added function get_geoid_coordsys

   Revision 1.1  1995/12/22 17:27:50  CHRIS
   Initial revision

*/

#ifndef GEOID_H_RCSID
#define GEOID_H_RCSID "$Id: geoid.h,v 1.4 1998/05/21 03:56:12 ccrook Exp $"
#endif

/* Header file for geoid grid calculations */

/* Note: set_find_file_directories should be called before create_geoid_filename */


#include <stdio.h>

#include "geoid/griddata.h"
#include "coordsys/coordsys.h"

#define GEOID_GRID_EXTENSION ".grd"

typedef struct
{
    grid_def *grd;
    coordsys *cs;
    double gridsize;
} geoid_def;

const char *create_geoid_filename( const char *geoidname );
void delete_geoid_filename( const char *filename );

geoid_def *create_geoid_grid( const char *filename );
void delete_geoid_grid( geoid_def *gd );
void print_geoid_header( geoid_def *gd, FILE *out, int width, char *prefix );
void print_geoid_data( geoid_def *gd, FILE *out, char showGrid );
coordsys *get_geoid_coordsys( geoid_def *gd );
const char *get_geoid_model( geoid_def *gd );

/* Note, lat/lon in radians in these calls */

int calculate_geoid_undulation( geoid_def *gd, double lat, double lon, double *undulation );
int calculate_geoid_exu( geoid_def *gd, double lat, double lon, double exu[3] );

#endif

