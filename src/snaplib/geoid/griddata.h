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


#define GRID_FILE_HEADER_1 "SNAP geoid binary file\r\n\x1A"
#define GRID_FILE_HEADER_2 "SNAP grid binary v1.0 \r\n\x1A"
#define GRID_FILE_HEADER_3 "CRS grid binary v1.0  \r\n\x1A"
#define GRID_FILE_HEADER_4 "SNAP grid binary v2.0 \r\n\x1A"
#define GRID_FILE_HEADER_5 "CRS grid binary v2.0  \r\n\x1A"

typedef struct file_row_s file_row;
typedef struct cache_row_s cache_row;

typedef struct
{
    FILE *bin;
    long indexloc;
    double maxy;
    double miny;
    double maxx;
    double minx;
    double yres;
    double xres;
    double vres;
    short ngrdy;
    short ngrdx;
    short ngrdval;    /* Number of values at each point */
    short latlon;     /* True if is a lat/long grid */
    short ncycle;     /* Increments to get around the globe */
    char global;
    char *desc1;
    char *desc2;
    char *desc3;
    char *crdsys;
    int  rowfmt;      /* 1 for simple array of shorts, 2 for compressed longs */
    int  rowsize;     /* Size of a row in bytes */
    int  maxcache;    /* Maximum number of cache entries */
    int  ncache;      /* Current number of cache entries */
    long undef;       /* Value representing and undefined grid value */
    file_row *rows;   /* Definition of rows */
    cache_row *cache; /* The cache */
    cache_row *cache_mru;  /* Most recently used */
    cache_row *cache_lru;  /* Least recently used */
    void *loadbuffer;
} grid_def;

struct file_row_s
{
    long fileloc;
    cache_row *cacheloc;
};

struct cache_row_s
{
    long *data;
    file_row *lat;
    cache_row *next;      /* For LRU cache */
    cache_row *prev;
};


int grd_open_grid_file( const char *filename, int dimension, grid_def **grid );
void grd_delete_grid( grid_def *grd );
void grd_grid_spacing( grid_def *grd, double *dx, double *dy );
int grd_calc_cubic(  grid_def *grd, double x, double y, double *value );
int grd_calc_linear( grid_def *grd, double x, double y, double *value );
const char *grd_coordsys_def( grid_def *grd );
const char *grd_title( grid_def *grd, int titleno );
void grd_print_grid_data( grid_def *grd, FILE *out, char showGrid );

#endif

