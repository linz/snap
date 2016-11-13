#ifndef _NETWORK_H
#define _NETWORK_H

/* Definition and manipulation of a network of geodetic stations.

   The network defines both a coordinate system and a list of stations.

*/


/* Need definitions of a coordinate system and of basic geodetic functions */

#ifndef _COORDSYS_H
#include "coordsys/coordsys.h"
#endif

#ifndef _GEOID_H
#include "geoid/geoid.h"
#endif

#ifndef _GEODETIC_H
#include "util/geodetic.h"
#endif

#ifndef _CLASSIFY_H
#include "util/classify.h"
#endif

#define DFLTSTLIST_EXT  ".stl"
#define DFLTSTOFFS_EXT  ".sts"

/*------------------------------------------------------------------------
   Structures

   station: A geodetic station is defined by a ellipsoidal coordinates,
            gravitational corrections.  It is identified by a station code
            of up to STNCODELEN characters.
------------------------------------------------------------------------*/

#define STNCODELEN 15

typedef struct
{
    char    Code[STNCODELEN+1];   /* Station ID */
    int     id;        /* Used to identify the station - populated when indexed */
    double  ELat;      /* Ellipsoidal lat, long, hgt */
    double  ELon;
    double  OHgt;      /* Orthometric height */
    double  GXi;       /* Gravitational corrections to ellipsoidal */
    double  GEta;      /* coordinates */
    double  GUnd;

    /* The following components are used primarily to calculate
       observations between stations and to facilitate station
       adjustments */

    double  XYZ[3];    /* Geocentric coordinates */
    rotmat  rTopo;     /* Rotation to topocentric system */
    rotmat  rGrav;     /* Rotation to gravimetric system */
    double  dNdLt;     /* The rate of change of latitude with distance */
    double  dEdLn;     /* The rate of change of longitude with distance */
    int   nclass;      /* Count of classifications */
    int   *classval;   /* Array of class values */
    char    *Name;     /* Pointer to station name */
    void    *ts;       /* Station coordinate time series data */
    void    *hook;     /* Pointer to user defined info */
} station;


/*------------------------------------------------------------------------

station_list
   Definition and manipulation of a list of geodetic stations.  The list
   provides a list indexed both by a station number, defined for each
   station that is added to the list, and a station code, which is part
   of the station structure.  Includes facilities for inserting and
   removing stations from the list.  Also has a facility for dumping the
   list to a binary file.

------------------------------------------------------------------------*/


typedef struct
{
    int count;               /* Number of stations */
    int lastid;              /* Last allocated station id - same as count if no deletions*/
    int indexsize;           /* Current size of index */
    station **index;         /* Array of stations */
    int nsorted;             /* Number of sorted ids */
    int maxsortid;           /* Array of ids sorted by code */
    station **codeindex;     /* Array of sorted stations */
    int usesorted;           /* Define which index to use for processing stations */
    int nextstn;             /* The next station to be returned by the iterator */

} station_list;

/*------------------------------------------------------------------------

network:
    A network comprises a coordinate system and a list of stations,
    and a topocentre.

------------------------------------------------------------------------*/

typedef void (*stationfunc)( station *st);

typedef struct
{
    char         *name;          /* Name of network */
    char         *crdsysdef;     /* Definition of the coordinate system */
    station_list *stnlist;       /* List of stations */
    coordsys     *crdsys;        /* Parameters of the coordinate system */
    coordsys     *geosys;        /* Parameters of the related geodetic system */
    coord_conversion ccnet;      /* Coord conversion to network */
    coord_conversion ccgeo;      /* Coord conversion to geodetic */
    double       topolat;        /* Definition of the topocentre */
    double       topolon;
    char         got_topocentre;
    char         options;        /* Flags availability of info about geoid */
    int          orderclsid;     /* Classification id used for station orders */
    classifications stnclasses;  /* Array of classifications used for stations */
    stationfunc  initstation;    /* Function called when a station is added */
    stationfunc  uninitstation;  /* Function called when a station is deleted */
} network;

/* Network options flags */

#define NW_GEOID_HEIGHTS       1
#define NW_DEFLECTIONS         2
#define NW_ELLIPSOIDAL_HEIGHTS 4
#define NW_EXPLICIT_GEOID      8
#define NW_GEOID_INFO          (NW_GEOID_HEIGHTS | NW_DEFLECTIONS)
#define NW_DEC_DEGREES        16

#define STATION_ORDER_CLASS_NAME "Order"

#define NW_MERGEOPT_OVERWRITE 1
#define NW_MERGEOPT_MERGECLASSES 2

#define NW_HGTFIXEDOPT_DEFAULT 0
#define NW_HGTFIXEDOPT_ELLIPSOIDAL 1
#define NW_HGTFIXEDOPT_ORTHOMETRIC 2

/* Options for reading data.  
 *  NW_READOPT_CALCHGTREF for recalculation geoid info if coordsys defines a 
 *                        height reference surface
 *  NW_READOPT_GBFORMAT   for reading data using the very historic "geodetic
 *                        branch" format
 */

#define NW_READOPT_CALCHGTREF      1 
#define NW_READOPT_GBFORMAT       16

/*------------------------------------------------------------------------

List of station/network functions supplied by the library

------------------------------------------------------------------------*/

/* Basic station manipulation - Note that to define a stations position
   you must have an ellipsoid definition.  This is used to calculate the
   parameters used for geodetic calculations.  */

station *new_station( void );
void    delete_station( station *st );

void    init_station( station *st,
                      const char *code, const char *Name,
                      double Lat, double Lon, double Hgt,
                      double Xi, double Eta, double Und,
                      ellipsoid *el );

void    modify_station_coords( station *st,
                               double Lat, double Lon, double Hgt,
                               ellipsoid *el );

void    modify_station_xyz( station *st, double xyz[3], ellipsoid *el );

void init_station_classes( station *s, int nclass );
void set_station_class( station *s, int class_id, int value );
int get_station_class( station *s, int class_id );

void    dump_station( station *s, FILE *bin );
station *reload_station( FILE *bin );

/*-------------------------------------------------------------------*/
/* Geodetic calculations using stations.  These routines calculate various
   parameters between stations.  They also calculate the relationship between
   the calculated values and the station coordinates (XYZ components).  These
   are returned in the variables dst (dst1, dst2) which are vector3 variables.
   If they are not required then NULL pointers can be supplied in place of
   dst1, dst2.   */

/* Latitude, longitude, orthometric height */

double calc_lat( station *st, vector3 dst ) ;
double calc_lon( station *st, vector3 dst ) ;
double calc_ohgt( station *st, double hgt, vector3 dst ) ;
double calc_ehgt( station *st, double hgt, vector3 dst ) ;


/* Geocentric X, Y, Z */

double calc_x( station *st, vector3 dst ) ;
double calc_y( station *st, vector3 dst ) ;
double calc_z( station *st, vector3 dst ) ;

/* Slope distance (metres) */

double calc_distance( station *st1, double hgt1, station *st2, double hgt2,
                      vector3 dst1, vector3 dst2 );

/* Horizontal distance (metres), ie at mean height of marks */

double calc_horizontal_distance( station *st1, station *st2,
                                 vector3 dst1, vector3 dst2 );
/* Ellipsoidal distance (metres) */

double calc_ellipsoidal_distance( station *st1, station *st2,
                                  vector3 dst1, vector3 dst2 );

/* Sea level distance (metres) */

double calc_msl_distance( station *st1, station *st2,
                          vector3 dst1, vector3 dst2 );

/* Azimuth (radians) - use gravitational reference frame if usegrav is
   true, ow use topocentric frame */

double calc_azimuth( station *st1, double hgt1, station *st2, double hgt2,
                     int usegrav, vector3 dst1, vector3 dst2 );

/* Projection azimuth (radians) */

double calc_prj_azimuth( network *net, station *st1, double hgt1, station *st2, double hgt2,
                         vector3 dst1, vector3 dst2 );

/* Zenith distance (radians) */

double calc_zenith_dist( station *st1, double hgt1, station *st2, double hgt2,
                         vector3 dst1, vector3 dst2 );

/* Orthometric height difference (metres) */

double calc_hgt_diff( station *st1, double hgt1, station *st2, double hgt2,
                      vector3 dst1, vector3 dst2 ) ;

/* Vector difference (metres, geocentric XYZ) */

void calc_vec_dif( station *st1, double hgt1, station *st2, double hgt2 ,
                   vector3 dif, vector3 dst1[3], vector3 dst2[3] );

/* Vector difference (metres, geocentric XYZ) */

void calc_xyz( station *st1, double hgt1, vector3 xyz, vector3 dst1[3] );

/* Calculate the ellipsoidal chord to arc correction as a ppm factor */

double ellipsoidal_distance_correction( station *st1, station *st2 );

/*-------------------------------------------------------------------*/
/* Managing a station list.. */

station_list *new_station_list( void );
void    delete_station_list( station_list *sl );

void    sl_add_station( station_list *sl, station *st );
void    sl_remove_station( station_list *sl, station *st );

int   sl_find_station( station_list *sl, const char *code );
int   sl_station_id( station_list *sl, station *st );
station *sl_station_ptr( station_list *sl, int stnindex );

/* Integer pointer to sorted index, and corresponding station  */

int sl_find_station_sorted_id( station_list *sl, const char *code );
station *sl_station_sorted_ptr( station_list *sl, int istn );

/* Iterator, sorted or not */

void    sl_reset_station_list( station_list *sl, int sorted );
station *sl_next_station( station_list *sl );

/* Process stations in sorted order */

void    sl_process_stations( station_list *sl, void *data, void (*function)( station *st, void *data) );

int   sl_number_of_stations( station_list *sl );

void    dump_station_list( station_list *sl, FILE *f );
station_list *reload_station_list( FILE *f );

/* Station code comparison function */

int stncodecmp( const char *s1, const char *s2 );

/* Functions for processing station offsets */

int station_has_offset( station *st );
int station_offset_is_deformation( station *st );
void calc_station_offset( station *st, double date, double denu[3] );
void print_station_offset( FILE *lst, station *st );

/*------------------------------------------------------------------*/
/* The network                                                      */

network *new_network( void );
void init_network( network *nw );
/* Set the function hook called when network station is created or removed.
 * If nw is NULL then sets default values that are used for a new network.
 */
void set_network_initstn_func( network *nw, stationfunc initfunc, stationfunc uninitfunc );
void clear_network( network *nw );
void delete_network( network *nw );

int read_network( network *nw, const char *filename, int options );
int write_network( network *nw, const char *filename, const char *comment,
                   int coord_precision, int (*select)(station *st) );

int merge_network( network *base, network *data, int mergeopts,
                   int (*select)(station *code) );

/* set_network_coordsys.  Returns OK if succeeds.  Otherwise network is unaltered
 * If errmsg is not null will copy up to nsmg chars of error message to it
 */

int   set_network_coordsys( network *nw, coordsys *cs, double epoch, char *errmsg, int nmsg );
void    set_network_name( network *nw, const char *name );

station * new_network_station( network *nw,
                               const char *code, const char *Name,
                               double Lat, double Lon, double Hgt,
                               double Xi, double Eta, double Und );

station * duplicate_network_station(  network *nw,
            station *st,
            const char *newcode,
            const char *name
        );

void    modify_network_station_coords( network *nw, station *st, double Lat,
                                       double Lon, double Hgt );

/* Calculate geoid info from a coordinate system height reference surface 
 * for all stations.  Assumes that the coordinate system is based on the 
 * geocentric coordinate system matching the station coordinates. */
/* For library use only, fixed_height_type one of the NW_HGTFIXOPT... defines */

int calculate_network_coordsys_geoid( network *nw, int errlevel );
int calc_station_geoid_info_from_coordsys( network *nw, coordsys *cs, int fixed_height_type, int errlevel )

/* set_network_geoid errlevel can be OK, no error, INFO_ERROR, or WARNING_DATA */
/* Returns OK, INFO_ERROR, or INCONSISTENT data if some stations cannot be calculated */
/* Returns INVALID_DATA if geoid not defined or invalid coordinate system */

int set_network_geoid( network *nw, const char *geoid, int errlevel );
int set_network_geoid_def( network *nw, geoid_def *gd, int errlevel );

int read_network_station_offsets( network *nw, const char *filename );

/* add_station only to be used by network routines .. use new_network_station */
int   add_station( network *nw, station *st );

void    remove_station( network *nw, station *st );

int   find_station( network *nw, const char *code );
int station_id( network *nw, station *st );
station *station_ptr( network *nw, int stnindex );

int   find_station_sorted_id( network *nw, const char *code );
station *station_sorted_ptr( network *nw, int sortedid );

void    reset_station_list( network *nw, int sorted );
station *next_station( network *nw );

void    process_stations( network *nw, void *data, void (*function)( station *st, void *data) );

int   number_of_stations( network *nw );

void get_network_coordinates( network *nw, station *st, double crd[3] );

void    set_network_topocentre( network *nw, double lat, double lon );
void    get_network_topocentre( network *nw, double *lat, double *lon );
void    get_network_topocentre_xyz( network *nw, double *xyz );


int network_classification_count( network *nw );
int network_class_id( network *nw, const char *classname, int create );
const char *network_class_name( network *nw, int class_id );
int network_class_count( network *nw, int class_id );
int network_class_value_id( network *nw, int class_id, const char *value, int create );
const char *network_class_value( network *nw, int class_id, int value_id );

int add_network_orders( network *nw );
int network_order_count( network *nw );
int network_order_id( network *nw, const char *order, int addorder );
const char *network_order( network *nw, int orderid );
int network_station_order( network *nw, station *stn );

void dump_network( network *nw, FILE *bin );
network *reload_network( FILE *bin );

/* Initiallize an object to hold station selection criteria */
/* If maxstn is greater than zero then the criteria have a selection cache installed 
 * so each station only needs be tested once.  Not useful if only processing each
 * station once in any case!
 */

void *new_station_criteria();

/* Delete the selection criteria object */

void delete_station_criteria( void *psc );

/* Add a cache to existing station criteria */
/* No-op if cache already installed */

void setup_station_criteria_cache( void *psc, int maxstn );

/* Compile selection criteria into the station_criteria object
 * Possible criteria are:
 *   code               station with specified code
 *   code*              station codes starting with code...
 *   code1-code2        stations with codes in the specified range
 *   inside coordsys wktfile   stations inside polygon defined in WKT file
 *   outside coordsys wktfile  stations outside polygon defined in WKT file
 *   class=value        stations matching a specified classification
 *   @file              read selection criteria from each line in file (the file will be sought
 *                      relative to base file and in the current directory)
 *    except ...        don't match following stations/criteria on the line
 */

int  compile_station_criteria( void *psc, network *nw, const char *select, char *basefile );

/* Check if a station matches the criteria */

bool station_criteria_match( void *psc, station *stn );

/* Check station criteria codes are valid.  Tests each station not already
 * tested against network, and returns stations based on selection criteria 
 * specifications for handling of missing station codes 
 */

int check_station_criteria_codes( void *psc, network *nw );

/* Apply station criteria to a network - processes each matching station with a function */

void apply_station_criteria_to_network( void *psc, network *nw, 
    void *data, void (*function)( station *stn, void *data ));

/* Process selected stations - compiles and applies station criteria in
 * a single function */

int process_selected_stations( network *nw, const char *select, char *basefile,
    void *data, void (*function)( station *st, void *data ));

#endif /* NETWORK_H not defined */



