#include "snapconfig.h"
/*
   $Log: adjparam.c,v $
   Revision 1.2  2001/05/14 18:26:17  ccrook
   Minor updates

   Revision 1.1  1996/01/03 21:55:41  CHRIS
   Initial revision

*/

#include <cstdio>
#include <cmath>
#include <string>
#include "util/leastsqu.h"
#include "util/lsobseq.h"
#include "output.h"
#include "snap/snapglob.h"
#include "snap/genparam.h"
#include "snap/obsparam.h"
#include "adjparam.h"

void set_param_obseq(int pid, void* hA, int irow, double v) {
    int rowno;

    rowno = param_rowno(pid);
    if (rowno)
        oe_param(hA, irow, rowno, v);
}

void update_params(int get_covariance) {
    double delta;
    int np;
    int nparam;

    nparam = param_count();

    if (nparam) {
        for (np = 0; np++ < nparam;) {
            int rn;
            double covar = 0.0;
            rn = param_rowno(np);
            if (rn) {
                rn--; /* Convert to 0 based numbers */
                lsq_get_params(&rn, 1, &delta, get_covariance ? &covar : nullptr);
                update_param_value(np, param_value(np) + delta, covar);
            }
        }
    }

    nparam = get_obs_param_count();
    if (nparam) {
        for (np = 0; np++ < nparam;) {
            int rn;
            double covar = 0.0;
            double value = 0.0;
            rn = get_obs_param_rowno(np, &value);
            if (rn) {
                rn--; /* Convert to 0 based numbers */
                lsq_get_params(&rn, 1, &delta, get_covariance ? &covar : nullptr);
                update_obs_param_value(np, value + delta, covar);
            }
        }
    }
}

void list_calculated_parameters(std::FILE* out) {
    int np;
    int first;
    int nparam;
    param* p;
    const char* header = "\n\nThe following parameters are also being calculated\n\n";

    nparam = param_count();
    first = 1;

    if (nparam) {
        for (np = 0; np++ < nparam;) {
            p = param_from_id(sorted_param_id(np));
            if (!(p->flags & PRM_ADJUST)) {
                continue;
            }
            if (first) {
                std::fputs(header, out);
                first = 0;
            }

            std::fputs(p->name.c_str(), out);
            if (p->identical) {
                std::fprintf(out, "  (same as %s)", param_name(p->identical).data());
            }
            std::fputs("\n", out);
        }
    }

    nparam = get_obs_param_count();
    if (nparam) {
        if (first) {
            std::fputs(header, out);
            first = 0;
        }
        for (np = 0; np++ < nparam;) {
            if (!get_obs_param_used(np)) {
                continue;
            }
            std::fputs(get_obs_param_name(np).c_str(), out);
            std::fputs("\n", out);
        }
    }
}

static void print_adjusted_parameters_header(std::FILE* out) {
    print_section_header(out, "OTHER PARAMETERS");
    std::fprintf(lst, "\nThe errors listed for calculated parameters are %s errors\n",
                 apriori ? "apriori" : "aposteriori");
    std::fprintf(out, "\nParameter                          value         +/-\n");
}

void print_adjusted_parameters(std::FILE* out) {
    param* p;
    int np;
    int nparam;
    double semult;
    int first = 1;

    semult = apriori ? 1.0 : seu;
    nparam = param_count();
    if (nparam) {
        for (np = 0; np++ < nparam;) {
            int pid;
            pid = sorted_param_id(np);
            p = param_from_id(pid);
            if (p->flags & PRM_LISTED) {
                continue;
            }

            if (first) {
                print_adjusted_parameters_header(out);
                first = 0;
            }

            std::fprintf(out, "%-30.30s   %11.5lf  ", p->name.c_str(), p->value);
            if (param_rowno(pid)) {
                std::fprintf(out, "%11.5lf", p->covar > 0.0 ? std::sqrt(p->covar) * semult : 0.0);
            } else {
                std::fprintf(out, "     -     ");
            }
            if (p->identical) {
                std::fprintf(out, "  = %s", param_name(p->identical).data());
            }
            std::fprintf(out, "\n");
        }
    }

    nparam = get_obs_param_count();
    if (nparam) {
        if (first) {
            print_adjusted_parameters_header(out);
            first = 0;
        }
        for (np = 0; np++ < nparam;) {
            const std::string& name = get_obs_param_name(np);
            const double value = get_obs_param_value(np);
            std::fprintf(out, "%-30.30s   %11.5lf  ", name.c_str(), value);
            if (get_obs_param_rowno(np, nullptr)) {
                const double covar = std::sqrt(get_obs_param_covar(np)) * semult;
                std::fprintf(out, "%11.5lf\n", covar);
            } else {
                std::fprintf(out, "     -\n");
            }
        }
    }

    if (!first) {
        print_section_footer(out);
    }
}
