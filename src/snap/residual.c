#include "snapconfig.h"
/* Maintain and print a list of the worst residuals */
/* Actually it maintains a separate list of the n worst residuals for
   each probability distribution.  When they are printed it takes values
   from each list in order of their significance.  This avoid having to
   calculate the significance of every residual. */


/*
   $Log: residual.c,v $
   Revision 1.6  2004/04/22 02:35:44  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.5  2001/07/04 22:57:51  ccrook
   Modified totalcount and nsigres to be long integers to avoid losing most
   significant residuals when over 32k obs components.

   Revision 1.4  1999/05/26 07:02:11  ccrook
   Modified to allow listing of worst residuals without listing all residuals

   Revision 1.3  1996/02/23 17:04:51  CHRIS
   Moved function residual_significance out of module (now in datastat.c)

   Revision 1.2  1996/02/19 19:03:53  CHRIS
   Fixed a bug resulting in the flag levels for used and unused observations
   being confused when aposteriori errors are used.

   Revision 1.1  1996/01/03 22:06:37  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "snap/snapglob.h"
#include "util/chkalloc.h"
#include "snap/stnadj.h"
#include "snap/survfile.h"
#include "output.h"
#include "residual.h"
#include "util/probfunc.h"
#include "util/binfile.h"
#include "notedata.h"
#include "snap/datastat.h"

typedef struct
{
    int from;
    int to;
    int id;
    int type;
    int file;
    int line;
    char unused;
    double sres;
    long note;
} residual;

#define MAXRANK 3

static residual *worstlst[2][MAXRANK];
static int nworst[2][MAXRANK];
static long totalcount = 0;

static double flagval[3][2][2];  /* rank, unused, level */
static long nsigres[2][3];      /* unused, level (including none) */
static int flag_values_set = 0;



static void setup_flag_values( void )
{
    int rank, level;
    double prob1, prob, value;

    for( level = 0; level < 2; level++ )
    {
        prob1 = 1.0 - flag_level[level]/100.0;

        for( rank = 1; rank <= 3; rank++ )
        {
            if( !taumax[level] )
            {
                prob = prob1;
            }
            else
            {
                prob = prob_of_maximum( prob1, rank );
            }

            if( apriori )
            {
                value = inv_chi2_distn( prob, rank );
                if( value > 0.0 ) value = sqrt( value/rank );
                flagval[rank-1][0][level] = flagval[rank-1][1][level] = value;
            }
            else
            {
                value = inv_f_distn( prob, rank, dof );
                flagval[rank-1][0][level] = value > 0.0 ? sqrt(value) : value;

                if( dof > rank )
                {
                    value = inv_f_distn( prob, rank, dof-rank );
                    value = dof * value / ( dof - rank + rank * value );
                    flagval[rank-1][1][level] = value > 0.0 ? sqrt(value) : value;
                }
                else
                {
                    flagval[rank-1][1][level] = 1.0;
                }
            }
        }
    }
    flag_values_set = 1;
}


const char *residual_flag( int unused, int rank, double sres )
{
    const static char *blank = "";
    const static char *flag1 = FLAG1;
    const static char *flag2 = FLAG2;
    int used;

    if( !flag_values_set ) setup_flag_values();

    rank--;
    if( rank < 0 || rank > 2 ) rank = 2;
    used = unused ? 0 : 1;

    if( sres < 0.0 ) sres = -sres;
    if( sres > flagval[rank][used][1] ) return flag2;
    if( sres > flagval[rank][used][0] ) return flag1;
    return blank;
}



void save_residual( int from, int to, int id, int type,
                    int file, int line, char unused, int rank, double sres, long note )
{
    int i, level;
    int used;
    residual rs;
    residual *ws;
    int nws;

    if( sres < 0 || rank < 1 || rank > MAXRANK ) return;
    if( ! apriori )
    {
        if( seu <= 0.0 ) return;
        sres /= seu;
    }

    rs.from = from;
    rs.to = to;
    rs.id = id,
       rs.type = type;
    rs.file = file;
    rs.line = line;
    rs.unused = unused;
    rs.sres = sres;
    rs.note = note;

    /* If we haven't initiallized yet - do so */

    if( totalcount == 0 )
    {
        for( i = 0; i < MAXRANK; i++ )
        {
            worstlst[0][i] = worstlst[1][i] = NULL;
            nworst[0][i] = nworst[1][i] = 0;
        }

        for( i=0; i<3; i++ )
        {
            nsigres[0][i] = nsigres[1][i] = 0;
        }
    }

    used = unused == ' ' ? 1 : 0;
    rank--;
    if( sres < 0.0 ) sres = -sres;

    if( worstlst[used][rank] == NULL )
    {
        worstlst[used][rank] = (residual *) check_malloc( (maxworst + 1) * sizeof(residual) );
    }

    ws = worstlst[used][rank];
    nws = nworst[used][rank];

    for( i = nws; i-- && ws[i].sres < sres; )
    {
        memcpy( ws+i+1, ws+i, sizeof(residual) );
    }

    i++;
    memcpy( ws+i, &rs, sizeof(residual) );

    if( nws < maxworst ) nws++;
    nworst[used][rank] = nws;

    if( !flag_values_set ) setup_flag_values();
    if( sres <= flagval[rank][used][0] ) level = 0;
    else if( sres <= flagval[rank][used][1] ) level = 1;
    else level = 2;

    nsigres[used][level]++;

    totalcount++;
}

void print_worst_residuals( FILE *out )
{
    double prob[2][MAXRANK];
    int index[2][MAXRANK];
    residual *ws;
    double maxprob;
    int i, j, maxi, maxj, nwslist, level;
    int useall, iused, imin, imax;

    if( totalcount <= 0 ) return;

    print_section_header( out, "MOST SIGNIFICANT RESIDUALS" );
    print_zero_inverse_warning( out );

    fprintf(out,"\n\nThe %ld residuals from this data are classified as follows:\n\n",totalcount);

    for( i=0; i<3; i++ )
    {
        if( i < 2 ) fprintf(out,"Under"); else fprintf(out,"Over ");
        level = i < 2 ? i : 1;
        fprintf(out,"%6.2lf%%%c significant    Used: %3ld    Unused: %3ld\n",
                flag_level[level], taumax[level] ? 'M':' ',
                nsigres[1][i], nsigres[0][i] );
    }

    fprintf(out,"\n");
    if( taumax[0] || taumax[1] )
    {
        fprintf(out,"The \'M\'indicates that the significance applies to the maximum\n");
        fprintf(out,"of all residuals rather than to the individual residuals.\n");
    }
    fprintf(out,"Note: Only the overall residual for vector data is counted\n");

    /* If useall is non-zero, then worst residuals of used and rejected
       observations are combined */

    useall = 0;

    for( iused=0; iused < (useall ? 1 : 2 ); iused++ )
    {
        if( useall )
        {
            imin = 0; imax = 1;
        }
        else
        {
            imin = imax = iused;
        }

        nwslist = 0;
        for( i=imin; i<=imax; i++ ) for( j=0; j<MAXRANK; j++ )
                nwslist += nworst[i][j];

        if( !nwslist ) continue;
        if( nwslist >= maxworst ) nwslist = maxworst;

        fprintf(out,"\n\nThe following table lists the %d worst residuals",
                (int)nwslist);
        if( !useall ) fprintf( out," of %s data", iused ? "used" : "rejected" );

        fprintf(out,"\n\n%-*s  %-*s%s  Type     S.R.  Sig (%%)       Line  File\n",
                stn_name_width,"From",stn_name_width,"To",have_obs_ids ? "      Id" : "");

        for( i = imin; i <= imax; i++ ) for( j = 0; j < MAXRANK; j++ )
            {
                index[i][j] = 0;
                if( index[i][j] < nworst[i][j] )
                {
                    prob[i][j] = residual_significance( worstlst[i][j][0].sres, i, j+1 );
                }
            }

        while( nwslist-- )
        {
            int from;
            int to;

            /* Find the largest probability */

            maxi = maxj = -1; maxprob = -2.0;
            for( i=imin; i<=imax; i++ ) for( j=0; j<MAXRANK; j++ )
                {
                    if( index[i][j] < nworst[i][j] && prob[i][j] > maxprob )
                    {
                        maxi = i;
                        maxj = j;
                        maxprob = prob[i][j];
                    }
                }
            if( maxi < 0 ) break;

            ws = worstlst[maxi][maxj]+index[maxi][maxj];
            from = ws->from;
            to = ws->to;
            if( ! from ) { from = to; to = 0; }

            list_note( out, ws->note );

            fprintf(out,"\n%-*s  %-*s",
                    stn_name_width, station_code(from),
                    stn_name_width, to ? station_code(to) : "");
            if( have_obs_ids )
            {
                fprintf( out, " %7d", ws->id );
            }
            fprintf(out,"   %-2s%c  %7.3lf  ",
                    datatype[ws->type].code,ws->unused,
                    ws->sres);
            if( prob[maxi][maxj] > 0.0 )
            {
                fprintf(out,"%7.3lf",prob[maxi][maxj]);
            }
            else
            {
                fputs("   -   ", out );
            }
            fprintf(out," %-3s",residual_flag( 1-maxi, maxj+1,ws->sres) );
            fprintf(out,"  %5d  %s\n",(int)(ws->line),survey_data_file_name(ws->file));

            index[maxi][maxj]++;
            if( index[maxi][maxj] < nworst[maxi][maxj] )
            {
                prob[maxi][maxj] = residual_significance(ws[1].sres,maxi,maxj+1);
            }
        }
    }
    print_section_footer( out );
}
