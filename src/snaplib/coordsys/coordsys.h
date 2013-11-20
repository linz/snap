
/*
   $Log: coordsys.h,v $
   Revision 1.3  2004/04/22 02:34:21  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  2003/11/28 01:59:24  ccrook
   Updated to be able to use grid transformation for datum changes (ie to
   support official NZGD49-NZGD2000 conversion)

   Revision 1.1  1995/12/22 16:24:51  CHRIS
   Initial revision

*/

#ifndef COORDSYS_H_RCSID
#define COORDSYS_H_RCSID "$Id: coordsys.h,v 1.3 2004/04/22 02:34:21 ccrook Exp $"
#endif
/*

coordsys.h - header file for routines managing coordinate systems.

This includes managing reference frames, ellipsoids, and projections.

*/

#ifndef COORDSYS_H
#define COORDSYS_H

#ifndef IOSTRING_H
#include "util/iostring.h"
#endif

/* Configuration area for coordinate system files */

#define COORDSYS_CONFIG_SECTION "coordsys"
#define CRDSYSFILE "coordsys.def"
#define CRDSYSENV "COORDSYSDEF"

#define CRDSYS_CODE_LEN 20
#define CRDCNV_CODE_LEN (CRDSYS_CODE_LEN+CRDSYS_CODE_LEN+1)
#define CRDSYS_NAME_LEN 128

enum { CSTP_CARTESIAN, CSTP_GEODETIC, CSTP_PROJECTION };
enum { CS_ELLIPSOID, CS_REF_FRAME, CS_COORDSYS, CS_REF_FRAME_NOTE, CS_COORDSYS_NOTE, CS_COORDSYS_COUNT, CS_INVALID };

/* Definition of an ellipsoid.  In addition to the ID, code and
	two parameters, it contains several calculated values which are
	useful in coordinate conversions. */

typedef struct
{
    char *code;        /* Code for the ellipsoid    */
    char *name;        /* Name of the ellipsoid     */
    double a;          /* Ellipsoid semi-major axis */
    double b;          /* Ellipsoid semi-minor axis */
    double rf;         /* Reciprocal of flattening  */
    double a2;         /* Square of a               */
    double b2;         /* Square of b               */
    double a2b2;       /* a2 - b2                   */
} ellipsoid;

/* Definition of a reference frame.  The refcode is an identifier for the
	reference system in terms of which the transformation parameters are
	defined.  This need not be an actual reference frame, though typically
	it will be.  Coordinate transformations are permitted only between systems
	with a common reference system */

typedef struct ref_frame_s ref_frame;
typedef struct ref_frame_func_s ref_frame_func;
typedef struct ref_deformation_s ref_deformation;

struct ref_frame_s
{
    char *code;        /* Code for the reference frame   */
    char *name;        /* Name of the frame              */
    ellipsoid *el;     /* Ellipsoid defined for the frame*/
    double txyz[3];    /* The translation components (m) */
    double rxyz[3];    /* The rotation components (sec)  */
    double scale;      /* The scale factor (ppm)         */
    double dtxyz[3];   /* The rate of change of translation components (m/yr) */
    double drxyz[3];   /* The rate of change of rotation components (sec/yr)  */
    double dscale;     /* The rate of change of scale factor (ppm/yr) */
    double refdate;    /* The date at which the translation,
                          rotation, and scale apply (years) */
    double calcdate;   /* Date at which the calculation values apply */
    double trans[3];   /* Translations applying at the date */
    double csrot[3];   /* Cosine of rotations at calculation date */
    double snrot[3];   /* Sine of rotations at calculation date */
    double sclfct;     /* Scale factor applying at calculation date */
    int use_rates;     /* Non-zero if have time dependent transformations */
    int use_iersunits; /* Non-zero if using IERS units mm, mas, ppb */
    char *refcode;     /* Base system code or NULL       */
    ref_frame *refrf;  /* Base system reference frame definition */
    ref_frame_func *func;
    /* Non-standard reference frame conversion function */
    double defepoch;      /* The reference epoch of the deformation model */
    ref_deformation *def; /* Deformation function */
};

struct ref_frame_func_s
{
    char *type;
    char *description;
    void *data;
    void (*delete_func)(void *data);
    int (*describe_func)(ref_frame *rf, output_string_def *os );
    void *(*copy_func)(void *data);
    int (*identical)(void *data1, void *data2);
    int (*xyz_to_std_func)( ref_frame *rf, double xyz[3], double date );
    int (*std_to_xyz_func)( ref_frame *rf, double xyz[3], double date );
};

struct ref_deformation_s
{
    char *type;
    void *data;
    void (*delete_func)(void *data);
    void *(*copy_func)(void *data);
    int (*identical)(void *data1, void *data2);
    int (*describe_func)( ref_frame *rf, output_string_def *os );
    int (*calc_denu)( ref_frame *rf, double lon, double lat, double epoch, double denu[3]);
    int (*apply_llh)( ref_frame *rf,  double llh[3], double epochfrom, double epochto );
};

/* A projection.  projection_type_s is defined in a private header file,
   crdsyspj.h */

typedef struct projection_type_s projection_type;

typedef struct
{
    struct projection_type_s *type;
    void *data;
} projection;

/* Definition of a coordinate system */

typedef struct
{
    char *code;        /* The code for the coordinate system */
    char *name;        /* The name of the coordinate system  */
    char *source;      /* Where the coordsys was loaded from */
    ref_frame *rf;     /* The reference frame                */
    projection *prj;   /* The projection - if any            */
    char crdtype;      /* As per CSTP_ enum above            */
    char gotrange;     /* Defines whether a valid range has  */
    double emin, nmin; /* Easting northing limits - if defined */
    double emax, nmax;
    double ltmin, lnmin;  /* Latitude/longitude range */
    double ltmax, lnmax;

    /* NOTE: units information is a placeholder at present - not used */

    char *hunits;        /* Name of horizontal units */
    double hmult;        /* Multiplier for horizontal units */
    char *vunits;        /* Name of vertical units */
    double vmult;        /* Multiplier for vertical units */
} coordsys;

/* Definition of a coordinate conversion */

#define CONVERRSIZE 256
#define CONVMAXRF 10

typedef struct
{
    ref_frame *rf;        /* Reference frame in which conversion is defined */
    char xyz_to_std;      /* Direction, 1 for xyz->base, 0 for base->xyz */
    char def_only;        /* Set if only need to apply deformation, not rf axes trans */
    char need_xyz;        /* Need geocentric at end of step (next rf has different ellipsoid ) */
} coord_conversion_rf;

typedef struct
{
    coordsys *from;    /* Source reference frame */
    coordsys *to;      /* Target reference frame */
    char     valid;    /* Flags whether a conversion is possible */
    double   epochconv; /* Conversion epoch */
    char     needsepoch; /* Flags whether the conversion needs an epoch defined */
    char     from_prj; /* Need projection of from system */
    char     to_prj;   /* Need to convert coords back to projection */
    char     from_geoc; /* Input system is geocentric */
    char     to_geoc;   /* Output system is geocentric */
    char     need_xyz;  /* Need xyz before first reference frame tfm */
    char     errmsg[CONVERRSIZE]; /* Last error message */;
    coord_conversion_rf crf[CONVMAXRF]; /* Conversion rf steps */
    int      ncrf;      /* Number of steps used */

} coord_conversion;

/*====================================================================*/
/* #defines to locate coordinates in arrays                           */

#define CRD_X  0
#define CRD_Y  1
#define CRD_Z  2

#define CRD_LON 0
#define CRD_LAT 1
#define CRD_HGT 2

#define CRD_EAST 0
#define CRD_NORTH 1

/*====================================================================*/
/* Routines to create, copy and destroy coordinate systems components */

/* Routines relating to ellipsoids */

void init_ellipsoid( ellipsoid *el, double a, double rf );
ellipsoid *create_ellipsoid( const char *code, const char *name, double a, double rf );
ellipsoid *copy_ellipsoid( ellipsoid *el );
void delete_ellipsoid( ellipsoid *el );

/* Routines relating to reference frames.  NOTE: The reference frame takes
   over ownership of the ellipsoid.  */

ref_frame *create_ref_frame( const char *code, const char *name,
                             ellipsoid *el,
                             const char *refcode, double txyz[3], double rxyz[3], double scale,
                             double refdate, double dtxyz[3], double drxyz[3], double dscale );
ref_frame *copy_ref_frame( ref_frame *rf );
void delete_ref_frame( ref_frame *rf );

ref_frame_func *copy_ref_frame_func( ref_frame_func *rff );
void delete_ref_frame_func( ref_frame_func *rff );

ref_deformation *copy_ref_deformation( ref_deformation *rdf );
void delete_ref_deformation( ref_deformation *rdf );

void init_ref_frame( ref_frame *rf, double convepoch );

/* Routines relating to projections */
/* This could go to a private header file */

projection_type *register_projection_type( projection_type *tp );
projection_type *find_projection_type( const char *code );

projection *create_projection( projection_type *type );
projection *copy_projection( projection *prj );
void delete_projection( projection *prj );

void set_projection_name( projection *prj, const char *name );
void set_projection_ellipsoid( projection *prj, ellipsoid *el );


/* Routines relating to coordinate systems */
/* NOTE: create_coordsys copies the pointers to the component features only.
  If the calling routines needs to retain ownership it should make
  copies for the call to create_coordsys */

coordsys *create_coordsys( const char *code, const char *name, int type,
                           ref_frame *rf, projection *prj );
coordsys *copy_coordsys( coordsys *cs );
coordsys *related_coordsys( coordsys *cs, int type );
void delete_coordsys( coordsys *cs );

/* Define the reference epoch for the coordinate system deformation model */

void define_deformation_model_epoch( coordsys *cs, double epoch );

/* For projection coordinate systems emin, nmin, emax, and nmax are
   obvious.  For geodetic coordinate systems the parameters are
   (cs, lonmin, latmin, lonmax, latmax ) */

void define_coordsys_range( coordsys *cs,
                            double emin, double nmin, double emax, double nmax );

void define_coordsys_units( coordsys *cs,
                            char *hunit, double hmult, char *vunit, double vmult );

/* For projection coordinate systems checks that xyz[CRD_EAST] lies in the
   range emin to emax, and xyz[CRD_NORTH] lies in the range nmin to nmax.
   For geodetic systems checks that xyz[CRD_LAT] lies between latmin amd
   latmax and that xyz[CRD_LON] lies between lonmin and lonmat.  xyz[CRD_LON]
   may be shifted by multiples of 360 degrees to get it in range.

   Return codes are one of OK, INCONSISTENT_DATA (valid after shifting
   longitude), and INVALID_DATA (out of range) */

int check_coordsys_range( coordsys *cs, double xyz[3] );


/*=====================================================================*/
/* Creating coordinate system components from definitions in a char    */
/* string.   Return NULL if the string is not valid.  Also return NULL */
/* and call error handler if fail to allocate memory.  Note that the   */
/* error may come from the calls to calls to *getel and *getrf for     */
/* the reference frame and coordinate system routines.                 */

ellipsoid  *parse_ellipsoid_def ( input_string_def *is, int embedded );
ref_frame  *parse_ref_frame_def ( input_string_def *is,
                                  ellipsoid *(*getel)(const char *code ),
                                  ref_frame *(*getrf)(const char *code, int loadref ),
                                  int embedded, int loadref );
int parse_ref_frame_func_def ( input_string_def *is, ref_frame_func **rff );
int parse_ref_deformation_def ( input_string_def *is, ref_deformation **rdf );

projection *parse_projection_def( input_string_def *is );
coordsys   *parse_coordsys_def  ( input_string_def *is,
                                  ref_frame *(*getrf)(const char *code, int loadref ));

int parse_crdsys_epoch( const char *epochstr, double *epoch );

/*=====================================================================*/
/* Getting information about components of coordinate systems.         */
/* Return 1 for true, 0 otherwise.                                     */
/* Related coordinate systems have the same reference frame, but need  */
/* not be of the same type                                             */
/* Identical datum may be the same reference frame but with a different*/
/* deformation model or reference epoch                                */
/* Note that identical coordinate systems does not test the range or   */
/* the units of the coordinate system.                                 */
/*=====================================================================*/

int  related_coordinate_systems( coordsys *c1, coordsys *c2 );
int  identical_coordinate_systems( coordsys *c1, coordsys *c2 );
int  identical_ref_frame_axes( ref_frame *rf1, ref_frame *rf2 );
int  identical_datum( ref_frame *rf1, ref_frame *rf2 );
int  identical_ref_frame_func( ref_frame_func *rff1, ref_frame_func *rff2 );
int  identical_ref_deformation( ref_deformation *def1, ref_deformation *def2 );
int  identical_ellipsoids( ellipsoid *el1, ellipsoid *el2 );
int  identical_projections( projection *prj1, projection *prj2 );

int is_projection( coordsys *cs );
int is_geodetic( coordsys *cs );
int is_geocentric( coordsys *cs );

int has_deformation_model( coordsys *cs );
double deformation_model_epoch( coordsys *cs );

/* Coordinates in range check functions.  Note that the latitude/longitude
   functions take pointers to the lat and long, as the longitude may be
   shifted by a mulltiple of 2*PI to get into range */

int en_coords_in_range( coordsys *cs, double e, double n );
int ll_coords_in_range( coordsys *cs, double *lon, double *lat );

/*=====================================================================*/
/* Routines to do coordinate conversions                               */

/* Conversion of geocentric coordinates to and from the standard
  reference frame */

int xyz_to_std( ref_frame *rf, double xyz[3], double date );
int std_to_xyz( ref_frame *rf, double xyz[3], double date );

/* Deformation calculations */

int ref_deformation_at_epoch( ref_frame *rf, double llh[3],
                              double epoch, double denu[3] );
int apply_ref_deformation_llh( ref_frame *rf, double llh[3],
                               double epochfrom, double epochto );

/* Utility function for deformation implementations */

int rf_apply_enu_deformation_to_llh( ref_frame *rf, double llh[3], double denu[3] );

/* Conversions geodetic (lon, lat, ellipsoidal height) <=> geocentric */
/* Lat, long in radians.  dNdLT and dEdLn may be NULL.  The new array */
/* may overwrite the old if required (i.e. llh == xyz is OK)          */

double *llh_to_xyz( ellipsoid *el, double llh[3], double xyz[3],
                    double *dEdLn, double *dNdLt);

double *xyz_to_llh( ellipsoid *el, double xyz[3], double llh[3] );

/* Conversion from latitude/longitude <=> projection coordinates */

void geog_to_proj( projection *prj, double lon, double lat,
                   double *easting, double *northing );
void proj_to_geog( projection *prj, double easting, double northing,
                   double *lon, double *lat );

/* Conversion of coordinates from one coordinate system to another     */
/* Converts coordinates, deflections, and undulations.  Input coords   */
/* are either projection easting, northing, or long and lat in radians */
/* Input heights are ellipsoidal.  Input deflections are in radians    */
/* Output coordinates may be written to the same vector as input, i.e. */
/* tneh == fneh is valid.  Input and output deflections/undulations    */
/* may be NULL.  Input are treated as 0,0,0 - output are ignored.      */
/* If the input or output coordinate systems are geocentric, then the  */
/* gravitational components are ignored.                               */


int define_coord_conversion( coord_conversion *conv,
                             coordsys *from, coordsys *to );

/* Define coordinate conversion, specifing the epoch at which the      */
/* the conversion will be applied (only applies for conversions        */
/* involving two different deformation models, it is the epoch at      */
/* which the reference frame transformation is applied).  Can be used  */
/* to convert where deformation models have different conversion epochs */

int define_coord_conversion_epoch( coord_conversion *conv,
                                   coordsys *from, coordsys *to, double convepoch );

int convert_coords( coord_conversion *conv,
                    double *fenh, double *fexu,
                    double *tenh, double *texu );

/*=======================================================================*/
/* Write a description of the item                                       */
/* Will probably write versions that goes directly to a string at some   */
/* point.                                                                */

int  describe_ref_frame( output_string_def *os, ref_frame *rf );
int  describe_deformation_model( output_string_def *os, ref_frame *rf );
int  describe_ellipsoid( output_string_def *os, ellipsoid *el );
int  describe_projection( output_string_def *os, projection *prj );
int  describe_coordsys( output_string_def *os, coordsys *cs );

/*=======================================================================*/
/* Maintenance of lists of coordinate systems                            */

/* Install coordinates systems for selection using hard-coded parameters */

void install_crdsys_nzgd49( void );
void install_crdsys_wgs84( void );
void install_crdsys_nzmg( void );
void install_crdsys_nz_metre_circuits( void );

/* Get definitions from a file file  */

int install_crdsys_file( const char *file_name );
int install_default_crdsys_file();
void  install_default_projections( void );

/* Clear the list of definitions */

void uninstall_crdsys_lists( void );

/* Functions to process the list of installed definitions */

int ref_frame_list_count( void );
const char *ref_frame_list_code( int item );
const char *ref_frame_list_desc( int item );
ref_frame * ref_frame_from_list( int item );
ref_frame * load_ref_frame( const char *code );


int ellipsoid_list_count( void );
const char *ellipsoid_list_code( int item );
const char *ellipsoid_list_desc( int item );
ellipsoid * ellipsoid_from_list( int item );
ellipsoid * load_ellipsoid( const char *code );


int coordsys_list_count( void);
const char *coordsys_list_code( int item );
const char *coordsys_list_desc( int item );
coordsys * coordsys_from_list( int item );
coordsys * load_coordsys( const char *code );

int get_notes( int type, const char *code, output_string_def *os );
int get_crdsys_notes( coordsys *cs, output_string_def *os );
int get_conv_code_notes( int type, const char *code1, const char *code2, output_string_def *os );
int get_conv_notes( coord_conversion *conv, output_string_def *os );

#endif /* COORDSYS_H defined */
