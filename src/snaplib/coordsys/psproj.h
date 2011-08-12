#ifndef PSPROJ_H
#define PSPROJ_H

/*
   $Log: psproj.h,v $
   Revision 1.1  1995/12/22 17:00:02  CHRIS
   Initial revision

*/

#ifndef PSPROJ_H_RCSID
#define PSPROJ_H_RCSID "$Id: psproj.h,v 1.1 1995/12/22 17:00:02 CHRIS Exp $"
#endif

/* Header file for the Polar Stereographic projection */

typedef struct
{
    double cm;         /* Central meridian */
    double sf;         /* Central meridian scale factor */
    double fe, fn;     /* False origin */
    char south;        /* TRUE for south pole projection */
    double k;     /* Not sure what this is yet */
    double a;          /* Some bits of the ellipsoid */
    double rf;
    double e;
    double e2;
} PSProjection;

void define_PSProjection( PSProjection *psp, double a, double rf,
                          double cm, double sf, double fe, double fn, char south );
void psp_geod( PSProjection *psp, double ce, double cn, double *sphi, double *slam );
void geod_psp(PSProjection *psp, double sphi, double slam, double *cee, double *cnn);

#endif
