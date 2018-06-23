#include "snapconfig.h"
/*
   $Log: coefs.c,v $
   Revision 1.2  2001/01/18 00:01:43  ccrook
   Fixed error assigning value past end of coefname buffer.

   Revision 1.1  1996/01/03 21:57:37  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "adjparam.h"
#include "coefs.h"
#include "snapdata/loaddata.h"
#include "snap/snapglob.h"
#include "network/network.h"
#include "util/geodetic.h"
#include "util/pi.h"

/* The following variables must match the definition of PRM_xxxx in
   the header file */

static const char *coefprefix[] =
{
    "Refr coef ",
    "Scale error ",
    "Bearing error ",
    "Systematic: "
};

//static const char *coefname[] =
//{
//    "Refraction coefficient",
//    "Distance scale error",
//    "Bearing error",
//    "Systematic error"
//};
//
//static int coefclassid[] = {0, 0, 0, 0};

static int prefixlen[] =
{
    10,
    12,
    14,
    12
};

#define MAXPFXLEN 14
#define MAXCOEFLEN MAXPFXLEN+COEFLEN

static double default_refcoef = DEFAULT_REFCOEF;

void make_prmname( char *prmname, int type, const char *name )
{
    char *coefname;
    strcpy( prmname, coefprefix[type]);
    coefname = prmname+prefixlen[type];
    strncpy( coefname,name,COEFLEN);
    coefname[COEFLEN-1] = 0;
    _strupr( coefname );
}


static int coef_prm( int type, const char *name )
{
    int prm;
    char  prmname[MAXCOEFLEN];

    make_prmname( prmname, type, name );

    prm = find_param( prmname );
    if( !prm )
    {
        double value=0.0;
        if( type == PRM_REFCOEF && _stricmp(name,"zero") != 0 )
        {
            value=default_refcoef;
        }
        prm = define_param( prmname, value, 0 );
    }

    return prm;
}



void define_coef( int type, const char *name, double value, int adjust )
{
    int prm;
    int len;

    len = strlen(name);
    if( !len ) return;

    if( name[len-1] == '*' )
    {
        char wildcard[MAXCOEFLEN];
        make_prmname( wildcard, type, name );
        wildcard[strlen(wildcard)-1] = 0;
        wildcard_param_value( wildcard, value, adjust );
    }
    else
    {
        prm = coef_prm( type, name );
        define_param_value( prm, value, adjust );
    }
}

void define_coef_match( int type, const char *coef1, const char *coef2 )
{
    int p1, p2, len;

    p2 = coef_prm( type, coef2 );

    len = strlen(coef1);
    if( !len ) return;

    if( coef1[len-1] == '*' )
    {
        char wildcard[MAXCOEFLEN];
        make_prmname( wildcard, type, coef1);
        wildcard[strlen(wildcard)-1] = 0;
        wildcard_param_match( wildcard, p2 );
    }
    else
    {
        p1 = coef_prm( type, coef1 );
        define_param_match( p1, p2 );
    }
}


/*==============================================================*/
/* Refraction coefficient specific bits.                        */

void set_default_refcoef( double value )
{
    default_refcoef = value;
}

int refcoef_prm( const char *refcoef )
{
    return coef_prm( PRM_REFCOEF, refcoef );
}


const char *refcoef_name( int rc )
{
    return param_name(rc) + prefixlen[PRM_REFCOEF];
}


double zd_ref_correction( int p, station *st1, station *st2, void *hA, int irow )
{
    vector3 vrt1, vrt2;
    double angle;

    /* Calculate the angle subtended by the verticals at the two stations */

    rot_vertical( &st1->rTopo, vrt1 );
    rot_vertical( &st2->rTopo, vrt2 );

    angle = vecdot( vrt1, vrt2 );
    angle = 1.0 - angle*angle;
    if( angle > 0.0 ) angle = sqrt(angle);

    /* Effect on zenith distance is negative */

    angle = -angle;
    set_param_obseq( p, hA, irow, angle );

    return angle * param_value( p );
}

/*=============================================================*/
/* Distance scale err specific bits                            */

int distsf_prm( const char *distsf )
{
    return coef_prm( PRM_DISTSF, distsf );
}

const char *distsf_name( int ds )
{
    return param_name(ds) + prefixlen[PRM_DISTSF];
}


double distsf_correction( int p, double dist, void *hA, int irow )
{
    if( !p ) return 0;
    dist *= 1.0e-6;
    set_param_obseq( p, hA, irow, dist );
    return param_value( p ) * dist;
}

/*==============================================================*/
/* Azimuth bias parameter                                       */

int brngref_prm( const char *brngref )
{
    return coef_prm( PRM_BRNGREF, brngref );
}

const char *brngref_name( int br )
{
    return param_name(br) + prefixlen[PRM_BRNGREF];
}

double brngref_correction( int br, void *hA, int irow )
{
    if( !br ) return 0;
    set_param_obseq( br, hA, irow, STOR );
    return param_value( br ) * STOR;
}


/*==============================================================*/
/* Systematic error parameter                                   */

int syserr_prm( const char *syserr )
{
    return coef_prm( PRM_SYSERR, syserr );
}

const char *syserr_name( int se )
{
    return param_name(se) + prefixlen[PRM_SYSERR];
}

double syserr_correction( int se, double influence, void *hA, int irow )
{
    if( !se ) return 0;
    set_param_obseq( se, hA, irow, influence );
    return param_value( se ) * influence;
}

