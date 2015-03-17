#include "snapconfig.h"
/*
   $Log: symmatrx.c,v $
   Revision 1.3  2004/04/22 02:35:27  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:56:04  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>

#include "util/symmatrx.h"

/*------------------------------------------------------------*/
/*  Routines for cholesky decomposition, solution, and        */
/*  inversion of symmetric positive definite linear           */
/*  equations.                                                */
/*                                                            */
/*  chol_dec Decomposition of a matrix to cholesky form.      */
/*           Overwrites the matrix with its decomposition     */
/*                                                            */
/*  chol_slv Solves linear equations given decomposition      */
/*           May optionally overwrite the initial vector      */
/*           with the solution vector                         */
/*                                                            */
/*  chol_inv Computes inverse given Choleski decomposition    */
/*           Requires a working vector equal to the one row   */
/*           of the matrix                                    */
/*                                                            */
/*------------------------------------------------------------*/


#define ROW(N,i) N + ((long) (i) * ((i)+1))/2

int chol_dec( ltmat N, int np )
{
    ltmat r1, r2;
    double sum;
    int i,j,k,kmin;
    static double small = 1.0e-10;
    static double abssmall = 1.0e-30;
    for (i = 0; i<np; i++ )
    {

        /* For each row being decomposed, skip over all initial zero's in
        the row */

        r1 = ROW(N,i);
        for ( j=0; j<i; j++ )
        {
            if( *r1++ != 0.0 ) break;
        }
        kmin = j;
        for ( ; j<= i; j++ )
        {
            r1=ROW(N,i)+kmin; r2=ROW(N,j)+kmin; sum=0.0;
            for( k=kmin; k<j; k++ ) sum -= *r1++ * *r2++;
            sum += *r1;
            if ( i==j )
            {
                if( sum < *r1 * small || sum < abssmall ) return ++i;
                *r1 = sqrt( sum );
            }
            else
                *r1 = sum/ *r2;
        }
    }
    return 0;
}

void chol_slv( ltmat N, ltmat b, ltmat r, int np )
{
    ltmat r1;
    double sum;
    int i,k;
    for ( i=0, r1=N; i<np; i++ )
    {
        for (k = 0, sum=0.0; k<i; k++ ) sum -= *r1++ * r[k];
        r[i] = (b[i]+sum) / *r1++;
    }
    for ( i=np; i--; )
    {
        r1 = ROW(N,np-1) + i;
        for (k = np, sum=0.0 ; --k > i; )
        {
            sum -= *r1 * r[k];
            r1 -= k;
        }
        r[i] += sum; r[i] /= *r1;
    }
}

void chol_inv( ltmat N, ltmat tmp, int np )
{
    ltmat r0;
    double sum;
    int i,j,k;
    for (i=np; i--; )
    {
        r0 = &Lij( N, i, np-1 );
        for (j=np; j-- > i; ) { tmp[j] = *r0; r0=r0-j; }
        for (j=np; j-- > i; )
        {
            sum = (i==j) ? 1.0/tmp[i] : 0.0;
            for (k=i; ++k < np; ) sum -= tmp[k] * Lij(N,k,j);
            Lij(N,i,j) = sum/tmp[i];
        }
    }
}
#undef ROW



void print_ltmat( FILE *out, ltmat N, int nrow, const char *format, int indent )
{
    int i, j, k, cols, wid;
    const char *f;

    for( f=format; *f != 0 && (*f < '0' || *f > '9'); f++);
    if( *f )
    {
        int iwid;
        sscanf(f,"%d",&iwid);
        wid = iwid;
    }
    else
    {
        wid = 10;
    }

    cols = (80-indent)/(wid+1);
    if( cols < 4 ) cols = 4;


    for (k=0; k<nrow; k+=cols)
    {
        fprintf(out,"\n%.*s       ",indent,"");
        for( j=k; j<nrow && j<k+cols; j++) fprintf(out,"%*d ",wid,j+1);
        fprintf(out,"\n");
        for (i=k; i<nrow; i++ )
        {
            fprintf(out,"%.*s%6d ",indent,"",i+1);
            for( j=k; j<=i && j<k+cols; j++)
            {
                fprintf(out,format,Lij(N,i,j));
                fputs( " ", out);
            }
            fprintf(out,"\n");
        }
    }
}


/* This code performs a choleski decomposition with pivoting to
   bring the largest remaining diagonal term to the top.  The
   routine automatically fills any zero diagonal elements with 1
   to ensure a solution.  Requires a double precision workspace
   tmp of size nprm, and a column of integers of twice the order
   of the matrix (to hold the permutation of columns)  */




int pvt_chol_dec( ltmat N, int *col, int nprm )
{
    int *prm;
    int i, irow, imax, icol;
    double rmax, rmin, sum;
    ltmat nptr;
    int nfix;

    /* As a crude method we will cut off when the maximum diagonal
       element is less than a specified multiple of the initial maximum.
       Use some very arbitrary numbers here! */

    rmin = -1.0;
    for( i=1, nptr = N; i++ <= nprm; nptr += i )
    {
        if( *nptr > rmin ) rmin = *nptr;
    }
    rmin /= 1.0e10;
    if( rmin < 1.0e-20 ) rmin = 1.0e-20;

    /* Initiallize the pointers */

    for( i = 0; i<nprm*2; i++) col[i] = -1;
    prm = col + nprm;

    /* Main decomposition loop */

    nfix = 0;

    for( irow = 0; irow < nprm; irow++ )
    {

        /* Find the next largest column remaining */

        rmax = -1.0;
        imax = 0;
        nptr = N;
        for( i= 0, nptr = N; i<nprm; i++, nptr += i+1 )
        {
            if( prm[i] < 0 && *nptr > rmax )
            {
                rmax = *nptr;
                imax = i;
            }
        }

        /*
        imax = irow;
        rmax = Lij(N,irow,irow);
        */

        /* Set up the column pointers */

        col[irow] = imax;
        prm[imax] = irow;

        /* Calculate the diagonal element for the row */

        if( rmax > rmin )
        {
            rmax = sqrt(rmax);
        }
        else
        {
            rmax = 1.0;
            nfix++;
        }
        Lij(N,imax,imax) = rmax;

        /* Calculate the other elements of the row and apply their
        influence to the corresponding diagonal element. */

        for( icol = 0; icol < nprm; icol++ ) if( prm[icol] < 0 )
            {
                sum = Lij(N,icol,imax);
                for( i = 0; i<irow; i++ )
                {
                    sum -= Lij(N,imax,col[i]) * Lij(N,icol,col[i]);
                }
                sum /= rmax;
                Lij(N,icol,imax) = sum;
                Lij(N,icol,icol) -= sum*sum;
            }
    }

    return nfix;
}


void pvt_chol_slv( ltmat N, ltmat b, ltmat r, int *col, int np )
{
    double sum;
    int i,k,irow,icol;
    for ( i=0; i<np; i++ )
    {
        irow = col[i];
        sum = b[irow];
        for (k = 0; k<i; k++ )
        {
            icol = col[k];
            sum -= Lij(N,irow,icol) * r[icol];
        }
        r[irow] = sum / Lij(N,irow,irow);
    }
    for ( i=np; i--; )
    {
        irow = col[i];
        sum = r[irow];
        for (k = np ; --k > i; )
        {
            icol = col[k];
            sum -= Lij(N,irow,icol) * r[icol];
        }
        r[irow] = sum/Lij(N,irow,irow);
    }
}


void pvt_chol_inv( ltmat N, ltmat tmp, int *col, int np )
{
    double sum;
    int i,j,k,irow,jrow;
    for (i=np; i--; )
    {
        irow = col[i];
        for (j=np; j-- > i; ) { tmp[j] = Lij(N,irow,col[j]); }
        for (j=np; j-- > i; )
        {
            jrow = col[j];
            sum = (i==j) ? 1.0/tmp[i] : 0.0;
            for (k=i; ++k < np; ) sum -= tmp[k] * Lij(N,col[k],jrow);
            Lij(N,irow,jrow) = sum/tmp[i];
        }
    }
}

#ifdef TESTSYM

int main(int argc, char *argv[] )
{
    FILE *in, *out;
    ltmat N,b,tmp,N1,b1,p;
    int *col;
    int nprm, nmem;
    int i;
    double v;

    if( argc<2 || NULL == (in=fopen(argv[1],"r")) )
    {
        printf("Need input file as parameter\n");
        return;
    }

    out = stdout;
    if( argc>2 && NULL == (out = fopen(argv[2],"w")) )
    {
        printf("Cannot open output file\n");
        return;
    }



    fscanf(in,"%d",&nprm);
    if( nprm <= 0 )
    {
        printf("Invalid number of parameters\n");
        return;
    }

    nmem = (nprm * (nprm+1))/2 + nprm;

    b = (ltmat) malloc( (2*nmem+nprm)*sizeof(double) );
    col = (int *) malloc( 2 * nprm * sizeof(int) );
    if( !b || !col )
    {
        printf("Not enough memory");
        return;
    }

    N = b+nprm;
    b1 = b+nmem;
    N1 = b1+nprm;
    tmp = b1+nmem;

    for( i = 0; i<nmem; i++ ) { fscanf(in,"%lf",&v); b[i] = b1[i] = v; }

    fprintf(out,"Input matrix\n\n");
    print_ltmat( out, N, nprm, "%10.2lf" );
    fprintf(out,"\nInput vector\n");
    for( i=0; i<nprm; i++ ) fprintf(out,"%3d  %10.2lf\n",i+1,b[i]);

    fprintf(out,"\nWITH PIVOTING\n");
    fprintf(out,"\nCholesky decomposition status = %d\n",pvt_chol_dec(N1,col,nprm));
    pvt_chol_slv( N1, b1, b1, col, nprm );

    fprintf(out,"\nDecomposed matrix\n\n");
    print_ltmat( out, N1, nprm, "%10.2lf" );
    fprintf(out,"\nSolution vector\n");
    for( i=0; i<nprm; i++ ) fprintf(out,"%3d  %10.2lf\n",i+1,b1[i]);

    pvt_chol_inv( N1, tmp, col, nprm );
    fprintf(out,"\nInverted matrix\n\n");
    print_ltmat( out, N1, nprm, "%10.2lf" );

    fprintf(out,"\n\nWITHOUT PIVOTING\n");
    fprintf(out,"\nCholesky decomposition status = %d\n",chol_dec(N,nprm));
    chol_slv( N, b, b, nprm );

    fprintf(out,"\nDecomposed matrix\n\n");
    print_ltmat( out, N, nprm, "%10.2lf" );
    fprintf(out,"\nSolution vector\n");
    for( i=0; i<nprm; i++ ) fprintf(out,"%3d  %10.2lf\n",i+1,b[i]);

    chol_inv( N, tmp, nprm );
    fprintf(out,"\nInverted matrix\n\n");
    print_ltmat( out, N, nprm, "%10.2lf" );

    fclose(in);
    if( argc > 2) fclose(out);

}

#endif

