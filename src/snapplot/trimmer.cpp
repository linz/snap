#include "snapconfig.h"
/* Code for clipping to a rectangular region */

/*
   $Log: trimmer.c,v $
   Revision 1.1  1996/01/03 22:32:46  CHRIS
   Initial revision

*/

#include "trimmer.h"

#define LEFT 0x01
#define RIGHT 0x02
#define TOP  0x04
#define BOTTOM 0x08

void SetTrimmerExtents( Trimmer *t, double xmin, double ymin, double xmax, double ymax )
{
    t->xmin = xmin;
    t->ymin = ymin;
    t->xmax = xmax;
    t->ymax = ymax;
    t->npt = 0;
}

static char PointFlags( Trimmer *t, double x, double y )
{
    char flags;
    if( x < t->xmin ) flags = LEFT; else if( x > t->xmax ) flags = RIGHT; else flags = 0;
    if( y < t->ymin ) flags |= BOTTOM; else if( y > t->ymax ) flags |= TOP;
    return flags;
}


/* Assume flag0 | flag1 == 0, flag0 != 0 */
static char ShiftIntoRange( Trimmer *t, double *x0, double *y0, char flag0,
                            double x1, double y1, char flag1 )
{
    double xnew, ynew;
    if( flag0 & (LEFT | RIGHT) )
    {
        xnew = (flag0 & LEFT) ? t->xmin : t->xmax;
        *y0 = ((x1 - xnew) * (*y0) + (xnew - *x0) * y1 ) / ( x1 - *x0 );
        *x0 = xnew;
        if( *y0 < t->ymin ) flag0 = BOTTOM;
        else if( *y0 > t->ymax ) flag0 = TOP;
        else flag0 = 0;
        if( !flag0 ) return 1;
        if( flag0 & flag1 ) return 0;
    }

    ynew = (flag0 & BOTTOM) ? t->ymin : t->ymax;
    *x0 = ((y1 - ynew) * (*x0) + (ynew - *y0) * x1 ) / ( y1 - *y0 );
    *y0 = ynew;
    if( *x0 < t->xmin || *x0 > t->xmax ) return 0;
    return 1;
}



int AddTrimmerPoint( Trimmer *t, double x, double y, char start )
{
    char flag;
    flag = PointFlags( t, x, y );

    /* First point on feature - ignore previous */
    if( start )
    {
        if( flag )
        {
            t->npt = 0;
        }
        else
        {
            t->npt = 1;
            t->x0 = x;
            t->y0 = y;
            t->s0 = start;
        }
    }

    /* Both points in range */
    else if( !(flag | t->flagnext) )
    {
        t->x0 = x;
        t->y0 = y;
        t->s0 = start;
        t->npt = 1;
    }

    /* Trivial rejection */
    else if( flag & t->flagnext )
    {
        t->npt = 0;
    }

    /* Start point in range - end point not */
    else if( ! t->flagnext )
    {
        t->x0 = x;
        t->y0 = y;
        ShiftIntoRange( t, &t->x0, &t->y0, flag, t->xnext, t->ynext, 0 );
        t->s0 = 0;
        t->npt = 1;
    }

    /* End point in range - start point not */
    else if( ! flag )
    {
        t->x1 = t->xnext;
        t->y1 = t->ynext;
        ShiftIntoRange( t, &t->x1, &t->y1, t->flagnext, x, y, 0 );
        t->s1 = 1;
        t->x0 = x;
        t->y0 = y;
        t->s0 = 0;
        t->npt = 2;
    }

    /* Both points out range */
    else if( ShiftIntoRange( t, &t->xnext, &t->ynext, t->flagnext, x, y, flag ))
    {
        t->x1 = t->xnext;
        t->y1 = t->ynext;
        t->s1 = 1;
        t->x0 = x;
        t->y0 = y;
        t->s0 = 0;
        ShiftIntoRange( t, &t->x0, &t->y0, flag, t->x1, t->y1, 0 );
        t->npt = 2;
    }

    else
    {
        t->npt = 0;
    }

    t->xnext = x;
    t->ynext = y;
    t->flagnext = flag;
    return t->npt;
}


char NextTrimmedPoint( Trimmer *t, double *x, double *y, char *start )
{
    switch( t->npt )
    {
    case 1: *x = t->x0; *y = t->y0; *start = t->s0; break;
    case 2: *x = t->x1; *y = t->y1; *start = t->s1; break;
    default: return 0;
    }
    t->npt--;
    return 1;
}

