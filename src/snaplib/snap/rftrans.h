#ifndef _RFTRANS_H
#define _RFTRANS_H

/*
   $Log: rftrans.h,v $
   Revision 1.1  1995/12/22 17:47:10  CHRIS
   Initial revision

*/

#ifndef RFTRANS_H_RCSID
#define RFTRANS_H_RCSID "$Id: rftrans.h,v 1.1 1995/12/22 17:47:10 CHRIS Exp $"
#endif

typedef double tmatrix[3][3];   /* Vector transformation matrix */

/* Parameters of a reference frame transformation */

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

typedef struct
{
    int id;               /* Id used to reference the frame */
    char *name;           /* The name of the reference frame     */
    double refepoch;      /* The reference date for the reference frame as a decimal year*/
    double prm[14];        /* Parameters of the transformation */
    double prmCvr[105];
    char calcPrm[14];
    int prmId[14];       /* The adjustment parameter ids of the tranformation */
    unsigned  userates:1;  /* True if rates are being used or calculated */
    unsigned  istopo:1; /* True if the reference frame is topocentric */
    unsigned  istrans:1; /* True if the translation component is used */
    unsigned  isorigin:1; /* True if the origin is offset */
    unsigned  calctrans:1; /* True if translation components are being calculated */
    unsigned  calcrot:1;   /* True if rotations components are being calculated */
    unsigned  calcscale:1; /* True if scale is being calculated */
    unsigned  calctransrate:1; /* True if translation components are being calculated */
    unsigned  calcrotrate:1;   /* True if rotations components are being calculated */
    unsigned  calcscalerate:1; /* True if scale is being calculated */
    int   origintype;       /* One of the REFFRM_ORIGIN_ options */
    char  prmUsed[14];     /* True if the reference frame is used  in data sets */
    double origin[3];     /* The reference point for the rotation and scale */
    double trans[3];      /* Translation components as XYZ */
    double transrate[3];  /* Translation components as XYZ */
    tmatrix tmat;         /* The matrix (1+s).Rx.Ry.Rz           */
    tmatrix invtmat;      /* The inverse of tmat                */
    tmatrix dtmatdrot[3]; /* The differential of tmat wrt x rot. */
    tmatrix tmatrate;     /* The matrix (1+s).Rx.Ry.Rz           */
    tmatrix invtmatrate;  /* The inverse of tmat                */
    tmatrix dtmatdrotrate[3]; /* The differential of tmat wrt x rot. */
    tmatrix toporot;      /* Conversion to and from topocentric system */
    tmatrix invtoporot;
} rfTransformation;

#define REFFRAMELEN 20

#define REFFRM_ORIGIN_DEFAULT     0
#define REFFRM_ORIGIN_ZERO        1
#define REFFRM_ORIGIN_TOPOCENTRE  2

#define FRF_VECDIFF  1
#define FRF_ABSOLUTE 2

int get_rftrans_id( const char *name ) ;
int get_topocentric_rftrans_id( const char *name );
int rftrans_count( void );

rfTransformation *rftrans_from_id( int id );
void clear_rftrans_list( void );
rfTransformation *new_rftrans( void );

/* Setup up reference frames, defining the topocentre */

void setup_rftrans_list( double lt, double ln ) ;
void setup_rftrans( rfTransformation *rf );

/* Note: date is set as a snap date (converted internally to decimal year) */

int rftrans_topocentric( rfTransformation * rf );
void set_rftrans_ref_date( rfTransformation *rf, double date );
void set_rftrans_origintype( rfTransformation *rf, int origintype );
void set_rftrans_scale( rfTransformation *rf, double scale, int adjust ) ;
void set_rftrans_rotation( rfTransformation *rf, double rot[3], int adjust[3] ) ;
void set_rftrans_translation( rfTransformation *rf, double tran[3], int adjust[3] ) ;
void set_rftrans_scale_rate( rfTransformation *rf, double scale, int adjust ) ;
void set_rftrans_rotation_rate( rfTransformation *rf, double rot[3], int adjust[3] ) ;
void set_rftrans_translation_rate( rfTransformation *rf, double tran[3], int adjust[3] ) ;
void set_rftrans_origin( rfTransformation *rf, double origin[3] );

void flag_rftrans_used( rfTransformation *rf, int usage_type );

/* tmat converts a vector to the reference frame,
   invtmat converts a vector from the reference frame */

double * rftrans_tmat( rfTransformation *rf );
double * rftrans_invtmat( rfTransformation *rf );
const char *   rftrans_name( rfTransformation *rf );

/* Convert a vector to from the reference frame to the standard reference */
/* Takes a date (snap date format) against which the vector is tested */

void rftrans_correct_vector( int rfid, double vd[3], double date );
void rftrans_correct_point( int rfid, double vd[3], double date );


#endif
