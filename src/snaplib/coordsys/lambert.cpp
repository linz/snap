#include "snapconfig.h"
/* Lambert conical conformal projection code:
   Written by Chris Crook.
   Department of Survey and Land Information, New Zealand
   30 April 1993
   */


/* Formulae for Lamberts conic conformal projection are based upon
   "Map Projections - a working manual"
   John P Snyder
   US Geological Survey Professional Paper 1395

   Pages 104-109 */

/*
   $Log: lambert.c,v $
   Revision 1.1  1995/12/22 16:55:15  CHRIS
   Initial revision

*/

#include <math.h>
#include "coordsys/lambert.h"
#include "util/pi.h"

/* Defines PI (from Abramowitz and Stegun Table 1.1) */

#define PI 3.1415926535898

#define MAX_IT 10
#define TOLERANCE 1.0e-9


static double calc_m( double e, double lt )
{
    double es;
    es = e*sin(lt);
    return cos(lt)/sqrt(1-es*es);
}

static double calc_t( double e, double lt )
{
    double s, es;
    s = sin(lt);
    es = e * s;
    return sqrt((1-s) * pow( (1+es)/(1-es) , e) / ( 1 + s ));
}

void defineLCCProjection( LCCProjection *lp,
                          double a, double rf,
                          double sp1, double sp2,
                          double lt0, double ln0,
                          double e0, double n0 )
{

    double m1, m2, t1, t2;
    double e;
    int rev;

    e = rf == 0.0 ? 0.0 : 1.0/rf;
    e = sqrt(2*e - e*e);

    lp->a = a;
    lp->rf = rf;
    lp->e = e;
    lp->sp1 = sp1;
    lp->sp2 = sp2;
    lp->lt0 = lt0;
    lp->ln0 = ln0;
    lp->e0 = e0;
    lp->n0 = n0;

    rev = (sp1+sp2) >= 0 ? 1 : -1;
    lp->rev = rev;
    sp1 *= rev;
    sp2 *= rev;
    lt0 *= rev;
    if( rev < 0 ) { double tmp = sp1; sp1 = sp2; sp2 = tmp; }

    m1 = calc_m( e, sp1);
    m2 = calc_m( e, sp2);
    t1 = calc_t( e, sp1);
    t2 = calc_t( e, sp2);
    if( fabs(sp1-sp2) > 1.0e-5 )
    {
        lp->n = (log(m1)-log(m2))/(log(t1)-log(t2));
    }
    else
    {
        lp->n = sin(lp->sp1);
    }
    lp->F = m1/(lp->n * pow(t1,lp->n));
    lp->r0 = a*lp->F*pow(calc_t(e,lt0),lp->n);
}

void convertGeogToLCC( LCCProjection *lp,
                       double ln, double lt,
                       double *ce, double *cn,
                       double *sf, double *cnv )
{

    double r, th;

    lt = lt * lp->rev;
    r = lp->a*lp->F*pow(calc_t(lp->e, lt) , lp->n);
    th = ln-lp->ln0;
    while ( th < -PI ) th += TWOPI;
    while ( th > PI ) th -= TWOPI;
    th *= lp->n;
    *ce = r * sin(th) + lp->e0;
    *cn = lp->rev*(lp->r0 - r * cos(th)) + lp->n0;

    if( sf )
    {
        double es;
        es = lp->e * sin(lt);
        *sf = lp->n*r*sqrt(1-es*es)/(lp->a*cos(lt));
    }
    if( cnv )
    {
        *cnv = -th*lp->rev;
    }
}


void convertLCCToGeog( LCCProjection *lp,
                       double ce, double cn, double *ln, double *lt )
{

    double r, t;
    double lt0, lt1;
    int it;
    double es;

    ce -= lp->e0;
    cn = lp->r0 - (lp->rev)*(cn - lp->n0);

    r = _hypot( ce, cn );
    if( lp->n < 0 ) r = -r;
    t = pow( r / (lp->a * lp->F), 1.0/lp->n);

    lt0 = PI/2.0 - 2.0*atan(t);

    it = 0;
    do
    {
        es = lp->e * sin(lt0);
        lt1 = PI/2 - 2.0*atan( t * pow( (1-es)/(1+es), lp->e/2 ) );
        if( fabs(lt0-lt1) < TOLERANCE ) break;
        lt0 = lt1;
    }
    while( it++ < MAX_IT );

#ifdef TESTLC
    printf("convertLCCToGeog: %d iterations\n",it);
#endif

    *lt = lt1*lp->rev;

    es = lp->n > 0 ? 1 : -1;
    *ln = atan2( es * ce, es * cn )/lp->n + lp->ln0;
    while( *ln > PI ) *ln -= TWOPI;
    while( *ln < -PI ) *ln += TWOPI;
}



#ifdef TESTLC

#define RTOD (180.0/PI)
#define DTOR (PI/180.0)

int main( int argc, char *argv[] )
{
    double a, f;
    double sp1, sp2;
    double lt0, ln0;
    double e0, n0;
    LCCProjection lp;
    char inrec[256];
    int gotProj;
    double e, n;
    double lt, ln;
    double sf, cnv;
    FILE *in, *out;

    if( argc < 3 )
    {
        printf("\nNeed input and output file names as parameters\n");
        return 0;
    }

    if( NULL == (in = fopen(argv[1],"r") ))
    {
        printf("Cannot open input file %s\n",argv[2]);
        return 0;
    }

    if( NULL == (out = fopen(argv[2],"w") ))
    {
        printf("Cannot open output file %s\n",argv[3]);
        return 0;
    }

    a = 6378388.0;
    f = 297.0;
    gotProj = 0;

    while( fgets( inrec, 256, in ) )
    {
        switch( inrec[0] )
        {

        case 'E':
        case 'e':
            if( sscanf(inrec+1,"%lf%lf",&a,&f) != 2 )
            {
                printf("Error reading ellipsoid definition\n");
                return 0;
            }
            break;

        case 'P':
        case 'p':
            if( sscanf(inrec+1,"%lf%lf%lf%lf%lf%lf",&sp1,&sp2,&lt0,&ln0,&e0,&n0)
                    != 6 )
            {
                printf("Error reading projection definition\n");
                return 0;
            }

            e = sqrt( 2.0/f - 1.0/(f*f));
            setupLCCProjection( &lp, a, e, DTOR*sp1, DTOR*sp2,
                                DTOR*lt0, DTOR*ln0, e0, n0 );
            gotProj = 1;
            fprintf(out,"\nL Conformal Conic Projection\n");
            fprintf(out,"  Ellipsoid  a = %.3lf  1/f = %.6lf  e = %.9lf\n",a,f,e);
            fprintf(out,"  Standard parallels:  %.6lf   %.6lf\n",sp1,sp2);
            fprintf(out,"  Origin:       lat = %.6lf  long = %.6lf\n",lt0,ln0);
            fprintf(out,"  False origin: e   = %.3lf     n = %.3lf\n",e0,n0);

            break;

        case 'T':
        case 't':

            if( !gotProj )
            {
                fprintf(out,"Need projection definition before conversion\n");
                break;
            }

            if( sscanf(inrec+1,"%lf%lf",&lt, &ln ) != 2 )
            {
                fprintf(out,"Invalid coordinate definition\n");
                break;
            }

            fprintf(out,"\nInput geodetic:  %14.8lf %14.8lf\n",lt,ln);
            lt *= DTOR;
            ln *= DTOR;
            convertGeogToLCC( &lp, ln, lt, &e, &n, &sf, &cnv );
            sf = (sf - 1.0) * 1e6;
            cnv *= RTOD*3600;
            fprintf(out,  "Projection:      %14.3lf %14.3lf  sf = %.0lfppm conv = %.0lfsecs\n",
                    e,n,sf,cnv);
            convertLCCToGeog( &lp, e, n, &ln, &lt );
            lt *= RTOD;
            ln *= RTOD;
            fprintf(out,  "Geodetic:        %14.8lf %14.8lf\n",lt,ln);
            break;

        case 'F':
        case 'f':

            if( !gotProj )
            {
                fprintf(out,"Need projection definition before conversion\n");
                break;
            }

            if( sscanf(inrec+1,"%lf%lf",&e, &n ) != 2 )
            {
                fprintf(out,"Invalid coordinate definition\n");
                break;
            }

            fprintf(out,"\nInput projctn:   %14.8lf %14.8lf\n",e,n);
            convertLCCToGeog( &lp, e, n, &ln, &lt );
            lt *= RTOD;
            ln *= RTOD;
            fprintf(out,  "Geodetic:        %14.8lf %14.8lf\n",lt,ln);
            lt *= DTOR;
            ln *= DTOR;
            convertGeogToLCC( &lp, ln, lt, &e, &n, &sf, &cnv );
            sf = (sf - 1.0) * 1e6;
            cnv *= RTOD*3600;
            fprintf(out,  "Projection:      %14.3lf %14.3lf sf = %.0lfppm conv = %.0lfsecs\n",
                    e,n,sf,cnv);
            break;
        }
    }
    return 0;
}


#endif
