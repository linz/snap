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

#include "snap/rftrans.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/geodetic.h"
#include "util/dateutil.h"
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

#define RFLIST_INC 10

#define DEFAULT_REF_EPOCH 2000

static rfTransformation **rflist = NULL;
static int nrflist = 0;
static int nrftrans = 0;

static char frames_setup = 0;

static tmatrix toporot;
static tmatrix invtoporot;


static int find_rftrans( const char *name )
{
    int nrf;

    for( nrf = 0; nrf < nrftrans; nrf++ )
    {
        if( _stricmp( rflist[nrf]->name, name ) == 0 ) return nrf+1;
    }
    return 0;
}


rfTransformation *new_rftrans( void )
{
    rfTransformation *rf;

    rf = (rfTransformation *) check_malloc( sizeof( rfTransformation ) );

    if( nrftrans >= nrflist )
    {
        nrflist = nrftrans + RFLIST_INC;
        rflist = (rfTransformation **) check_realloc( rflist,
                 nrflist * sizeof( rfTransformation *) );
    }

    rflist[nrftrans] = rf;
    nrftrans++;

    rf->name = NULL;
    rf->id = nrftrans;

    return rf;
}

void clear_rftrans_list( void )
{
    int i;
    for( i = 0; i < nrftrans; i++ )
    {
        if( rflist[i] )
        {
            if( rflist[i]->name ) check_free( rflist[i]->name );
            check_free( rflist[i] );
        }
    }
    nrftrans = 0;
}

static int create_rftrans( const char *name, int rftype )
{
    rfTransformation *rf;
    int i;

    rf = new_rftrans();

    rf->name = copy_string( name );
    _strupr( rf->name );
    rf->refepoch=DEFAULT_REF_EPOCH;
    rf->usage=0;
    rf->userates=0;
    rf->usetrans = 0;
    rf->origintype = REFFRM_ORIGIN_DEFAULT;
    rf->localoriginok = 0;
    rf->localorigin = 0;
    rf->calctrans = 0;
    rf->calcrot = 0;
    rf->calcscale = 0;
    rf->calctransrate = 0;
    rf->calcrotrate = 0;
    rf->calcscalerate = 0;
    rf->istopo = rftype == REFFRM_TOPOCENTRIC;
    rf->isiers = rftype == REFFRM_IERS;

    rf->origin[0] = 0.0;
    rf->origin[1] = 0.0;
    rf->origin[2] = 0.0;

    rf->trans[0] = 0.0;
    rf->trans[1] = 0.0;
    rf->trans[2] = 0.0;
    rf->transrate[0] = 0.0;
    rf->transrate[1] = 0.0;
    rf->transrate[2] = 0.0;

    for( i = 0; i < 14; i++ )
    {
        rf->prm[i] = 0.0;
        rf->calcPrm[i] = 0;
        rf->prmId[i] = 0;
        rf->prmUsed[i] = 0;
    }
    for( i = 0; i < 105; i++ )
    {
        rf->prmCvr[i] = 0.0;
    }

    if( frames_setup ) setup_rftrans( rf );

    return nrftrans;
}


int get_rftrans_id( const char *name, int rftype )
{
    int rf;

    rf = find_rftrans( name );
    if( !rf ) rf = create_rftrans( name, rftype );
    return rf;
}

int rftrans_count( void )
{
    return nrftrans;
}

rfTransformation *rftrans_from_id( int id )
{
    return id > 0 && id <= nrftrans ? rflist[id-1] : NULL;
}

int rftrans_topocentric( rfTransformation *rf )
{
    return rf->istopo;
}

int rftrans_iers( rfTransformation *rf )
{
    return rf->isiers;
}

void set_rftrans_ref_date( rfTransformation *rf , double date )
{
    rf->refepoch = date_as_year(date);
}

void set_rftrans_origintype( rfTransformation *rf, int origintype )
{
    rf->origintype = origintype;
}

void set_rftrans_parameters( rfTransformation *rf, double val[14], int calcval[14], int defined[14])
{
    int i;
    for( i=0; i<14; i++ )
    {
        if( defined[i] ) rf->prm[i]=val[i];
        if( calcval[i] ) rf->calcPrm[i]=1;
    }
}

void set_rftrans_scale( rfTransformation *rf , double scale, int adjust )
{
    rf->prm[rfScale] = scale;
    rf->calcPrm[rfScale] = adjust;
}


void set_rftrans_rotation( rfTransformation *rf, double rot[3], int adjust[3] )
{
    int i;
    for( i= 0; i<3; i++ )
    {
        rf->prm[rfRotx+i] = rot[i];
        rf->calcPrm[rfRotx+i] = adjust[i];
    }
}


void set_rftrans_translation( rfTransformation *rf, double tran[3], int adjust[3] )
{
    int i;
    for( i= 0; i<3; i++ )
    {
        rf->prm[rfTx+i] = tran[i];
        rf->calcPrm[rfTx+i] = adjust[i];
        rf->trans[i] = tran[i];
    }
    if( rf->istopo ) premult3( (double *) rf->invtoporot, rf->prm+rfTx, rf->trans, 1 );
}

void set_rftrans_scale_rate( rfTransformation *rf , double scale, int adjust )
{
    if( scale != 0.0 || adjust ) rf->userates = 1;
    rf->prm[rfScaleRate] = scale;
    rf->calcPrm[rfScaleRate] = adjust;
}


void set_rftrans_rotation_rate( rfTransformation *rf, double rot[3], int adjust[3] )
{
    int i;
    for( i= 0; i<3; i++ )
    {
        if( rot[i] != 0.0 || adjust[i] ) rf->userates=1;
        rf->prm[rfRotxRate+i] = rot[i];
        rf->calcPrm[rfRotxRate+i] = adjust[i];
    }
}


void set_rftrans_translation_rate( rfTransformation *rf, double tran[3], int adjust[3] )
{
    int i;
    for( i= 0; i<3; i++ )
    {
        if( tran[i] != 0.0 || adjust[i] ) rf->userates=1;
        rf->prm[rfTxRate+i] = tran[i];
        rf->calcPrm[rfTxRate+i] = adjust[i];
    }
    // if( rf->istopo ) premult3( (double *) rf->invtoporot, rf->prm+rfTxRate, rf->trans, 1 );
}

void set_rftrans_origin ( rfTransformation *rf, double origin[3] )
{
    veccopy(origin,rf->origin);
    rf->localorigin = (origin[0] != 0 || origin[1] != 0 || origin[2] != 0) ? 1 : 0;
}

void flag_rftrans_used( rfTransformation *rf, int usage_type )
{
    int i;
    rf->usage |= usage_type;
    rf->prmUsed[rfScale] = 1;
    rf->prmUsed[rfScaleRate] = 1;
    for( i=0; i<3; i++ ) 
    {
        rf->prmUsed[rfRotx+i] = 1;
        rf->prmUsed[rfRotxRate+i] = 1;
    }
    if( usage_type == FRF_ABSOLUTE )
    {
        for( i=0; i<3; i++ ) 
        {
            rf->prmUsed[rfTx+i] = 1;
            rf->prmUsed[rfTxRate+i] = 1;
        }
    }
}

/* Determine whether it is OK to use an offset origin for
 * reference frame calculations
 */

static void setup_rftrans_flags( rfTransformation *rf )
{
    int calctrans;
    int calctransrate;

    /* If only used vectors rather than absolute positions then
     * then cannot calculate translations.  
     *
     * Only apply this if usage flag has been set..
     */

    if( rf->usage )
    {
        rf->usetrans = rf->usage & FRF_ABSOLUTE ? 1 : 0;
        if( ! rf->usetrans ) 
        {
            rf->calcPrm[rfTx]=rf->calcPrm[rfTy]=rf->calcPrm[rfTz]=0;
            rf->calcPrm[rfTxRate]=rf->calcPrm[rfTyRate]=rf->calcPrm[rfTzRate]=0;
        }
    }

    /* Set the calculation types */
    /* calctrans and calctransrate check if all translation parameters are calculated */

    rf->calctrans = (rf->calcPrm[rfTx] || rf->calcPrm[rfTy] || rf->calcPrm[rfTx]) ? 1 : 0;
    calctrans = (rf->calcPrm[rfTx] && rf->calcPrm[rfTy] && rf->calcPrm[rfTx]);
    rf->calcrot = (rf->calcPrm[rfRotx] || rf->calcPrm[rfRoty] || rf->calcPrm[rfRotx]) ? 1 : 0;
    rf->calcscale = rf->calcPrm[rfScale];

    rf->calctransrate = (rf->calcPrm[rfTxRate] || rf->calcPrm[rfTyRate] || rf->calcPrm[rfTxRate]) ? 1 : 0;
    calctransrate = (rf->calcPrm[rfTxRate] && rf->calcPrm[rfTyRate] && rf->calcPrm[rfTxRate]);
    rf->calcrotrate = (rf->calcPrm[rfRotxRate] || rf->calcPrm[rfRotyRate] || rf->calcPrm[rfRotxRate]) ? 1 : 0;
    rf->calcscalerate = rf->calcPrm[rfScaleRate];

    /* Determine whether we are interested in transformation rates at all */

    rf->userates=0;
    if( rf->prm[rfTxRate] != 0 ||
        rf->prm[rfTyRate] != 0 ||
        rf->prm[rfTzRate] != 0 ||
        rf->prm[rfScaleRate] != 0 ||
        rf->prm[rfRotxRate] != 0 ||
        rf->prm[rfRotxRate] != 0 ||
        rf->prm[rfRotxRate] != 0 ) rf->userates=1;
    if( rf->calctransrate || rf->calcrotrate || rf->calcscalerate ) rf->userates=1;

    /* Set flag for using offset origin in calculations */
    rf->localoriginok=1;

    /* If only using vectors then no advantage in offsetting origin */
    if( ! rf->usetrans ) rf->localoriginok=0;

    /* If the user has requested not to, then don't */
    if( rf->origintype == REFFRM_ORIGIN_ZERO ) rf->localoriginok=0;

    /* If calculating rotation and scale, but not equivalent rates
     * then can't offset origin
     */

    if( (rf->calcrot || rf->calcscale) && ! calctrans ) rf->localoriginok=0;
    if( (rf->calcrotrate || rf->calcscalerate) && ! calctransrate ) rf->localoriginok=0;

    /* If not calculating scales or rotations then no point */

    if( ! (rf->calcrot || rf->calcrotrate || rf->calcscale || rf->calcscalerate ) ) rf->localoriginok=0;
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

static void calc_tmat( rfTransformation *rf, double *prm, tmatrix tmat, tmatrix invtmat )
{
    double cs, sn;
    tmatrix mult;
    int axis;
    double angle;
    double scl;
    int i, j;

    if( !rf->istopo )
    {
        calcrotmat( 0, 1.0, 0.0, tmat );
    }
    else
    {
        memcpy(tmat,rf->toporot,sizeof(tmatrix) );
    }

    for( axis = 3; axis--; )
    {
        angle = prm[rfRotx+axis] * STOR;
        cs = cos(angle);
        sn = sin(angle);
        calcrotmat( axis, cs, sn, mult );
        premult3( DS mult, DS tmat, DS tmat, 3 );
    }

    if( rf->istopo )
    {
        premult3( DS (rf->invtoporot), DS tmat, DS tmat, 3 );
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

    scl = 1.0 + prm[rfScale] * 1.0e-6;

    for( i=0; i<3; i++ ) for( j=0 ; j<3; j++ )
    {
        tmat[i][j] *= scl;
        invtmat[i][j] /= scl;
    }
}

#define TMAT_CALC_MULT 10

void setup_rftrans( rfTransformation *rf )
{
    tmatrix mult;
    double angle, cs, sn, scl;
    int i, j, k, axis;

    setup_rftrans_flags( rf );

    for( i = 0; i < 3; i++ ) for( j = 0; j < 3; j++ )
        {
            rf->toporot[i][j] = toporot[i][j];
            rf->invtoporot[i][j] = invtoporot[i][j];
        }

    /* Calculate the translation component */

    if( !rf->istopo )
    {
        rf->trans[0] = rf->prm[rfTx];
        rf->trans[1] = rf->prm[rfTy];
        rf->trans[2] = rf->prm[rfTz];
        rf->transrate[0] = rf->prm[rfTxRate];
        rf->transrate[1] = rf->prm[rfTyRate];
        rf->transrate[2] = rf->prm[rfTzRate];
    }
    else
    {
        premult3( (double *) rf->invtoporot, rf->prm+rfTx, rf->trans, 1 );
        premult3( (double *) rf->invtoporot, rf->prm+rfTxRate, rf->transrate, 1 );
    }



    /* Calculate that transformation matrix and inverse.
     * Calculate for rates by averaging over TMAT_CALC_MULT years after ref epoch */

    calc_tmat( rf, rf->prm, rf->tmat, rf->invtmat );
    if( rf->userates )
    {
        double prm[7];
        for( i=0; i<7; i++ )
        {
            prm[i]=rf->prm[i]+rf->prm[i+7]*TMAT_CALC_MULT;
        }
        calc_tmat( rf, prm, rf->tmatrate, rf->invtmatrate );
        for( i=0; i<3; i++ )
        {
            for( j=0; j<3; j++ )
            {
                rf->tmatrate[i][j]=(rf->tmatrate[i][j]-rf->tmat[i][j])/TMAT_CALC_MULT;
                rf->invtmatrate[i][j]=(rf->invtmatrate[i][j]-rf->invtmat[i][j])/TMAT_CALC_MULT;
            }
        }
    }
        
    /*  Calc change of coords for unit change in rotations */

    if( !rf->istopo )
    {
        for( i = 0; i<3; i++ ) calcrotmat( 0, 1.0, 0.0, rf->dtmatdrot[i] );
    }
    else
    {
        for( i=0; i<3; i++ )
        {
            memcpy(rf->dtmatdrot[i],toporot,sizeof(tmatrix) );
        }
    }

    for( axis = 3; axis--; )
    {
        angle = rf->prm[rfRotx+axis] * STOR;
        cs = cos(angle);
        sn = sin(angle);

        calcrotmat( axis, cs, sn, mult );
        for( i = 0; i<3; i++ )
        {
            if( i != axis )
            {
                premult3( DS mult, DS rf->dtmatdrot[i], DS rf->dtmatdrot[i], 3 );
            }
        }
        calcdrotdang( axis, cs, sn, mult );
        premult3( DS mult, DS rf->dtmatdrot[axis], DS rf->dtmatdrot[axis], 3 );
    }

    if( rf->istopo )
    {
        for( i=0; i<3; i++ )
        {
            premult3( DS (rf->invtoporot), DS rf->dtmatdrot[i], DS rf->dtmatdrot[i], 3 );
        }
    }

    /* Apply the scale factor */

    scl = 1.0 + rf->prm[rfScale] * 1.0e-6;

    for( i=0; i<3; i++ ) for( j=0 ; j<3; j++ )
    {
        for( k=0; k<3; k++ )
        {
            rf->dtmatdrot[k][i][j] *= scl;
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

    invtoporot[0][0] = toporot[0][0] = -sln;
    invtoporot[1][0] = toporot[0][1] = cln;
    invtoporot[2][0] = toporot[0][2] = 0.0;
    invtoporot[0][1] = toporot[1][0] = -cln*slt;
    invtoporot[1][1] = toporot[1][1] = -sln*slt;
    invtoporot[2][1] = toporot[1][2] = clt;
    invtoporot[0][2] = toporot[2][0] = cln*clt;
    invtoporot[1][2] = toporot[2][1] = sln*clt;
    invtoporot[2][2] = toporot[2][2] = slt;
}


void setup_rftrans_list( double lt, double ln )
{
    int nrf;

    setup_topo_rotations( lt, ln );

    for( nrf = 0; nrf < nrftrans; nrf++ )
    {
        setup_rftrans( rflist[nrf] );
    }

    frames_setup = 1;
}

double * rftrans_tmat( rfTransformation *rf )
{
    return (double *) (rf->tmat);
}

double * rftrans_invtmat( rfTransformation *rf )
{
    return (double *) (rf->invtmat);
}

const char * rftrans_name( rfTransformation *rf )
{
    return rf->name;
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
        vecadd2( vd, 1, rf->transrate, factor, vd );
    }
    else
    {
        premult3( DS rf->invtmat, vd, vd, 1 );
        vecadd(vd, rf->trans, vd);
    }

    vecadd(vd,rf->origin,vd);
}

