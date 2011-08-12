#include "snapconfig.h"
/*
   $Log: adjparam.c,v $
   Revision 1.2  2001/05/14 18:26:17  ccrook
   Minor updates

   Revision 1.1  1996/01/03 21:55:41  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>

#include "util/leastsqu.h"
#include "util/lsobseq.h"
#include "output.h"
#include "snap/snapglob.h"
#include "adjparam.h"

static char rcsid[]="$Id: adjparam.c,v 1.2 2001/05/14 18:26:17 ccrook Exp $";

void set_param_obseq( int pid, void *hA, int irow, double v )
{
    int rowno;

    rowno = param_rowno( pid );
    if( rowno ) oe_param( hA, irow, rowno, v );
}

void update_params( int get_covariance )
{
    double delta;
    int np;
    int nparam;

    nparam = param_count();

    if( !nparam ) return;

    for( np = 0; np++ < nparam;  )
    {
        int rn;
        double covar = 0.0;
        rn = param_rowno( np );
        if( rn )
        {
            rn--;   /* Convert to 0 based numbers */
            lsq_get_params( &rn, 1, &delta, get_covariance ? &covar : NULL );
            update_param_value( np, param_value(np) + delta, covar );
        }
    }
}


void list_calculated_parameters( FILE *out )
{
    int np;
    int first;
    int nparam;
    param *p;

    nparam = param_count();

    if( !nparam ) return;

    first = 1;

    for( np = 0; np++ < nparam;  )
    {
        p = param_from_id(sorted_param_id(np));
        if( !(p->flags & PRM_ADJUST) ) continue;
        if( first )
        {
            fputs("\n\nThe following parameters are also being calculated\n\n",out);
            first = 0;
        }

        fputs( p->name, out );
        if( p->identical ) fprintf(out,"  (same as %s)", param_name(p->identical) );
        fputs( "\n", out );
    }
}


void print_adjusted_parameters( FILE *out )
{
    param *p;
    int np;
    int nparam;
    double semult = 0.0;
    int first = 1;

    nparam = param_count();
    if( !nparam ) return;

    for( np = 0; np++ < nparam;  )
    {
        int pid;
        pid = sorted_param_id( np );
        p = param_from_id( pid );
        if( p->flags & PRM_LISTED ) continue;

        if( first )
        {
            first = 0;

            print_section_heading( out, "OTHER PARAMETERS");

            fprintf(lst,"\nThe errors listed for calculated parameters are %s errors\n",
                    apriori ? "apriori" : "aposteriori" );

            semult = apriori ? 1.0 : seu;

            fprintf(out,"\nParameter                          value         +/-\n");
        }

        fprintf(out,"\n%-30.30s   %11.5lf  ",p->name,p->value);
        if( param_rowno(pid) )
        {
            fprintf(out,"%11.5lf",p->covar > 0.0 ? sqrt(p->covar)*semult : 0.0 );
        }
        else
        {
            fprintf(out,"     -     ");
        }
        if( p->identical )
        {
            fprintf(out,"  = %s",param_name( p->identical ));
        }
        fprintf(out,"\n");
    }

}

