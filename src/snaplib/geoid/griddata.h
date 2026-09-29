#ifndef GRIDDATA_H
#define GRIDDATA_H

/*
   $Log: griddata.h,v $
   Revision 1.3  1999/05/18 14:36:16  ccrook
   *** empty log message ***

   Revision 1.1  1998/05/14 09:08:00  CHRIS
   Initial revision


*/

/* Header file for geoid grid calculations */

#include <optional>
#include <string>

#define GRID_FILE_HEADER_1 "SNAP geoid binary file\r\n\x1A"
#define GRID_FILE_HEADER_2 "SNAP grid binary v1.0 \r\n\x1A"
#define GRID_FILE_HEADER_3 "CRS grid binary v1.0  \r\n\x1A"
#define GRID_FILE_HEADER_4 "SNAP grid binary v2.0 \r\n\x1A"
#define GRID_FILE_HEADER_5 "CRS grid binary v2.0  \r\n\x1A"

struct file_row;
struct cache_row;

// Maximum number of grid rows held in the in-memory row cache at once.
constexpr int MAXCACHE = 60;

struct grid_def
{
    // Takes ownership of bin (already positioned past the header) and rows.
    // Every value here has already been read and validated by the time this
    // is called (see create_grid_def in griddata.cpp) - this constructor
    // itself always succeeds; a load failure means it is never called at
    // all.
    grid_def( FILE *bin, long indexloc, double miny, double maxy, double minx, double maxx,
              double vres, short ngrdy, short ngrdx, short ngrdval, short latlon, int rowfmt,
              std::optional<std::string> desc1, std::optional<std::string> desc2,
              std::optional<std::string> desc3, std::optional<std::string> crdsys,
              file_row *rows );
    ~grid_def();

    // Returns title line 1-3 (titleno clamped to that range) - absent if
    // the grid file didn't include that title.
    const std::optional<std::string> &title( int titleno ) const;

    FILE * const bin;
    const long indexloc;
    const double maxy;
    const double miny;
    const double maxx;
    const double minx;
    const double yres;
    const double xres;
    const double vres;
    const short ngrdy;
    const short ngrdx;
    const short ngrdval;    /* Number of values at each point */
    const short latlon;     /* True if is a lat/long grid */
    const short ncycle;     /* Increments to get around the globe */
    const char global;
    const std::optional<std::string> desc1;
    const std::optional<std::string> desc2;
    const std::optional<std::string> desc3;
    const std::optional<std::string> crdsys;
    const int rowfmt;      /* 1 for simple array of shorts, 2 for compressed longs */
    const int rowsize;     /* Size of a row in bytes */
    const int maxcache = MAXCACHE;    /* Maximum number of cache entries */
    int ncache = 0;      /* Current number of cache entries */
    const long undef = 0x7FFFFFFF;       /* Value representing and undefined grid value */
    file_row * const rows;   /* Definition of rows */
    cache_row * const cache; /* The cache */
    cache_row *cache_mru = nullptr;  /* Most recently used */
    cache_row *cache_lru = nullptr;  /* Least recently used */
    unsigned char *loadbuffer = nullptr;
};

struct file_row
{
    long fileloc;
    cache_row *cacheloc;
};

struct cache_row
{
    long *data;
    file_row *lat;
    cache_row *next;      /* For LRU cache */
    cache_row *prev;
};


int grd_open_grid_file( const std::string &filename, int dimension, grid_def **grid );
void grd_delete_grid( grid_def *grd );
void grd_grid_spacing( grid_def *grd, double *dx, double *dy );
int grd_calc_cubic(  grid_def *grd, double x, double y, double *value );
int grd_calc_linear( grid_def *grd, double x, double y, double *value );
const char *grd_coordsys_def( grid_def *grd );
const char *grd_title( grid_def *grd, int titleno );
void grd_print_grid_data( grid_def *grd, FILE *out, char showGrid );

#endif

