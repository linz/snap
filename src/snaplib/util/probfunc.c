#include "snapconfig.h"
/* Cumulative probability functions and inverse functions using boost
*/

#include <stdio.h>
#include <math.h>
#include <boost/math/distributions/chi_squared.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/students_t.hpp>
#include <boost/math/distributions/fisher_f.hpp>
#include "util/probfunc.h"

using boost::math::chi_squared;
using boost::math::normal;
using boost::math::students_t;
using boost::math::fisher_f;

double normal_distn( double value )
{
    normal n;
    return 1.0-cdf(n,value);
}

double inv_normal_distn( double prob )
{
    normal n;
    return quantile(n,1.0-prob);
}

double chi2_distn( double value, long dof )
{
    if( dof < 0 ) return 0.0;
    if( value < 0.0 ) return 0.0;
    chi_squared chi2(dof);
    return 1.0-cdf(chi2,value);
}

double inv_chi2_distn( double prob, long dof )
{
    chi_squared chi2(dof);
    return quantile( chi2, 1.0-prob );
}


double students_t_distn( double value, long dof )
{
    students_t st(dof);
    return 1.0-cdf(st,value);
}


double inv_students_t_distn( double prob, long dof )
{
    students_t st(dof);
    return quantile(st,1.0-prob);
}


/* F distribution is based upon equations 26.6.4, 26.6.5, and 26.6.8 */


double f_distn( double value, long dofn, long dofd )
{
    if( dofn <= 0 || dofd < 0 ) return 0.0;
    if( dofd == 0 )  return chi2_distn( value*dofn, dofn );
    if( value<=0.0) return 0.0;
    fisher_f f(dofn,dofd); 
    return 1.0-cdf(f,value);
}

double inv_f_distn( double prob, long dofn, long dofd )
{
    if( dofn <= 0 || dofd < 0 ) return 0.0;
    if( dofd == 0 )  return inv_chi2_distn( prob, dofn )/dofn;
    fisher_f f(dofn,dofd); 
    return quantile(f,1.0-prob);
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


#ifdef TESTPROB

int main( int argc, char *argv[] )
{
    char line[80], opt[3], *o;
    double value;
    long dof1, dof2;
    char inverse;
    char doinv = 0;
    int istty=1;
    FILE *infile=stdin;

    if( argc > 1 )
    {
        infile=fopen(argv[1],"r");
        istty=0;
        if( ! infile )
        {
            printf("Cannot open %s\n",argv[1]);
            return 0;
        }
    }

    if( istty ) printf("Enter [I](N|F|C|S|T) value [dof1] [dof2]\n");

    while( (! istty || printf("> ")), fgets(line,79,infile))
    {
        if( line[0] == 0 )
        {
            if(!doinv) break;
            inverse = !inverse;
            doinv = 0;
        }
        else
        {
            sscanf(line,"%2s%lf%ld%ld",opt,&value,&dof1,&dof2);
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
        printf(" = %.8lf\n",value);
    }
}

#endif
