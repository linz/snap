#ifndef _POLYGON_H
#define _POLYGON_H

#include <string>

#define DFLT_WKT_EXT "wkt"

void *read_polygon_wkt( const std::string &filename, int islonlat );
void delete_polygon( void *pgn);
int polygon_contains_point( void *pgn, double lon, double lat );

#endif
