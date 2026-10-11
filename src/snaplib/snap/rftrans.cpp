#include "snapconfig.h"
/* Code for managing a list of reference frame transformations */

/*
   $Log: rftrans.c,v $
   Revision 1.1  1995/12/22 17:46:54  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <memory>
#include <vector>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include "snap/rftrans.h"
#include "util/dstring.h"
#include "util/geodetic.h"
#include "util/dateutil.h"
#include "util/fieldscanner.hpp"
/* #include "errdef.h" */
#include "util/pi.h"


/* Definition of a reference frame transformation   */

/*  A reference frame is related to the local coordinate system by the
    equation

       Xr = (1+s).Rx.Ry.Rz.Xl + T

    where Xl is the coordinates in the local system
          Xr is the coordinates in the reference frame
          s is a scale factor in ppm
          Rx, Ry and Rz are rotations about the x, y, and z axes
          T is a translation

    A topocentric version is defined as

      Xr = R'(1+s).Rx.Ry.Rz.R.Xl + T

   where R rotates a vector into the topocentric system, and R' does
   the inverse rotation.

  NOTE: Translations not implemented yet!!!
*/

static std::vector<std::unique_ptr<rfTransformation>> rflist;

static char frames_setup = 0;

/// The rotations to and from the topocentric system at the network topocentre,
/// copied into each frame by setup().
static tmatrix topocentreRotation;
static tmatrix invTopocentreRotation;


/// Looks up a reference frame by name, ignoring case.
/// Returns the frame's id, which is its position in `rflist` plus one
/// (ids start at 1, matching rftrans_from_id), or 0 if there is no such frame.
static int find_rftrans( const std::string_view name )
{
    for( size_t index = 0; index < rflist.size(); index++ )
    {
        if( compare_ignoring_case( rflist[index]->name, name ) == 0 ) return boost::numeric_cast<int>( index + 1 );
    }
    return 0;
}


/// Defines a new frame: every parameter is zero, the reference epoch is the default,
/// and the matrices are set up straight away if the topocentre is already known.
rfTransformation::rfTransformation( const int id, const std::string_view name, const int rftype )
    : RfTransformationData{},
      id( id ),
      name( boost::algorithm::to_upper_copy( std::string( name ) ) )
{
    refepoch = DEFAULT_REF_EPOCH;
    origintype = REFFRM_ORIGIN_DEFAULT;
    istopo = rftype == REFFRM_TOPOCENTRIC;
    isiers = rftype == REFFRM_IERS;

    if( frames_setup ) setup();
}

/// Rebuilds a frame from data read from a binary file. The stored matrices are
/// used as they are, because snaplist and snapplot never call setup().
rfTransformation::rfTransformation( const int id, std::string name, const RfTransformationData &data )
    : RfTransformationData( data ),
      id( id ),
      name( std::move( name ) )
{
}

/// Ids are positions in rflist plus one, so only the next id in sequence can be added.
rfTransformation *add_rftrans( const int id, std::string name, const RfTransformationData &data )
{
    if( id != rftrans_count() + 1 ) return nullptr;
    rflist.push_back( std::make_unique<rfTransformation>( id, std::move( name ), data ) );
    return rflist.back().get();
}

/// Deletes every frame, so the next frame added gets id 1.
void clear_rftrans_list( void )
{
    rflist.clear();
}

/// Returns the id of the named frame, defining a new frame of type rftype
/// (REFFRM_GEOCENTRIC, REFFRM_TOPOCENTRIC or REFFRM_IERS) if there is none.
/// The type is ignored for a frame that already exists.
int get_rftrans_id( const std::string_view name, const int rftype )
{
    const int existing = find_rftrans( name );
    if( existing ) return existing;

    const int id = rftrans_count() + 1;
    rflist.push_back( std::make_unique<rfTransformation>( id, name, rftype ) );
    return id;
}

/// The number of frames defined, which is also the highest valid id.
int rftrans_count( void )
{
    return boost::numeric_cast<int>( rflist.size() );
}

/// Returns the frame with the given id (1 to rftrans_count()), or nullptr if there is none.
rfTransformation *rftrans_from_id( const int id )
{
    return id > 0 && id <= rftrans_count() ? rflist[id-1].get() : nullptr;
}

/// Sets the reference date, given as a snap date and held as a decimal year.
void rfTransformation::setRefDate( const double date )
{
    refepoch = date_as_year(date);
}

/// Chooses how the origin of rotation and scale is treated (one of the REFFRM_ORIGIN_ options).
void rfTransformation::setOriginType( const int origintype )
{
    this->origintype = origintype;
}

/// Sets the 14 parameters, in the order of the rfTx... enumeration.
/// A parameter is only changed if defined[i] is set, and is only flagged
/// for adjustment (never cleared) if adjust[i] is set.
void rfTransformation::setParameters( const double val[14], const int adjust[14], const int defined[14] )
{
    for( int i = 0; i < 14; i++ )
    {
        if( defined[i] ) prm[i] = val[i];
        if( adjust[i] ) calcPrm[i] = 1;
    }
}

/// Sets the scale parameter and whether it is adjusted.
void rfTransformation::setScale( const double scale, const int adjust )
{
    prm[rfScale] = scale;
    calcPrm[rfScale] = adjust;
}

/// Sets the x, y and z rotation parameters and whether each is adjusted.
void rfTransformation::setRotation( const double rot[3], const int adjust[3] )
{
    for( int i = 0; i < 3; i++ )
    {
        prm[rfRotx+i] = rot[i];
        calcPrm[rfRotx+i] = adjust[i];
    }
}

/// Sets the x, y and z translation parameters and whether each is adjusted.
/// `trans` is a copy of the translation, except that for a topocentric frame it
/// is the translation multiplied by invtoporot.
void rfTransformation::setTranslation( const double tran[3], const int adjust[3] )
{
    for( int i = 0; i < 3; i++ )
    {
        prm[rfTx+i] = tran[i];
        calcPrm[rfTx+i] = adjust[i];
        trans[i] = tran[i];
    }
    if( istopo ) premult3( &invtoporot[0][0], prm+rfTx, trans, 1 );
}

/// Sets the scale rate and whether it is adjusted. This and the other rate
/// setters switch on the use of rates (userates) if any rate is non-zero or is to be adjusted.
void rfTransformation::setScaleRate( const double scaleRate, const int adjust )
{
    if( scaleRate != 0.0 || adjust ) userates = 1;
    prm[rfScaleRate] = scaleRate;
    calcPrm[rfScaleRate] = adjust;
}

/// Sets the x, y and z rotation rates and whether each is adjusted.
void rfTransformation::setRotationRate( const double rotationRates[3], const int adjust[3] )
{
    for( int i = 0; i < 3; i++ )
    {
        if( rotationRates[i] != 0.0 || adjust[i] ) userates = 1;
        prm[rfRotxRate+i] = rotationRates[i];
        calcPrm[rfRotxRate+i] = adjust[i];
    }
}

/// Sets the x, y and z translation rates and whether each is adjusted.
void rfTransformation::setTranslationRate( const double translationRates[3], const int adjust[3] )
{
    for( int i = 0; i < 3; i++ )
    {
        if( translationRates[i] != 0.0 || adjust[i] ) userates = 1;
        prm[rfTxRate+i] = translationRates[i];
        calcPrm[rfTxRate+i] = adjust[i];
    }
}

/// Sets the reference point for rotation and scale. The origin counts as
/// offset (localorigin) if any component is non-zero.
void rfTransformation::setOrigin( const double newOrigin[3] )
{
    std::copy_n( newOrigin, 3, origin );
    localorigin = (newOrigin[0] != 0 || newOrigin[1] != 0 || newOrigin[2] != 0) ? 1 : 0;
}

/// Called when a data set uses the frame: records the usage and marks the
/// parameters that the data can determine as used. Translations are only
/// marked as used for FRF_ABSOLUTE data.
void rfTransformation::flagUsed( const int usage_type )
{
    usage |= usage_type;
    prmUsed[rfScale] = 1;
    prmUsed[rfScaleRate] = 1;
    for( int i = 0; i < 3; i++ )
    {
        prmUsed[rfRotx+i] = 1;
        prmUsed[rfRotxRate+i] = 1;
    }
    if( usage_type == FRF_ABSOLUTE )
    {
        for( int i = 0; i < 3; i++ )
        {
            prmUsed[rfTx+i] = 1;
            prmUsed[rfTxRate+i] = 1;
        }
    }
}

/// Sets the flags that depend on the parameters: which kinds of parameter are
/// adjusted (calctrans, calcrot...), whether rates are used (userates), and
/// whether it is OK to use an offset origin for reference frame calculations
/// (localoriginok).
void rfTransformation::_setupFlags()
{
    /* If only vectors are used, rather than absolute positions, then
     * translations cannot be calculated.
     *
     * Only apply this if the usage flag has been set.
     */

    if( usage )
    {
        usetrans = usage & FRF_ABSOLUTE ? 1 : 0;
        if( ! usetrans )
        {
            calcPrm[rfTx]=calcPrm[rfTy]=calcPrm[rfTz]=0;
            calcPrm[rfTxRate]=calcPrm[rfTyRate]=calcPrm[rfTzRate]=0;
        }
    }

    /* Set the calculation types */
    /* allcalctrans and allcalctransrate check if all translation parameters are calculated */

    calctrans = (calcPrm[rfTx] || calcPrm[rfTy] || calcPrm[rfTz]) ? 1 : 0;
    const bool allcalctrans = (calcPrm[rfTx] && calcPrm[rfTy] && calcPrm[rfTz]);
    calcrot = (calcPrm[rfRotx] || calcPrm[rfRoty] || calcPrm[rfRotz]) ? 1 : 0;
    calcscale = calcPrm[rfScale] ? 1 : 0;

    calctransrate = (calcPrm[rfTxRate] || calcPrm[rfTyRate] || calcPrm[rfTzRate]) ? 1 : 0;
    const bool allcalctransrate = (calcPrm[rfTxRate] && calcPrm[rfTyRate] && calcPrm[rfTzRate]);
    calcrotrate = (calcPrm[rfRotxRate] || calcPrm[rfRotyRate] || calcPrm[rfRotzRate]) ? 1 : 0;
    calcscalerate = calcPrm[rfScaleRate] ? 1 : 0;

    /* Determine whether we are interested in transformation rates at all */

    userates=0;
    if( prm[rfTxRate] != 0 ||
        prm[rfTyRate] != 0 ||
        prm[rfTzRate] != 0 ||
        prm[rfScaleRate] != 0 ||
        prm[rfRotxRate] != 0 ||
        prm[rfRotyRate] != 0 ||
        prm[rfRotzRate] != 0 ) userates=1;
    if( calctransrate || calcrotrate || calcscalerate ) userates=1;

    /* Set flag for using offset origin in calculations */
    localoriginok=1;

    /* If only using vectors then no advantage in offsetting origin */
    if( ! usetrans ) localoriginok=0;

    /* If the user has requested not to, then don't */
    if( origintype == REFFRM_ORIGIN_ZERO ) localoriginok=0;

    /* If calculating rotation and scale, but not equivalent rates
     * then can't offset origin
     */

    if( (calcrot || calcscale) && ! allcalctrans ) localoriginok=0;
    if( (calcrotrate || calcscalerate) && ! allcalctransrate ) localoriginok=0;

    /* If not calculating scales or rotations then no point */

    if( ! (calcrot || calcrotrate || calcscale || calcscalerate ) ) localoriginok=0;
}


/* Create a rotation matrix for a rotation about axis, where cs is the
   cosine of the rotation angle and sn is the sine of the angle */

static void calcrotmat( int axis, double cs, double sn, tmatrix rot )
{
    int c0, c1, c2;
    c0 = axis;
    c1 = c0+1; if( c1 > 2 ) c1 -= 3;
    c2 = c1+1; if( c2 > 2 ) c2 -= 3;

    rot[c0][c1] = rot[c0][c2] = rot[c1][c0] = rot[c2][c0] = 0.0;
    rot[c0][c0] = 1.0;
    rot[c1][c1] = rot[c2][c2] = cs;
    rot[c1][c2] = sn;
    rot[c2][c1] = -sn;
}


/* Create the differential of the rotation matrix wrt the rotation angle */

static void calcdrotdang( int axis, double cs, double sn, tmatrix drot )
{
    int c0, c1, c2;
    c0 = axis;
    c1 = c0+1; if( c1 > 2 ) c1 -= 3;
    c2 = c1+1; if( c2 > 2 ) c2 -= 3;

    cs *= STOR;
    sn *= STOR;

    drot[c0][c0] = drot[c0][c1] = drot[c0][c2] = drot[c1][c0] = drot[c2][c0] = 0.0;
    drot[c1][c1] = drot[c2][c2] = -sn;
    drot[c1][c2] = cs;
    drot[c2][c1] = -cs;
}


/* Set up the reference frame matrices */

#define DS (double *)

/// Calculates the matrix that converts a vector to the reference frame, and
/// its inverse, for a set of parameters (the frame's own, or the parameters
/// at another epoch). The scale is applied to both.
void rfTransformation::_calcTmat( const double *parameters, tmatrix tmat, tmatrix invtmat ) const
{
    double cs, sn;
    tmatrix mult;
    int axis;
    double angle;
    double scl;
    int i, j;

    if( !istopo )
    {
        calcrotmat( 0, 1.0, 0.0, tmat );
    }
    else
    {
        memcpy(tmat,toporot,sizeof(tmatrix) );
    }

    for( axis = 3; axis--; )
    {
        angle = parameters[rfRotx+axis] * STOR;
        cs = cos(angle);
        sn = sin(angle);
        calcrotmat( axis, cs, sn, mult );
        premult3( DS mult, DS tmat, DS tmat, 3 );
    }

    if( istopo )
    {
        premult3( DS (invtoporot), DS tmat, DS tmat, 3 );
    }

    invtmat[0][0] = tmat[0][0];
    invtmat[0][1] = tmat[1][0];
    invtmat[0][2] = tmat[2][0];
    invtmat[1][0] = tmat[0][1];
    invtmat[1][1] = tmat[1][1];
    invtmat[1][2] = tmat[2][1];
    invtmat[2][0] = tmat[0][2];
    invtmat[2][1] = tmat[1][2];
    invtmat[2][2] = tmat[2][2];

    /* Apply the scale factor */

    scl = 1.0 + parameters[rfScale] * 1.0e-6;

    for( i=0; i<3; i++ ) for( j=0 ; j<3; j++ )
    {
        tmat[i][j] *= scl;
        invtmat[i][j] /= scl;
    }
}

#define TMAT_CALC_MULT 10

/// Recalculates the flags, the topocentric rotations, the translations and the
/// transformation matrices (and for rates, and their derivatives with respect
/// to the rotations) from the frame's parameters.
void rfTransformation::setup()
{
    tmatrix mult;
    double angle, cs, sn, scl;
    int i, j, k, axis;

    _setupFlags();

    for( i = 0; i < 3; i++ ) for( j = 0; j < 3; j++ )
        {
            toporot[i][j] = topocentreRotation[i][j];
            invtoporot[i][j] = invTopocentreRotation[i][j];
        }

    /* Calculate the translation component */

    if( !istopo )
    {
        trans[0] = prm[rfTx];
        trans[1] = prm[rfTy];
        trans[2] = prm[rfTz];
        transrate[0] = prm[rfTxRate];
        transrate[1] = prm[rfTyRate];
        transrate[2] = prm[rfTzRate];
    }
    else
    {
        premult3( &invtoporot[0][0], prm+rfTx, trans, 1 );
        premult3( &invtoporot[0][0], prm+rfTxRate, transrate, 1 );
    }



    /* Calculate that transformation matrix and inverse.
     * Calculate for rates by averaging over TMAT_CALC_MULT years after ref epoch */

    _calcTmat( prm, tmat, invtmat );
    if( userates )
    {
        double futurePrm[7];
        for( i=0; i<7; i++ )
        {
            futurePrm[i]=prm[i]+prm[i+7]*TMAT_CALC_MULT;
        }
        _calcTmat( futurePrm, tmatrate, invtmatrate );
        for( i=0; i<3; i++ )
        {
            for( j=0; j<3; j++ )
            {
                tmatrate[i][j]=(tmatrate[i][j]-tmat[i][j])/TMAT_CALC_MULT;
                invtmatrate[i][j]=(invtmatrate[i][j]-invtmat[i][j])/TMAT_CALC_MULT;
            }
        }
    }

    /*  Calc change of coords for unit change in rotations */

    if( !istopo )
    {
        for( i = 0; i<3; i++ ) calcrotmat( 0, 1.0, 0.0, dtmatdrot[i] );
    }
    else
    {
        for( i=0; i<3; i++ )
        {
            memcpy(dtmatdrot[i],topocentreRotation,sizeof(tmatrix) );
        }
    }

    for( axis = 3; axis--; )
    {
        angle = prm[rfRotx+axis] * STOR;
        cs = cos(angle);
        sn = sin(angle);

        calcrotmat( axis, cs, sn, mult );
        for( i = 0; i<3; i++ )
        {
            if( i != axis )
            {
                premult3( DS mult, DS dtmatdrot[i], DS dtmatdrot[i], 3 );
            }
        }
        calcdrotdang( axis, cs, sn, mult );
        premult3( DS mult, DS dtmatdrot[axis], DS dtmatdrot[axis], 3 );
    }

    if( istopo )
    {
        for( i=0; i<3; i++ )
        {
            premult3( DS (invtoporot), DS dtmatdrot[i], DS dtmatdrot[i], 3 );
        }
    }

    /* Apply the scale factor */

    scl = 1.0 + prm[rfScale] * 1.0e-6;

    for( i=0; i<3; i++ ) for( j=0 ; j<3; j++ )
    {
        for( k=0; k<3; k++ )
        {
            dtmatdrot[k][i][j] *= scl;
        }
    }
}


/* Set up the toporot and invtoporot matrices which define rotations to
   and from the topocentric reference system */

static void setup_topo_rotations( double lt, double ln )
{
    double clt, slt, cln, sln;

    clt = cos(lt); slt = sin(lt);
    cln = cos(ln); sln = sin(ln);

    invTopocentreRotation[0][0] = topocentreRotation[0][0] = -sln;
    invTopocentreRotation[1][0] = topocentreRotation[0][1] = cln;
    invTopocentreRotation[2][0] = topocentreRotation[0][2] = 0.0;
    invTopocentreRotation[0][1] = topocentreRotation[1][0] = -cln*slt;
    invTopocentreRotation[1][1] = topocentreRotation[1][1] = -sln*slt;
    invTopocentreRotation[2][1] = topocentreRotation[1][2] = clt;
    invTopocentreRotation[0][2] = topocentreRotation[2][0] = cln*clt;
    invTopocentreRotation[1][2] = topocentreRotation[2][1] = sln*clt;
    invTopocentreRotation[2][2] = topocentreRotation[2][2] = slt;
}


/// Sets the topocentre (latitude and longitude in radians, as passed to the
/// rotation set-up above) and sets up every frame defined so far. Frames
/// defined later are set up as they are created.
void setup_rftrans_list( const double lt, const double ln )
{
    setup_topo_rotations( lt, ln );

    for( const auto &rf : rflist )
    {
        rf->setup();
    }

    frames_setup = 1;
}

void rftrans_correct_vector( int rfid, double vd[3], double date )
{
    rfTransformation *rf=rftrans_from_id(rfid);
    if( rf->userates )
    {
        double vr[3];
        double factor=date_as_year(date)-rf->refepoch;
        premult3( DS rf->invtmatrate, vd, vr, 1 );
        premult3( DS rf->invtmat, vd, vd, 1 );
        vecadd2( vd, 1, vr, factor, vd );
        return;
    }
    premult3( DS rf->invtmat, vd, vd, 1 );
}

void rftrans_correct_point( int rfid, double vd[3], double date )
{
    rfTransformation *rf=rftrans_from_id(rfid);
    vecdif(vd, rf->origin,vd);
    if( rf->userates )
    {
        double vr[3];
        double factor=date_as_year(date)-rf->refepoch;
        premult3( DS rf->invtmatrate, vd, vr, 1 );
        premult3( DS rf->invtmat, vd, vd, 1 );
        vecadd2( vd, 1, vr, factor, vd );
        vecadd2( vd, 1, rf->transrate, -1.0*factor, vd );
    }
    else
    {
        premult3( DS rf->invtmat, vd, vd, 1 );
        vecdif(vd, rf->trans, vd);
    }

    vecadd(vd,rf->origin,vd);
}

