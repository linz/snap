#include "snapconfig.h"
/* Cumulative probability functions and inverse functions using boost
*/

#include <cstdio>
#include <cmath>
#include <boost/math/distributions/chi_squared.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/students_t.hpp>
#include <boost/math/distributions/fisher_f.hpp>
#include "util/probfunc.h"

using boost::math::chi_squared;
using boost::math::fisher_f;
using boost::math::normal;
using boost::math::students_t;

double normal_distn(double value) {
    const normal n;
    return 1.0 - cdf(n, value);
}

double inv_normal_distn(double prob) {
    const normal n;
    return quantile(n, 1.0 - prob);
}

double chi2_distn(double value, long dof) {
    if (dof < 0) {
        return 0.0;
    }
    if (value < 0.0) {
        return 0.0;
    }
    const chi_squared chi2(dof);
    return 1.0 - cdf(chi2, value);
}

double inv_chi2_distn(double prob, long dof) {
    const chi_squared chi2(dof);
    return quantile(chi2, 1.0 - prob);
}

double students_t_distn(double value, long dof) {
    const students_t st(dof);
    return 1.0 - cdf(st, value);
}

double inv_students_t_distn(double prob, long dof) {
    const students_t st(dof);
    return quantile(st, 1.0 - prob);
}

/* F distribution is based upon equations 26.6.4, 26.6.5, and 26.6.8 */

double f_distn(double value, long dofn, long dofd) {
    if (dofn <= 0 || dofd < 0) {
        return 0.0;
    }
    if (dofd == 0) {
        return chi2_distn(value * dofn, dofn);
    }
    if (value <= 0.0) {
        return 0.0;
    }
    const fisher_f f(dofn, dofd);
    return 1.0 - cdf(f, value);
}

double inv_f_distn(double prob, long dofn, long dofd) {
    if (dofn <= 0 || dofd < 0) {
        return 0.0;
    }
    if (dofd == 0) {
        return inv_chi2_distn(prob, dofn) / dofn;
    }
    const fisher_f f(dofn, dofd);
    return quantile(f, 1.0 - prob);
}

double tau_distn(double value, long dof) {
    char negative;
    double prob;

    negative = value < 0.0;
    value *= value;

    if (dof < 1 || value >= dof) {
        return 0.0;
    }
    value = std::sqrt((dof - 1) * value / (dof - value));
    prob = students_t_distn(value, dof - 1);
    if (negative) {
        prob = 1.0 - prob;
    }
    return prob;
}

double inv_tau_distn(double prob, long dof) {
    double value;
    double tmp;
    if (dof < 1) {
        return 0.0;
    }
    value = inv_students_t_distn(prob, dof - 1);
    if (value != 0) {
        tmp = std::sqrt(dof * value * value / (dof - 1 + value * value));
        value = value < 0.0 ? -tmp : tmp;
    }
    return value;
}

#ifdef TESTPROB

int main(int argc, char* argv[]) {
    char line[80], opt[3], *o;
    double value;
    long dof1, dof2;
    char inverse;
    char doinv = 0;
    int istty = 1;
    std::FILE* infile = stdin;

    if (argc > 1) {
        infile = std::fopen(argv[1], "r");
        istty = 0;
        if (!infile) {
            std::printf("Cannot open %s\n", argv[1]);
            return 0;
        }
    }

    if (istty)
        std::printf("Enter [I](N|F|C|S|T) value [dof1] [dof2]\n");

    while ((!istty || std::printf("> ")), std::fgets(line, 79, infile)) {
        if (line[0] == 0) {
            if (!doinv)
                break;
            inverse = !inverse;
            doinv = 0;
        } else {
            std::sscanf(line, "%2s%lf%ld%ld", opt, &value, &dof1, &dof2);
            o = opt;
            inverse = (*o == 'i' || *o == 'I');
            if (inverse)
                o++;
            doinv = 1;
        }
        if (inverse) {
            std::printf("Inverse ");
        }
        switch (*o) {
            case 'n':
            case 'N':
                std::printf("Normal (%.4lf)", value);
                value = inverse ? inv_normal_distn(value) : normal_distn(value);
                break;

            case 'c':
            case 'C':
                std::printf("Chi2 (%.4lf,%ld)", value, dof1);
                value = inverse ? inv_chi2_distn(value, dof1) : chi2_distn(value, dof1);
                break;

            case 's':
            case 'S':
                std::printf("Students t (%.4lf,%ld)", value, dof1);
                value = inverse ? inv_students_t_distn(value, dof1) : students_t_distn(value, dof1);
                break;

            case 't':
            case 'T':
                std::printf("Tau (%.4lf,%ld)", value, dof1);
                value = inverse ? inv_tau_distn(value, dof1) : tau_distn(value, dof1);
                break;

            case 'f':
            case 'F':
                std::printf("F (%.4lf,%ld,%ld)", value, dof1, dof2);
                value = inverse ? inv_f_distn(value, dof1, dof2) : f_distn(value, dof1, dof2);
                break;

            default:
                std::printf("Invalid command\n");
                doinv = 0;
                continue;
        }
        std::printf(" = %.8lf\n", value);
    }
}

#endif
