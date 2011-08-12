#include "snapconfig.h"
/*
   $Log: nzmg.c,v $
   Revision 1.1  1995/12/22 16:56:53  CHRIS
   Initial revision

*/

#include "util/pi.h"
#include "coordsys/nzmg.h"

static char rcsid[]="$Id: nzmg.c,v 1.1 1995/12/22 16:56:53 CHRIS Exp $";

/*
  Coefficients for converting NZMG coordinates to longitude and latitude
  From L+S technical note set 1973/32
*/

typedef struct { double real, imag; } complex;

static double a =  6378388.0;
static double n0 = 6023150.0;
static double e0 = 2510000.0;
static double lt0 = -41.0;
static double ln0 = 173.0;

static double rad2deg = 180/PI;

static double cfi[] = {   0.6399175073,
                          -0.1358797613,
                          0.063294409,
                          -0.02526853,
                          0.0117879,
                          -0.0055161,
                          0.0026906,
                          -0.001333,
                          0.00067,
                          -0.00034
                      };

static double cfl[] = {   1.5627014243,
                          0.5185406398,
                          -0.03333098,
                          -0.1052906,
                          -0.0368594,
                          0.007317,
                          0.01220,
                          0.00394,
                          -0.0013
                      };

static complex cfb1[] = { {0.7557853228,           0.0},
    { 0.249204646,   0.003371507},
    {-0.001541739,   0.041058560},
    { -0.10162907,    0.01727609},
    { -0.26623489,   -0.36249218},
    {  -0.6870983,    -1.1651967}
};

static complex cfb2[] = { {1.3231270439,           0.0},
    {-0.577245789,  -0.007809598},
    { 0.508307513,  -0.112208952},
    { -0.15094762,    0.18200602},
    {  1.01418179,    1.64497696},
    {   1.9660549,     2.5127645}
};

/*----------------------------------------------------------------*/
/*                                                                */
/*  Basic complex arithmetic routines                             */
/*                                                                */
/*----------------------------------------------------------------*/

static complex *cadd(complex *cr, complex *c1, complex *c2)
{
    cr->real = c1->real + c2->real;
    cr->imag = c1->imag + c2->imag;
    return cr;
}

static complex *cmult(complex *cr, complex *c1, complex *c2)
{
    complex temp;
    temp.real = c1->real * c2->real - c1->imag * c2->imag;
    temp.imag = c1->real * c2->imag + c1->imag * c2->real;
    cr->real = temp.real;
    cr->imag = temp.imag;
    return cr;
}

static complex *cdiv(complex *cr, complex *c1, complex *c2)
{
    complex temp;
    double cmod2;
    cmod2 = (c2->real*c2->real + c2->imag*c2->imag);
    temp.real = c2->real/cmod2;
    temp.imag = -c2->imag/cmod2;
    cmult( cr, c1, &temp );
    return cr;
}

static complex *cscale(complex *cr, complex *c1, double sc)
{
    cr->real = c1->real * sc;
    cr->imag = c1->imag * sc;
    return cr;
}

/*----------------------------------------------------------------*/
/*                                                                */
/*  Routines to do the conversions to and from NZMG               */
/*                                                                */
/*----------------------------------------------------------------*/

void nzmg_geod( double e, double n, double *ln, double *lt )
{
    complex z0, z1, zn, zd, tmp1, tmp2;
    double sum,tmp;
    int i, it;

    z0.real = (n-n0)/a;     z0.imag = (e-e0)/a;
    z1.real = cfb2[5].real; z1.imag = cfb2[5].imag;
    for (i=5; i--; ) cadd(&z1, cmult(&z1, &z1, &z0), cfb2+i );
    cmult(&z1,&z1,&z0);

    for(it=2; it--; )
    {
        cscale( &zn, cfb1+5, 5.0);
        cscale( &zd, cfb1+5, 6.0);
        for (i=4; i; i--)
        {
            cadd( &zn, cmult(&tmp1, &zn, &z1), cscale(&tmp2, cfb1+i, (double) i));
            cadd( &zd, cmult(&tmp1, &zd, &z1), cscale(&tmp2, cfb1+i, (double) (i+1)));
        }
        cadd( &zn, &z0, cmult( &zn, cmult( &zn, &zn, &z1), &z1));
        cadd( &zd, cfb1, cmult( &zd, &zd, &z1 ));
        cdiv( &z1, &zn, &zd );
    }

    *ln = ln0/rad2deg + z1.imag;

    tmp = z1.real;
    sum = cfl[8];
    for (i=8; i--;) sum = sum*tmp + cfl[i];
    sum *= tmp/3600.0e-5;
    *lt = (lt0+sum)/rad2deg;
}


void geod_nzmg( double ln, double lt, double *e, double *n )
{
    double sum;
    int i;
    complex z0,z1;

    lt = (lt*rad2deg - lt0) * 3600.0e-5;
    sum = cfi[9];;
    for (i = 9; i--;) sum = sum*lt+cfi[i];
    sum *= lt;

    z1.real = sum; z1.imag = ln-ln0/rad2deg;
    z0.real = cfb1[5].real; z0.imag = cfb1[5].imag;
    for ( i=5; i--;) cadd(&z0,cmult(&z0,&z0,&z1),cfb1+i);
    cmult(&z0,&z0,&z1);

    *n = n0+z0.real*a;
    *e = e0+z0.imag*a;
}
