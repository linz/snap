
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

/*

coordsys.h - header file for routines managing coordinate systems.

This includes managing reference frames, ellipsoids, and projections.

*/

#ifndef COORDSYS_H
#define COORDSYS_H

#include <string>
#include <optional>

#ifndef IOSTRING_H
#include "util/iostring.h"
#endif

/* Configuration area for coordinate system files */

#define COORDSYS_CONFIG_SECTION "coordsys"
#define CRDSYSFILE "coordsys.def"
#define CRDSYSENV "COORDSYSDEF"

#define CRDSYS_CODE_LEN 20
#define CRDCNV_CODE_LEN (CRDSYS_CODE_LEN+CRDSYS_CODE_LEN+CRDSYS_CODE_LEN+5)
#define CRDSYS_NAME_LEN 128

enum { CSTP_CARTESIAN, CSTP_GEODETIC, CSTP_PROJECTION };
enum { CS_ELLIPSOID, CS_REF_FRAME, CS_COORDSYS, CS_REF_FRAME_NOTE, CS_COORDSYS_NOTE, CS_VDATUM, CS_COORDSYS_COUNT, CS_INVALID };

/* Default epoch for low accuracy coordinate conversions between coordinate systems.
   Use this to allow conversions where an epoch is required but is not available.
   */

#define DEFAULT_CRDSYS_EPOCH 2000.0

/* Definition of an ellipsoid.  In addition to the ID, code and
	two parameters, it contains several calculated values which are
	useful in coordinate conversions. */

/// Always fully-formed once constructed (no default constructor, all
/// fields const) - copy freely via the compiler-generated copy
/// constructor, never assign.
struct ellipsoid
{
    /// Uppercases code (matching every existing ellipsoid code convention)
    /// and derives b/a2/b2/a2b2 from a/rf.
    ellipsoid( const std::string &code, const std::string &name, double a, double rf );

    const std::string code;  ///< Code for the ellipsoid
    const std::string name;  ///< Name of the ellipsoid
    const double a;          ///< Ellipsoid semi-major axis
    const double b;          ///< Ellipsoid semi-minor axis
    const double rf;         ///< Reciprocal of flattening
    const double a2;         ///< Square of a
    const double b2;         ///< Square of b
    const double a2b2;       ///< a2 - b2
};

/* Definition of a reference frame.  The refcode is an identifier for the
	reference system in terms of which the transformation parameters are
	defined.  This need not be an actual reference frame, though typically
	it will be.  Coordinate transformations are permitted only between systems
	with a common reference system */

struct ref_frame_func;
struct ref_deformation;

/// Always fully-formed once constructed (no default constructor) - copy
/// via copy_ref_frame() (a real deep copy: el/func/def/refrf are owned
/// pointers, so a shallow member-wise copy would be wrong), never via the
/// copy constructor, which is deleted. Destroying one recursively deletes
/// el/func/def/refrf.
///
/// func is temporarily swapped out and restored by some conversion
/// functions; refrf is mutated while resolving a chain of base reference
/// frames, including on already-existing ref_frame objects; calcdate/
/// trans/csrot/snrot/sclfct are a lazily-recomputed cache (see
/// init_ref_frame); defepoch is mutated by define_deformation_model_epoch().
/// None of these five are const, for those reasons.
struct ref_frame
{
    ref_frame( const std::string &code, const std::string &name, ellipsoid *el,
               std::optional<std::string> refcode, double txyz[3], double rxyz[3], double scale,
               double refdate, double dtxyz[3], double drxyz[3], double dscale,
               ref_frame_func *func = nullptr, ref_deformation *def = nullptr,
               int use_iersunits = 0 );
    ref_frame( const ref_frame& ) = delete;
    ~ref_frame();

    const std::string code;        ///< Code for the reference frame
    const std::string name;        ///< Name of the frame
    ellipsoid * const el;          ///< Ellipsoid defined for the frame (nullptr if none)
    double txyz[3];    /* The translation components (m) */
    double rxyz[3];    /* The rotation components (sec)  */
    const double scale;      /* The scale factor (ppm)         */
    double dtxyz[3];   /* The rate of change of translation components (m/yr) */
    double drxyz[3];   /* The rate of change of rotation components (sec/yr)  */
    const double dscale;     /* The rate of change of scale factor (ppm/yr) */
    const double refdate;    /* The date at which the translation,
                          rotation, and scale apply (years) */
    double calcdate;   /* Date at which the calculation values apply */
    double trans[3];   /* Translations applying at the date */
    double csrot[3];   /* Cosine of rotations at calculation date */
    double snrot[3];   /* Sine of rotations at calculation date */
    double sclfct;     /* Scale factor applying at calculation date */
    const bool use_rates;     /* True if have time dependent transformations */
    const int use_iersunits; /* Non-zero if using IERS units mm, mas, ppb */
    const std::optional<std::string> refcode;     ///< Base system code, or nullopt
    ref_frame *refrf;  /* Base system reference frame definition */
    ref_frame_func *func;
    /* Non-standard reference frame conversion function */
    double defepoch;      /* The reference epoch of the deformation model */
    ref_deformation * const def; /* Deformation function */
};

/// Always fully-formed once constructed (no default constructor, all
/// fields const) - copy via copy_ref_frame_func() (a real deep copy: data
/// is deep-copied via copy_func), never via the copy constructor, which is
/// deleted. Destroying one calls delete_func(data). Exactly one concrete
/// implementation exists today (the grid transform in
/// crdsys_rffunc_grid.cpp), reached only through create_rf_grid_func().
struct ref_frame_func
{
    /// type/description are taken by value and moved into the member,
    /// not by const&: both are stored verbatim with no transformation, so
    /// a caller passing a temporary (e.g. the string literal "GRID")
    /// avoids the extra copy const& would still require.
    ref_frame_func( std::string type, std::optional<std::string> description, void *data,
                     void (*delete_func)(void *data),
                     int (*describe_func)(ref_frame *rf, output_string_def *os),
                     void *(*copy_func)(void *data),
                     int (*identical)(void *data1, void *data2),
                     int (*xyz_to_std_func)( ref_frame *rf, double xyz[3], double date ),
                     int (*std_to_xyz_func)( ref_frame *rf, double xyz[3], double date ) );
    ref_frame_func( const ref_frame_func& ) = delete;
    ~ref_frame_func();

    const std::string type;                      ///< Discriminator tag for the concrete implementation, e.g. "GRID"
    const std::optional<std::string> description; ///< Human-readable description used in reporting, or nullopt
    void * const data;                            ///< Opaque payload for the concrete implementation, freed by delete_func
    void (* const delete_func)(void *data);       ///< Frees the object pointed to by this struct's own data member
    int (* const describe_func)(ref_frame *rf, output_string_def *os ); ///< Writes a human-readable description of the transform
    void *(* const copy_func)(void *data);        ///< Deep-copies data
    int (* const identical)(void *data1, void *data2); ///< Compares two data payloads for equality
    int (* const xyz_to_std_func)( ref_frame *rf, double xyz[3], double date ); ///< Overrides the standard xyz->std transform
    int (* const std_to_xyz_func)( ref_frame *rf, double xyz[3], double date ); ///< Overrides the standard std->xyz transform
};

/// Always fully-formed once constructed (no default constructor, all
/// fields const) - copy via copy_ref_deformation() (a real deep copy: data
/// is deep-copied via copy_func), never via the copy constructor, which is
/// deleted. Destroying one calls delete_func(data). 3 concrete
/// implementations exist today (linzdef, grid, and a shared xyz-transform
/// one for both BW14 and Euler deformation types), each reached only
/// through its own factory in crdsys_rfdef_*.cpp. apply_llh is the one
/// field that is genuinely, deliberately null for a real implementation
/// (linzdef) - every other function pointer is unconditionally set by all
/// 3 implementations.
struct ref_deformation
{
    /// type is taken by value and moved into the member, not by const&:
    /// it's stored verbatim with no transformation, so a caller passing a
    /// temporary (e.g. a string literal type tag) avoids the extra copy
    /// const& would still require.
    ref_deformation( std::string type, void *data,
                      void (*delete_func)(void *data),
                      void *(*copy_func)(void *data),
                      int (*identical)(void *data1, void *data2),
                      int (*describe_func)( ref_frame *rf, output_string_def *os ),
                      int (*calc_denu)( ref_frame *rf, double lon, double lat, double epoch, double denu[3]),
                      int (*apply_llh)( ref_frame *rf, double llh[3], double epochfrom, double epochto ) );
    ref_deformation( const ref_deformation& ) = delete;
    ~ref_deformation();

    const std::string type;                       ///< Discriminator tag for the concrete implementation, e.g. "LINZDEF"
    void * const data;                             ///< Opaque payload for the concrete implementation, freed by delete_func
    void (* const delete_func)(void *data);        ///< Frees the object pointed to by this struct's own data member
    void *(* const copy_func)(void *data);         ///< Deep-copies data
    int (* const identical)(void *data1, void *data2); ///< Compares two data payloads for equality
    int (* const describe_func)( ref_frame *rf, output_string_def *os ); ///< Writes a human-readable description of the deformation model
    int (* const calc_denu)( ref_frame *rf, double lon, double lat, double epoch, double denu[3]); ///< Computes the east/north/up offset the model predicts at a given epoch
    int (* const apply_llh)( ref_frame *rf, double llh[3], double epochfrom, double epochto ); ///< Applies the deformation between two epochs directly to llh, or nullptr to use the generic calc_denu-difference fallback
};

/* A projection.  projection_type is defined in a private header file,
   crdsyspj.h */

struct projection_type;

struct projection
{
    projection_type *type;
    void *data;
};

/* Vertical datum definition */

struct vdatum_func;

/// Always fully-formed once constructed (no default constructor, all
/// fields const) - copy via copy_vdatum() (a real deep copy: basehrs/rf/
/// func are owned pointers, so a shallow member-wise copy would be
/// wrong), never via the copy constructor, which is deleted. Destroying
/// one recursively deletes basehrs/rf/func. Exactly one of basehrs/rf is
/// ever set - which constructor is used decides which, structurally
/// (not validated at runtime).
struct vdatum
{
    /// Based on another vertical datum (basehrs), not a reference frame.
    vdatum( const std::string &code, const std::string &name,
            vdatum *basehrs, vdatum_func *hrf,
            std::optional<std::string> source = std::nullopt );
    /// Based directly on a reference frame (rf), not another vertical datum.
    vdatum( const std::string &code, const std::string &name,
            ref_frame *rf, vdatum_func *hrf,
            std::optional<std::string> source = std::nullopt );
    vdatum( const vdatum& ) = delete;
    ~vdatum();

    const std::string code;                  ///< Code for the vertical datum
    const std::string name;                  ///< Name of the surface
    const std::optional<std::string> source; ///< Where the vdatum was loaded from
    vdatum * const basehrs;                  ///< Base reference surface pointer
    ref_frame * const rf;                    ///< The underlying reference frame
    vdatum_func * const func;                ///< Function surface height relative base
                                              ///< surface, or to ellipsoidal if basehrscode is null
};

/* Definition of a coordinate system */

/// Always fully-formed once constructed - copy via copy_coordsys() (a real
/// deep copy: rf/prj/hrs are owned pointers, so a shallow member-wise copy
/// would be wrong), never via the copy constructor, which is deleted.
/// Destroying one recursively deletes rf (if owned)/prj/hrs.
///
/// rf/hrs are mutated after construction by the public set_coordsys_ref_frame()/
/// set_coordsys_vdatum() setters; crdtype is overridden by related_coordsys();
/// gotrange/emin.../ltmax are mutated by define_coordsys_range(); hunits/
/// hmult/vunits/vmult are mutated by define_coordsys_units(); ownsrf/setrf
/// are mutated alongside rf. None of these are const, for those reasons.
struct coordsys
{
    coordsys( const std::string &code, const std::string &name, int type,
              ref_frame *rf, projection *prj,
              std::optional<std::string> source = std::nullopt );
    coordsys( const coordsys& ) = delete;
    ~coordsys();

    const std::string code;        ///< The code for the coordinate system
    const std::string name;        ///< The name of the coordinate system
    const std::optional<std::string> source; ///< Where the coordsys was loaded from
    ref_frame *rf;     /* The reference frame                */
    projection * const prj;   /* The projection - if any            */
    vdatum *hrs;   /* Vertical datum, if any   */
    char crdtype;      /* As per CSTP_ enum above            */
    char gotrange;     /* Defines whether a valid range has  */
    char ownsrf;       /* If true then CS owns rf            */
    char setrf;        /* If true then original rf is overridden */
    double emin, nmin; /* Easting northing limits - if defined */
    double emax, nmax;
    double ltmin, lnmin;  /* Latitude/longitude range */
    double ltmax, lnmax;

    /* NOTE: units information is a placeholder at present - not used */

    std::string hunits;  /* Name of horizontal units */
    double hmult;        /* Multiplier for horizontal units */
    std::string vunits;  /* Name of vertical units */
    double vmult;        /* Multiplier for vertical units */
};

/* Definition of a coordinate conversion */

#define CONVERRSIZE 256
#define CONVMAXRF 10

struct coord_conversion_rf
{
    ref_frame *rf;        /* Reference frame in which conversion is defined */
    char xyz_to_std;      /* Direction, 1 for xyz->base, 0 for base->xyz */
    char def_only;        /* Set if only need to apply deformation, not rf axes trans */
    char need_xyz;        /* Need geocentric at end of step (next rf has different ellipsoid ) */
};

struct coord_conversion
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
    vdatum_func *hrf[CONVMAXRF]; /* Vertical datum functions */
    int      nhrf_from;  /* Number of vertical datum functions from source */
    int      nhrf_to;   /* Number of vertical datum functions to target */

};

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

/* Routines relating to reference frames.  NOTE: The reference frame takes
   over ownership of the ellipsoid.  */

ref_frame *copy_ref_frame( ref_frame *rf );

ref_frame_func *copy_ref_frame_func( ref_frame_func *rff );

ref_deformation *copy_ref_deformation( ref_deformation *rdf );

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
/* NOTE: coordsys's constructor copies the pointers to the component
  features only. If the calling routines needs to retain ownership it
  should make copies before constructing the coordsys */

coordsys *copy_coordsys( coordsys *cs );
coordsys *related_coordsys( coordsys *cs, int type );
int set_coordsys_ref_frame( coordsys *cs, ref_frame *rf );

/* Set the vertical datum for the coordinate system.  The 
 * coordinate system takes ownership of the vertical datum
 *
 * If they are not compatible (based on same datum), then 
 * set_coordsys_vdatum will delete it.
 */

bool coordsys_vdatum_compatible( coordsys *cs, vdatum *hrs );
vdatum *coordsys_vdatum( coordsys *cs );
int set_coordsys_vdatum( coordsys *cs, vdatum *hrs );
void set_coordsys_geoid( coordsys *cs, const char *geoidfile );
bool coordsys_heights_orthometric( coordsys *cs );

/* Define the reference epoch for the coordinate system deformation model */

void define_deformation_model_epoch( coordsys *cs, double epoch );

/* For projection coordinate systems emin, nmin, emax, and nmax are
   obvious.  For geodetic coordinate systems the parameters are
   (cs, lonmin, latmin, lonmax, latmax ) */

void define_coordsys_range( coordsys *cs,
                            double emin, double nmin, double emax, double nmax );

void define_coordsys_units( coordsys *cs,
                            const std::string &hunit, double hmult,
                            const std::string &vunit, double vmult );

/* For projection coordinate systems checks that xyz[CRD_EAST] lies in the
   range emin to emax, and xyz[CRD_NORTH] lies in the range nmin to nmax.
   For geodetic systems checks that xyz[CRD_LAT] lies between latmin amd
   latmax and that xyz[CRD_LON] lies between lonmin and lonmat.  xyz[CRD_LON]
   may be shifted by multiples of 360 degrees to get it in range.

   Return codes are one of OK, INCONSISTENT_DATA (valid after shifting
   longitude), and INVALID_DATA (out of range) */

int check_coordsys_range( coordsys *cs, double xyz[3] );

/* Routines relating to vertical datum systems */

vdatum *geoid_vdatum( const char *geoidfile, ref_frame *rf );
vdatum *copy_vdatum( vdatum *hrs );
int identical_vdatum( vdatum *hrs1, vdatum *hrs2 );
int calc_vdatum_offset( vdatum *hrs, double llh[3], double *height, double *exu );

/* Calculate geoid information from coordinate info.  If exu is not null
 * it is assumed to be a double[3] and receives the coordinates and
 * deflection of the vertical */ 

int coordsys_geoid_exu( coordsys *cs, double llh[3], double *height, double *exu );

vdatum *base_vdatum( vdatum *hrs );
ref_frame *vdatum_ref_frame( vdatum *hrs );

/*=====================================================================*/
/* Creating coordinate system components from definitions in a char    */
/* string.   Return NULL if the string is not valid.  Also return NULL */
/* and call error handler if fail to allocate memory.  Note that the   */
/* error may come from the calls to calls to *getel and *getrf for     */
/* the reference frame and coordinate system routines.                 */

ellipsoid  *parse_ellipsoid_def ( input_string_def &is, int embedded );
ref_frame  *parse_ref_frame_def ( input_string_def &is,
                                  ellipsoid *(*getel)(const char *code ),
                                  ref_frame *(*getrf)(const char *code, int loadref ),
                                  int embedded, int loadref );
int parse_ref_frame_func_def ( input_string_def &is, ref_frame_func **rff );
int parse_ref_deformation_def ( input_string_def &is, ref_deformation **rdf );

projection *parse_projection_def( input_string_def &is );
coordsys   *parse_coordsys_def  ( input_string_def &is,
                                  ref_frame *(*getrf)(const char *code, int loadref ));

int parse_crdsys_epoch( const char *epochstr, double *epoch );

vdatum *parse_vdatum_def ( input_string_def &is,
                                  ref_frame *(*getrf)(const char *code, int loadref ),
                                  vdatum *(*gethrs)(const char *code, int loadref ));

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
   shifted by a multiple of 2*PI to get into range */

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

int geog_to_proj( projection *prj, double lon, double lat,
                   double *easting, double *northing );
int proj_to_geog( projection *prj, double easting, double northing,
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

/* Define coordinate conversion, specifying the epoch at which the      */
/* the conversion will be applied (only applies for conversions        */
/* involving two different deformation models, it is the epoch at      */
/* which the reference frame transformation is applied).  Can be used  */
/* to convert where deformation models have different conversion epochs */

int define_coord_conversion_epoch( coord_conversion *conv,
                                   coordsys *from, coordsys *to, double convepoch );

/* Version converts ellipsoidal coordinates - ignores vertical datum */

int define_ellipsoidal_coord_conversion_epoch( coord_conversion *conv,
                                   coordsys *from, coordsys *to, double convepoch );

int convert_coords( coord_conversion *conv,
                    double *fenh, double *fexu,
                    double *tenh, double *texu );

coordsys *conversion_from_coordsys( coord_conversion *conv );
coordsys *conversion_to_coordsys( coord_conversion *conv );

/*=======================================================================*/
/* Write a description of the item                                       */
/* Will probably write versions that goes directly to a string at some   */
/* point.                                                                */

int  describe_ref_frame( output_string_def *os, ref_frame *rf );
int  describe_deformation_model( output_string_def *os, ref_frame *rf );
int  describe_ellipsoid( output_string_def *os, ellipsoid *el );
int  describe_projection( output_string_def *os, projection *prj );
int  describe_vdatum( output_string_def *os, coordsys *cs );
int  describe_coordsys( output_string_def *os, coordsys *cs );

/*=======================================================================*/
/* Maintenance of lists of coordinate systems                            */

/* Install coordinates systems for selection using hard-coded parameters */

void install_crdsys_nzgd49( void );
void install_crdsys_wgs84( void );
void install_crdsys_nzmg( void );
void install_crdsys_nz_metre_circuits( void );

/* Get definitions from a file */

int install_crdsys_file( const char *file_name );

/// Locates the default coordinate system definition file: first the
/// CRDSYSENV environment variable (used verbatim, not checked to exist),
/// then CRDSYSFILE searched via find_file's FF_TRYALL strategies within
/// the COORDSYS_CONFIG_SECTION config subdirectory. Returns nullopt if
/// neither yields a file.
std::optional<std::string> get_default_crdsys_file();

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

/* Note - load_coordsys handles vertical datum also as cscode/hrscode */

int coordsys_list_count( void);
const char *coordsys_list_code( int item );
const char *coordsys_list_desc( int item );
coordsys * coordsys_from_list( int item );
coordsys * load_coordsys( const char *code );
/* coordsys_load_code returns the code a coordinate system including potential hrs
 * can be loaded as.  Returns a pointer to a static buffer, so must be used or 
 * copied immediately! */
const char* coordsys_load_code( coordsys *cs );

int vdatum_list_count( void);
const char *vdatum_list_code( int item );
const char *vdatum_list_desc( int item );
vdatum * vdatum_from_list( int item );
vdatum * load_vdatum( const char *code );

int get_notes( int type, const char *code, output_string_def *os );
int get_crdsys_notes( coordsys *cs, output_string_def *os );
int get_conv_code_notes( int type, const char *code1, const char *code2, output_string_def *os );
int get_conv_notes( coord_conversion *conv, output_string_def *os );

/// Searches every installed coordinate system source (crdsys_source_def's
/// getcsfile callback, e.g. get_csfile in crdsys_src_csdef.cpp - relative
/// to that source's own file, not the current file context or project)
/// for filename+extension, trying each source in turn until one succeeds.
/// Returns nullopt if none do.
std::optional<std::string> get_crdsys_file(
    const std::string &filename,   ///< base filename to search for
    const std::string &extension );///< extension (incl. the leading '.') to try

/// General purpose search for a coordinate-system-related data file (e.g.
/// a datum grid), trying local/project search strategies first (via
/// find_file's FF_TRYALL, no base file and no config subdirectory), then
/// every installed coordinate system source (via get_crdsys_file), then
/// finally the COORDSYS_CONFIG_SECTION config subdirectory (via find_file
/// again). Returns nullopt if none succeed.
std::optional<std::string> find_coordsys_data_file(
    const std::string &filename,   ///< base filename to search for
    const std::string &extension );///< extension (incl. the leading '.') to try

#endif /* COORDSYS_H defined */
