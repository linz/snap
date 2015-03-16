#include "snapconfig.h"
/* Cumulative probability functions and inverse functions based on
   formulae in Abramowitz and Stegun */

/*
   $Log: probfunc.c,v $
   Revision 1.3  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  1998/06/15 02:23:02  ccrook
   Modified to handle "long" parameters.  However should really do this again to use more efficient approximations when the parameters are large.

   Revision 1.1  1995/12/22 19:54:07  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include "util/probfunc.h"

#ifdef __BORLANDC__
// #pragma warn -par
#endif

#ifdef __TSC__
// #pragma warn(wpnu=>off)
#endif

static double inverse_distribution( double prob,
                                    double (*distfunc)( double value, long prm1, long prm2 ),
                                    long prm1, long prm2 );


double normal_distn( double value )
{
    int i;
    char negative;
    double d[] = {1.0,0.0498673470,0.0211410061,0.0032776263,0.0000380036,
                  0.0000488906,0.0000053830
                 };
    double prob;

    negative = value < 0.0;
    if (negative) value = -value;

    prob = d[6];
    for( i=6; i--; ) prob = prob*value+d[i];

    prob = 0.5*pow(prob,-16.0);
    if( negative ) prob = 1.0-prob;

    return prob;
}

// #pragma warning (disable : 4100)

static double ndf2( double value, long dum1, long dum2 )
{
    return normal_distn( value );
}

double inv_normal_distn( double prob )
{
    char negative;
    double value;
    negative = prob > 0.5;
    if( negative ) prob = 1.0 - prob;
    value = inverse_distribution( prob, ndf2, 0, 0 );
    if( negative ) value = -value;
    return value;
}



/* Chi squared is based upon equations 26.4.4 and 26.4.5 */

double chi2_distn( double value, long dof )
{
    long odd;
    double prob;
    double t, x;
    long i;

    /* Invalid conditions */

    if( dof < 0 ) return 0.0;
    if( value < 0.0 ) return 0.0;

    odd = dof % 2;

    t = exp( -value/2.0 );
    x = sqrt( value );
    prob = 0;

    if( odd ) t *= x * 0.797884561;

    for( i=odd; i<dof; i+=2 ) { prob += t; t *= value/(i+2); }

    if( odd ) prob += 2.0 * normal_distn( x );

    return prob;
}


static double cdf2( double value, long dof, long dum )
{
    return chi2_distn( value, dof );
}

double inv_chi2_distn( double prob, long dof )
{
    return inverse_distribution( prob, cdf2, dof, 0 );
}


double students_t_distn( double value, long dof )
{
    double prob;

    prob = f_distn( value*value, 1, dof )/2.0;
    if( value < 0.0 ) prob = 1.0-prob;

    return prob;
}


double inv_students_t_distn( double prob, long dof )
{
    char negative;
    double value;

    negative = prob > 0.5;
    if( negative ) prob = 1.0 - prob;
    prob += prob;
    value = inverse_distribution( prob, f_distn, 1, dof );
    if( value > 0.0 ) value = sqrt(value);
    if( negative ) value = -value;
    return value;
}


/* F distribution is based upon equations 26.6.4, 26.6.5, and 26.6.8 */


double f_distn( double value, long dofn, long dofd )
{

    double prob;
    double twobypi = 0.636619772;
    long nodd,dodd,swap,i;
    double ta, cs, sn, term, fact, x, t, num, den;

    prob = 0.0;

    if( dofn <= 0 || dofd < 0 ) return 0.0;
    if( dofd == 0 )  return chi2_distn( value*dofn, dofn );
    if( value<=0.0) return 0.0;

    /* Different algorithms are used depending on which of dofn and dofd
       are even.  If one is odd, then if it is not the first the swap
       values */

    nodd = dofn % 2;
    dodd = dofd % 2;

    if( nodd && dodd )
    {
        ta = sqrt( dofn * value / dofd );
        cs = 1.0 / sqrt( 1.0 + ta*ta );
        sn = ta * cs;
        prob = 1.0 - twobypi * atan( ta );
        term = sn * cs * twobypi;
        fact = 1.0;

        for( i = 1; i <= dofd-2; i+= 2 )
        {
            prob -= term;
            term *= (i+1)*cs*cs/(i+2);
            fact *= (double) (i+1) / (double) i;
        }

        term = twobypi * fact * sn * pow(cs,(double) dofd);
        for( i=1; i<=dofn-2; i+=2 )
        {
            prob += term;
            term *= (dofd+i)*sn*sn/(i+2);
        }
    }


    /* If one is even then ensure that the numerator is even.  If both are
       even, put the smallest in the numerator */

    else
    {

        value = dofd/(dofd + dofn * value );
        swap = nodd || (!dodd && dofd < dofn);
        if( swap ) { i = dofd; dofd = dofn; dofn = i; value = 1.0-value; }

        x = 1.0-value;
        num = dofd;
        den = 2.0;
        t = 1.0;
        for( i = 0; i <= dofn-2; i+= 2 )
        {
            prob += t;
            t *= num*x/den;
            num += 2;
            den += 2;
        }
        prob *= sqrt( pow(value,(double)dofd) );
        if( swap ) prob = 1.0 - prob;
    }

    return prob;

}


double inv_f_distn( double prob, long dofn, long dofd )
{
    if( dofn <= 0 || dofd < 0 ) return 0.0;
    return inverse_distribution( prob, f_distn, dofn, dofd );
}


double tau_distn( double value, long dof )
{
    char negative;
    double prob;

    negative = value < 0.0;
    value *= value;

    if( dof < 1 || value >= dof ) return 0.0;
    value = sqrt((dof-1)*value/(dof - value));
    prob = students_t_distn( value, dof-1 );
    if( negative ) prob = 1.0 - prob;
    return prob;
}


double inv_tau_distn( double prob, long dof )
{
    double value, tmp;
    if( dof < 1 ) return 0.0;
    value = inv_students_t_distn( prob, dof-1 );
    if( value != 0 )
    {
        tmp = sqrt( dof * value * value / ( dof - 1 + value * value ) );
        value = value < 0.0 ? -tmp : tmp;
    }
    return value;
}



/* Routine to locate the inverse of a statistical distribution function
   This is a very crude inversion routine, but it works for nice
   monotonically decreasing 1 - cumulative distribution functions.
   It will only search VALUE>0.
*/

// #pragma warning ( disable: 4127 )
static double inverse_distribution( double prob,
                                    double (*distfunc)( double value, long prm1, long prm2),
                                    long prm1, long prm2 )
{
    double tol = 0.0001;  /* Tolerance accepted in log of value */
    double lvmin = -11.5; /* Minimum value of log */
    double lvmax = 20.0;  /* Maximum value of log */

    double lv0, lv1, lv2, pr;

    /* First set A1 and A2 so that log(VALUE) lies between them */

    pr = (*distfunc)( 1.0, prm1, prm2 );

    if( pr < prob )
    {
        lv2 = 0.0;
        lv0 = -1.0;

        for(;;)
        {
            pr = (*distfunc)(exp(lv0),prm1,prm2);
            if( pr > prob ) break;
            lv0 += lv0;
            if( lv0 < lvmin) return 0.0;
        }
    }

    else
    {
        lv0 = 0.0;
        lv2 = 1.0;

        for(;;)
        {
            pr = (*distfunc)(exp(lv2),prm1,prm2);
            if( pr < prob ) break;
            lv2 += lv2 ;
            if( lv2 > lvmax ) return exp(lvmax);
        }
    }

    do
    {
        lv1 = (lv0+lv2)/2.0;
        pr = (*distfunc)(exp(lv1),prm1,prm2);
        if( pr > prob ) lv0 = lv1; else lv2 = lv1;
    }
    while( lv2-lv0 > tol );


    return exp(lv1);
}


#ifdef TESTPROB

int main( )
{
    char line[80], opt[3], *o;
    double value;
    long dof1, dof2;
    char inverse;
    char doinv = 0;

    printf("Enter [I](N|F|C|S|T) value [dof1] [dof2]\n");

    while( printf("> "), gets(line))
    {
        if( line[0] == 0 )
        {
            if(!doinv) break;
            inverse = !inverse;
            doinv = 0;
        }
        else
        {
            sscanf(line,"%2s%lf%d%d",opt,&value,&dof1,&dof2);
            o = opt;
            inverse = ( *o == 'i' || *o == 'I' );
            if( inverse ) o++;
            doinv = 1;
        }
        if( inverse ) { printf("Inverse "); }
        switch (*o)
        {
        case 'n':
        case 'N': printf("Normal (%.4lf)", value);
            value = inverse ? inv_normal_distn( value ) :
                    normal_distn( value );
            break;

        case 'c':
        case 'C': printf("Chi2 (%.4lf,%ld)",value,dof1);
            value = inverse ? inv_chi2_distn( value, dof1 ) :
                    chi2_distn( value, dof1 );
            break;

        case 's':
        case 'S': printf("Students t (%.4lf,%ld)",value,dof1);
            value = inverse ? inv_students_t_distn( value, dof1 ) :
                    students_t_distn( value, dof1 );
            break;

        case 't':
        case 'T': printf("Tau (%.4lf,%ld)",value,dof1);
            value = inverse ? inv_tau_distn( value, dof1 ) :
                    tau_distn( value, dof1 );
            break;

        case 'f':
        case 'F': printf("F (%.4lf,%ld,%ld)",value,dof1,dof2);
            value = inverse ? inv_f_distn( value, dof1, dof2 ) :
                    f_distn( value, dof1, dof2 );
            break;

        default: printf("Invalid command\n");
            doinv = 0;
            continue;
        }
        printf(" = %.4lf\n",value);
    }
}

#endif
