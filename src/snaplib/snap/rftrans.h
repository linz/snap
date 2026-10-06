#ifndef _RFTRANS_H
#define _RFTRANS_H

/*
   $Log: rftrans.h,v $
   Revision 1.1  1995/12/22 17:47:10  CHRIS
   Initial revision

*/

#ifndef _BINFILE_H
#include "util/binfile.h"
#endif

#include <string>
#include <string_view>

typedef double tmatrix[3][3];   /* Vector transformation matrix */

/* Parameters of a reference frame transformation */
/* Note: Code control.c (snap) assumes these are ordered as for
 * IERS parameters.
 */

enum
{
    rfTx,
    rfTy,
    rfTz,
    rfScale,
    rfRotx,
    rfRoty,
    rfRotz,
    rfTxRate,
    rfTyRate,
    rfTzRate,
    rfScaleRate,
    rfRotxRate,
    rfRotyRate,
    rfRotzRate
};

/// The default reference epoch of a reference frame, as a decimal year.
inline constexpr double DEFAULT_REF_EPOCH = 2000.0;

/// Everything about a reference frame except its id and name, which is the
/// part that is stored on disk as a fixed-width block (see RFTRANS_DISK_FIELDS).
/// It holds plain data only, so it is standard layout and `offsetof` is
/// valid on it. Value-initialise it (`RfTransformationData{}`) to zero every field.
struct RfTransformationData
{
    double refepoch;      /* The reference date for the reference frame as a decimal year*/
    double prm[14];        /* Parameters of the transformation */
    double prmCvr[105];
    char calcPrm[14];
    int usage;            /* Or'ed usage flags */
    int prmId[14];       /* The adjustment parameter ids of the transformation */
    unsigned  istopo:1; /* True if the reference frame is topocentric */
    unsigned  isiers:1; /* True if the reference frame was defined using IERS units */
    unsigned  userates:1;  /* True if rates are being used or calculated */
    unsigned  usetrans:1; /* True if the translation component is used */
    unsigned  localoriginok:1; /* True if the origin can be offset */
    unsigned  localorigin:1; /* True if the origin is offset */
    unsigned  calctrans:1; /* True if translation components are being calculated */
    unsigned  calcrot:1;   /* True if rotations components are being calculated */
    unsigned  calcscale:1; /* True if scale is being calculated */
    unsigned  calctransrate:1; /* True if translation components are being calculated */
    unsigned  calcrotrate:1;   /* True if rotations components are being calculated */
    unsigned  calcscalerate:1; /* True if scale is being calculated */
    int   origintype;       /* One of the REFFRM_ORIGIN_ options */
    char  prmUsed[14];     /* True if the reference frame is used in data sets */
    double origin[3];     /* The reference point for the rotation and scale */
    double trans[3];      /* Translation components as XYZ */
    double transrate[3];  /* Translation rate components as XYZ */
    tmatrix tmat;         /* The matrix (1+s).Rx.Ry.Rz, which converts a vector to the reference frame */
    tmatrix invtmat;      /* The inverse of tmat, which converts a vector from the reference frame */
    tmatrix tmatrate;     /* The matrix (1+s).Rx.Ry.Rz           */
    tmatrix invtmatrate;  /* The inverse of tmat                */
    tmatrix dtmatdrot[3]; /* The differential of tmat wrt x rot. */
    tmatrix toporot;      /* Conversion to and from topocentric system */
    tmatrix invtoporot;
};

/// A reference frame transformation: the stored data plus the id and name
/// that identify it.
class rfTransformation : public RfTransformationData
{
public:
    const int id;           ///< Id used to reference the frame
    const std::string name; ///< The name of the reference frame, in upper case

    /// Defines a new reference frame with every parameter zero and the default
    /// reference epoch. The name is converted to upper case.
    rfTransformation(
        int id,                  ///< The id of the frame, its position in the list of frames plus one
        std::string_view name,   ///< The name of the frame
        int rftype );            ///< REFFRM_GEOCENTRIC, REFFRM_TOPOCENTRIC or REFFRM_IERS

    /// Rebuilds a reference frame from its stored data (the binary file loader).
    rfTransformation(
        int id,                             ///< The id of the frame, as stored
        std::string name,                   ///< The name of the frame, as stored
        const RfTransformationData &data ); ///< The stored fixed-width data

    /// Sets the reference date.
    void setRefDate( double date ); ///< A snap date, held as a decimal year

    /// Chooses how the origin of rotation and scale is treated.
    void setOriginType( int origintype ); ///< One of the REFFRM_ORIGIN_ options

    /// Sets the 14 parameters, in the order of the rfTx... enumeration.
    void setParameters(
        const double val[14],      ///< The parameter values
        const int adjust[14],      ///< If non-zero, estimate the parameter in the adjustment
        const int defined[14] );   ///< If non-zero, val holds a value for the parameter

    /// Sets the scale parameter.
    void setScale(
        double scale,   ///< The scale
        int adjust );   ///< If non-zero, estimate the scale in the adjustment

    /// Sets the x, y and z rotation parameters.
    void setRotation(
        const double rot[3],    ///< The rotations
        const int adjust[3] );  ///< If non-zero, estimate the corresponding rotation in the adjustment

    /// Sets the x, y and z translation parameters.
    void setTranslation(
        const double tran[3],   ///< The translations
        const int adjust[3] );  ///< If non-zero, estimate the corresponding translation in the adjustment

    /// Sets the scale rate.
    void setScaleRate(
        double scaleRate,  ///< The scale rate
        int adjust );      ///< If non-zero, estimate the scale rate in the adjustment

    /// Sets the x, y and z rotation rates.
    void setRotationRate(
        const double rotationRates[3],  ///< The rotation rates
        const int adjust[3] );          ///< If non-zero, estimate the corresponding rotation rate in the adjustment

    /// Sets the x, y and z translation rates.
    void setTranslationRate(
        const double translationRates[3],  ///< The translation rates
        const int adjust[3] );             ///< If non-zero, estimate the corresponding translation rate in the adjustment

    /// Sets the reference point for rotation and scale.
    void setOrigin( const double newOrigin[3] ); ///< The X, Y and Z coordinates of the origin

    /// Records that a data set uses the frame, which marks the parameters
    /// that the data can determine as used.
    void flagUsed( int usage_type ); ///< FRF_VECDIFF or FRF_ABSOLUTE

    /// Recalculates the transformation matrices from the parameters.
    void setup();

private:
    /// Sets the calculation flags and whether rates and an offset origin are used.
    void _setupFlags();

    /// Calculates a transformation matrix and its inverse for a set of parameters.
    void _calcTmat(
        const double *parameters,  ///< The rotation and scale parameters (at least 7 values)
        tmatrix tmat,        ///< Receives the matrix
        tmatrix invtmat )    ///< Receives the inverse matrix
        const;
};

// The fixed-width on-disk layout of every field of RfTransformationData
// except the 12 bitfields (packed separately into a uint16_t). The id and
// name of an rfTransformation are written separately - see
// rftrndmp.cpp, where this table is defined and checked at compile time
// against rfTransformation's actual memory layout. Exposed here, rather
// than kept file-local, so a caller elsewhere can walk the same fields via
// for_each_disk_field (util/binfile.h) without re-listing them by hand.
// `extern` (plain C++ external linkage, unrelated to `extern "C"`) is
// required because a `static` array at file scope is only visible within
// its own translation unit - this declares "a definition exists
// elsewhere," letting rftrndmp.cpp's one real array be linked from here.
extern const DiskField RFTRANS_DISK_FIELDS[];
extern const size_t RFTRANS_DISK_FIELD_COUNT;

#define REFFRAMELEN 20

#define REFFRM_DEFAULT            0
#define REFFRM_GEOCENTRIC         0
#define REFFRM_TOPOCENTRIC        1
#define REFFRM_IERS               2

#define REFFRM_ORIGIN_DEFAULT     0
#define REFFRM_ORIGIN_ZERO        1
#define REFFRM_ORIGIN_TOPOCENTRE  2

#define FRF_VECDIFF  1
#define FRF_ABSOLUTE 2

int get_rftrans_id( std::string_view name, int rftype ) ;
int rftrans_count( void );

rfTransformation *rftrans_from_id( int id );
void clear_rftrans_list( void );

/// Adds a reference frame rebuilt from stored data (the binary file loader).
/// Returns nullptr, adding nothing, if \p id is not the next id in sequence.
rfTransformation *add_rftrans( int id, std::string name, const RfTransformationData &data );

/* Setup up reference frames, defining the topocentre */

void setup_rftrans_list( double lt, double ln ) ;


/* Convert a vector to from the reference frame to the standard reference */
/* Takes a date (snap date format) against which the vector is tested */

void rftrans_correct_vector( int rfid, double vd[3], double date );
void rftrans_correct_point( int rfid, double vd[3], double date );

#endif
