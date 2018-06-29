#ifndef TRIMMER_H
#define TRIMMER_H

/*
   $Log: trimmer.h,v $
   Revision 1.1  1996/01/03 22:33:11  CHRIS
   Initial revision

*/

/* Object for trimming lines to a rectangular boundary */

typedef struct
{
    double xmin, xmax, ymin, ymax;
    double x0, y0, x1, y1, xnext, ynext;
    char flagnext;
    char s0, s1, npt;
} Trimmer;

void SetTrimmerExtents( Trimmer *t, double xmin, double ymin, double xmax, double ymax );
int AddTrimmerPoint( Trimmer *t, double x, double y, char start );
char NextTrimmedPoint( Trimmer *t, double *x, double *y, char *start );

#define GotTrimmedPoint(t) ((t)->npt)

#endif
