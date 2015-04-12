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

static int create_rftrans( const char *name, int topocentric )
{
    rfTransformation *rf;
    int i;

    rf = new_rftrans();

    rf->name = copy_string( name );
    _strupr( rf->name );
    rf->userates=0
    rf->istrans = 0;
    rf->isorigin = 0;
    rf->istopo = topocentric ? 1 : 0;
    rf->calctrans = 0;
    rf->calcrot = 0;
    rf->calcscale = 0;
    rf->calctransrate = 0;
    rf->calcrotrate = 0;
    rf->calcscalerate = 0;

    rf->origin[0] = 0.0;
    rf->origin[1] = 0.0;
    rf->origin[2] = 0.0;

    rf->trans[0] = 0.0;
    rf->trans[1] = 0.0;
    rf->trans[2] = 0.0;

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


int get_rftrans( const char *name )
{
    int rf;

    rf = find_rftrans( name );
    if( !rf ) rf = create_rftrans( name, 0 );
    return rf;
}

int rftrans_count( void )
{
    return nrftrans;
}

rfTransformation *rftrans_from_id( int id )
{
    return id ? rflist[id-1] : NULL;
}

int get_topocentric_rftrans( const char *name )
{
    int rf;

    rf = find_rftrans( name );
    if( !rf ) rf = create_rftrans( name, 1 );
    return rf;
}


int rftrans_topocentric( int rf )
{
    return rflist[rf-1]->istopo;
}

void set_rftrans_scale( int rf , double scale, int adjust )
{
    rfTransformation *rfp;
    rfp = rflist[rf-1];
    rfp->prm[rfScale] = scale;
    rfp->calcPrm[rfScale] = adjust;
}


void set_rftrans_rotation( int rf, double rot[3], int adjust[3] )
{
    rfTransformation *rfp;
    int i;

    rfp = rflist[rf-1];

    for( i= 0; i<3; i++ )
    {
        rfp->prm[rfRotx+i] = rot[i];
        rfp->calcPrm[rfRotx+i] = adjust[i];
    }

}


void set_rftrans_translation( int rf, double tran[3], int adjust[3] )
{
    rfTransformation *rfp;
    int i;

    rfp = rflist[rf-1];

    for( i= 0; i<3; i++ )
    {
        rfp->prm[rfTx+i] = tran[i];
        rfp->calcPrm[rfTx+i] = adjust[i];
        rfp->trans[i] = tran[i];
    }

    if( rfp->istopo ) premult3( (double *) rfp->invtoporot, rfp->prm+rfTx, rfp->trans, 1 );

}

void set_rftrans_scalerate( int rf , double scale, int adjust )
{
    rfTransformation *rfp;
    rfp = rflist[rf-1];
    if( scale != 0.0 || adjust ) rfp->userates = 1;
    rfp->prm[rfScaleRate] = scale;
    rfp->calcPrm[rfScaleRate] = adjust;
}


void set_rftrans_rotationrate( int rf, double rot[3], int adjust[3] )
{
    rfTransformation *rfp;
    int i;

    rfp = rflist[rf-1];
    rfp->userates = 1;

    for( i= 0; i<3; i++ )
    {
        if( rot[i] != 0.0 || adjust[i] ) rfp->userates=1;
        rfp->prm[rfRotxRate+i] = rot[i];
        rfp->calcPrm[rfRotxRate+i] = adjust[i];
    }

}


void set_rftrans_translationrate( int rf, double tran[3], int adjust[3] )
{
    rfTransformation *rfp;
    int i;

    rfp = rflist[rf-1];

    for( i= 0; i<3; i++ )
    {
        if( tran[i] != 0.0 || adjust[i] ) rfp->userates=1;
        rfp->prm[rfTxRate+i] = tran[i];
        rfp->calcPrm[rfTxRate+i] = adjust[i];
    }

    // if( rfp->istopo ) premult3( (double *) rfp->invtoporot, rfp->prm+rfTxRate, rfp->trans, 1 );

}

void set_rftrans_origin ( rfTransformation *rfp, double origin[3] )
{
    veccopy(origin,rfp->origin);
}

void flag_rftrans_used( int rf, int usage_type )
{
    rfTransformation *rfp;
    int i;
    rfp = rflist[rf-1];
    rfp->prmUsed[rfScale] = 1;
    for( i=0; i<3; i++ ) rfp->prmUsed[rfRotx+i] = 1;
    if( usage_type == FRF_ABSOLUTE )
    {
        rfp->istrans = 1;
        for( i=0; i<3; i++ ) rfp->prmUsed[rfTx+i] = 1;
    }
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

void setup_rftrans( rfTransformation *rf )
{
    tmatrix mult;
    double angle, cs, sn, scl;
    int i, j, k, axis;
    tmatrix temp;


    for( i = 0; i < 3; i++ ) for( j = 0; j < 3; j++ )
        {
            rf->toporot[i][j] = toporot[i][j];
            rf->invtoporot[i][j] = invtoporot[i][j];
        }

    /*  Calc all the matrices - first set them to identity matrices,
        then premultiply by each rotation component in turn  */

    if( !rf->istopo )
    {
        calcrotmat( 0, 1.0, 0.0, rf->tmat );
        for( i = 0; i<3; i++ ) calcrotmat( 0, 1.0, 0.0, rf->dtmatdrot[i] );
        rf->trans[0] = rf->prm[rfTx];
        rf->trans[1] = rf->prm[rfTy];
        rf->trans[2] = rf->prm[rfTz];
    }
    else
    {
        memcpy(rf->tmat,toporot,sizeof(tmatrix) );
        for( i=0; i<3; i++ )
        {
            memcpy(rf->dtmatdrot[i],toporot,sizeof(tmatrix) );
        }

        premult3( (double *) rf->invtoporot, rf->prm+rfTx, rf->trans, 1 );
    }

    for( axis = 3; axis--; )
    {
        angle = rf->prm[rfRotx+axis] * STOR;
        cs = cos(angle);
        sn = sin(angle);

        calcrotmat( axis, cs, sn, mult );

        premult3( DS mult, DS rf->tmat, DS rf->tmat, 3 );
        for( i = 0; i<3; i++ )
            if( i != axis )
                premult3( DS mult, DS rf->dtmatdrot[i], DS rf->dtmatdrot[i], 3 );

        calcdrotdang( axis, cs, sn, mult );
        premult3( DS mult, DS rf->dtmatdrot[axis], DS rf->dtmatdrot[axis], 3 );

        calcrotmat( axis, cs, -sn, mult );
        memcpy( temp, rf->invtmat, sizeof( tmatrix ) );
        premult3( DS temp, DS mult, DS rf->invtmat, 3 );
    }

    if( rf->istopo )
    {
        premult3( DS invtoporot, DS rf->tmat, DS rf->tmat, 3 );
        for( i=0; i<3; i++ )
        {
            premult3( DS invtoporot, DS rf->dtmatdrot[i], DS rf->dtmatdrot[i], 3 );
        }
    }

    rf->invtmat[0][0] = rf->tmat[0][0];
    rf->invtmat[0][1] = rf->tmat[1][0];
    rf->invtmat[0][2] = rf->tmat[2][0];
    rf->invtmat[1][0] = rf->tmat[0][1];
    rf->invtmat[1][1] = rf->tmat[1][1];
    rf->invtmat[1][2] = rf->tmat[2][1];
    rf->invtmat[2][0] = rf->tmat[0][2];
    rf->invtmat[2][1] = rf->tmat[1][2];
    rf->invtmat[2][2] = rf->tmat[2][2];

    /* Apply the scale factor */

    scl = 1.0 + rf->prm[rfScale] * 1.0e-6;

    for( i=0; i<3; i++ ) for( j=0 ; j<3; j++ )
        {
            rf->tmat[i][j] *= scl;
            rf->invtmat[i][j] /= scl;
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

double * rftrans_tmat( int rf )
{
    return (double *) (rflist[rf-1]->tmat);
}

double * rftrans_invtmat( int rf )
{
    return (double *) (rflist[rf-1]->invtmat);
}

const char * rftrans_name( int rf )
{
    return rflist[rf-1]->name;
}

void rftrans_correct_vector( int nrf, double vd[3] )
{
    rfTransformation *rf;
    rf = rflist[nrf-1];
    premult3( DS rf->invtmat, vd, vd, 1 );
}

void rftrans_correct_point( int nrf, double vd[3] )
{
    rfTransformation *rf;
    rf = rflist[nrf-1];
    vecdif(vd, rf->origin,vd);
    premult3( DS rf->invtmat, vd, vd, 1 );
    vecadd(vd, rf->trans, vd);
    vecadd(vd,rf->origin,vd);
}




