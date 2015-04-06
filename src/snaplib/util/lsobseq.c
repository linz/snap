#include "snapconfig.h"
/*
   $Log: lsobseq.c,v $
   Revision 1.3  2004/04/22 02:35:43  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  2003/12/09 22:46:28  ccrook
   Fixed bug which prevents summing of sets of obs with more than 127 observations

   Revision 1.1  1996/01/03 22:00:20  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "util/lsobseq.h"
#include "util/chkalloc.h"
#include "util/errdef.h"


#define ROW_INC 5
#define COL_INC 5


/*================================================================*/


static void alloc_err( void )
{
    handle_error( MEM_ALLOC_ERROR, "Insufficient memory for program", NO_MESSAGE );
}


void *create_oe( int nprm )
{
    obseqn *A;
    A = (obseqn *) check_malloc( sizeof(obseqn));
    A->nrow = A->maxrow = A->maxcol = 0;
    A->maxelt = 0;
    A->obs = NULL;
    A->cvr = NULL;
    A->nprm = nprm;
    return (void *) A;
}


void delete_oe( void *hA )
{
    int i;
    obseqn *A;
    A = (obseqn *) hA;
    for( i = 0; i<A->maxrow; i++ )
    {
        check_free( A->obs[i]->col);
        check_free( A->obs[i]->val);
        check_free( A->obs[i]);
    }
    check_free( A->obs );
    free( A->cvr );
    check_free( A );
}


void init_oe( void *hA, int nrow, int ncol, char options )
{
    int i;
    long nelt;
    int maxrow;
    int addcol;
    obsrow **obs;
    obseqn *A;

    A = (obseqn *) hA;

    /* Free up rows that are too int */

    obs = A->obs;
    addcol = A->maxrow;
    if( ncol <= 0 ) ncol = 1;
    if( nrow <= 0 ) nrow = 1;

    if(ncol>A->maxcol)
    {
        for( i=0; i<A->maxrow; i++ )
        {
            check_free( obs[i]->col );
            check_free( obs[i]->val );
        }
        A->maxcol = ncol+COL_INC;
        addcol = 0;
    }

    /* Ensure that the array of rows is sufficiently large */

    if(nrow>A->maxrow)
    {
        maxrow = nrow + ROW_INC;
        obs = (obsrow **) check_realloc( obs, maxrow*sizeof(obsrow*) );
        for ( i=maxrow; --i>=A->maxrow; )
        {
            obs[i] = (obsrow *) check_malloc ( sizeof(obsrow) );
        }
        A->maxrow = maxrow;
        A->obs = obs;
    }

    /* Allocate memory for the rows of observations and implicit parameters */

    for (i=addcol; i<A->maxrow; i++ )
    {
        obs[i]->col = (int *) check_malloc( A->maxcol * sizeof(int) );
        obs[i]->val = (double *) check_malloc( A->maxcol * sizeof(double) );
    }

    /* Initiallize the observation equations data */

    A->nrow = nrow;
    A->flag = options;
    for (i=0; i<nrow; i++)
    {
        obs[i]->ncol = 0;
        obs[i]->flag = 0;
        obs[i]->schv = 0.0;
        obs[i]->obsv = 0.0;
    }

    /* Allocate memory for the weight matrix, and clear the matrix */

    nelt = options & OE_DIAGONAL_CVR ? nrow : ((long)nrow * (nrow+1))/2;
    if (nelt>A->maxelt)
    {
        if( A->cvr != NULL ) free( A->cvr );
        A->cvr = (ltmat) malloc ( nelt*2*sizeof( double ));
        if( A->cvr == NULL ) alloc_err();
        A->maxelt = nelt;
    }
    A->wgt = A->cvr + nelt;
    for( ; nelt--; ) A->cvr[nelt] = 0.0;
}


/* All other initiallizations defined as macros */

void oe_add_param( obseqn *A, int irow, int c, double v )
{
    obsrow *obsr;
    int i;
    int *cl;
    obsr = A->obs[irow-1];
    cl = obsr->col;
    c = c-1;
    for( i=0; i<obsr->ncol; i++, cl++) if (*cl==c) { obsr->val[i]+=v; return;}
    *cl=c; obsr->val[i]=v; obsr->ncol = ++i;
}


void oe_covar( void *hA, int irow, int icol, double v )
{
    obseqn *A;
    long ielt;

    A = (obseqn *) hA;
    irow--;
    icol--;
    if( A->flag & OE_DIAGONAL_CVR )
    {
        if( irow == icol ) A->cvr[irow] = v;
    }
    else
    {
        if( irow > icol )
        {
            ielt = ((long)irow * (irow+1))/2 + icol;
        }
        else
        {
            ielt = ((long)icol * (icol+1))/2 + irow;
        }
        A->cvr[ielt] = v;
    }
}

void print_obseqn( FILE *out, void *hA )
{
    obseqn *A;
    int i, j;
    obsrow *obs;
    ltmat wgt;
    double w;

    A = (obseqn *) hA;

    wgt = A->cvr;
    for( i = 0; i++<A->nrow; )
    {
        obs = A->obs[i-1];
        if( obs->flag & OE_UNUSED ) fputs("*",out); else fputs(" ",out);
        fprintf(out,"%-2d ",(int) i);
        fprintf(out,"%12.5le ",obs->obsv);
        if( A->flag & OE_SCHREIBER )
        {
            fprintf(out,"%12s ","SCH");
        }
        for( j = 0; j< obs->ncol; j++ )
        {
            fprintf(out,"%12d ",(int) (obs->col[j]+1));
        }
        fprintf(out,"\n    ");
        w = *wgt;
        if( w > 0.0 ) w = sqrt(w);
        fprintf(out,"%12.5le ",w);
        wgt += A->flag & OE_DIAGONAL_CVR ? 1 : i;
        if( A->flag & OE_SCHREIBER )
        {
            fprintf(out,"%12.5le ",obs->schv);
        }
        for( j = 0; j < obs->ncol; j++ )
        {
            fprintf(out,"%12.5le ",obs->val[j]);
        }
        fprintf(out,"\n\n");
    }

    if( !(A->flag & OE_DIAGONAL_CVR ) )
    {
        print_ltmat( out, A->cvr, A->nrow, "%12.5le", 0 );
        fprintf(out,"\n");
    }
}

void print_obseqn_json( FILE *out, void *hA, const char *srcjson )
{
    obseqn *A;
    int i, j;
    obsrow *obs;
    ltmat cvr;
    int printcvr;

    A = (obseqn *) hA;
    cvr = A->cvr;

    printcvr = !(A->flag & OE_DIAGONAL_CVR );

    fprintf( out, "{\n" );
    if( srcjson )
    {
        fprintf( out, "  \"source\": %s,\n",srcjson );
    }
    fprintf( out, "  \"nobs\": %d,\n", A->nrow );
    fprintf( out, "  \"obs\": [\n");
    for( i = 0; i++<A->nrow; )
    {
        obs = A->obs[i-1];
        fprintf( out, "   {\n");
        fprintf(out,"    \"value\": %15.8le,\n",obs->obsv);
        if( A->flag & OE_SCHREIBER )
        {
            fprintf(out,"    \"schreiber\": %15.8le,\n",obs->schv);
        }
        fprintf(out,"    \"useobs\": %s,\n",obs->flag & OE_UNUSED ? "false" : "true" );
        fprintf(out,"    \"ncolumns\": %d,\n",obs->ncol);
        fprintf(out,"    \"columns\": [");
        for( j = 0; j< obs->ncol; j++ )
        {
            fprintf(out,"%s%d",j ? ", " : "",(int) (obs->col[j]+1));
        }
        fprintf(out,"],\n");
        fprintf(out,"    \"values\": [");
        for( j = 0; j< obs->ncol; j++ )
        {
            fprintf(out,"%s%15.8le",j ? ", " : "",(obs->val[j]));
        }
        fprintf(out,"]\n");
        fprintf( out, "   }%s\n", i < A->nrow ? "," : "");
    }
    fprintf( out, "  ],\n");
    if (A->flag & OE_DIAGONAL_CVR )
    {
        int ir;
        fprintf( out, "  \"cvrdiag\": [\n" );
        for( ir=0; ir < A->nrow; ir++ )
        {
            fprintf(out,"%s%15.8le",
                    ir == 0 ? "" :
                    ir %10 == 0 ? ",\n    " :
                    ",", cvr[ir]);
        }
        fprintf( out, "\n  ]\n");
    }
    else
    {
        int ir, ic;
        fprintf( out, "  \"cvr\": [" );
        for( ir=0; ir < A->nrow; ir++ )
        {
            fprintf( out, "%s\n    [",ir ? "," : "" );
            for( ic=0; ic < A->nrow; ic++ )
            {
                fprintf(out,"%s%15.8le",
                        ic == 0 ? "" :
                        ic %10 == 0 ? ",\n    " :
                        ",", Lij(cvr,ir,ic));
            }
            fprintf( out, "]");
        }
        fprintf( out, "\n  ]\n");
    }
    fprintf( out, "}\n" );
}
