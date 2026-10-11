#include "snapconfig.h"
/* Code to form summaries of residuals */

/*
   $Log: ressumry.c,v $
   Revision 1.2  1998/06/15 02:21:01  ccrook
   Modified to support long integer number of observations

   Revision 1.1  1996/01/03 22:07:24  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <algorithm>
#include <assert.h>
#include <utility>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include "util/fieldscanner.hpp"

#include "ressumry.h"

using boost::numeric_cast;
#include "snap/snapglob.h"
#include "util/classify.h"
#include "snapdata/datatype.h"
#include "snap/survfile.h"
#include "snapdata/survdata.h"
#include "snapdata/gpscvr.h"
#include "snap/stnadj.h"
#include "snap/bindata.h"
#include "util/errdef.h"
#include "util/progress.h"
#include "output.h"


/* summary_def defines a summary that is required.  The levels are defined
   as an array of level_id's.  Possible values are BY_DATA_TYPE, BY_FILE,
   or the number of a classification type.  */

struct summary_def
{
    int nlevel = 0;                  ///< The number of levels actually defined
    int enu_components = 0;          ///< Non-zero if east, north and up summaries are required
    std::vector<int> level_id;       ///< BY_DATA_TYPE, BY_FILE or a classification id for each level
    std::vector<int> level_count;    ///< The number of values at each level, set by init_summary

    /// Creates a summary with room for up to maxlevel levels
    explicit summary_def( const size_t maxlevel ) : level_id( maxlevel ), level_count( maxlevel ) {}
};

#define BY_DATA_TYPE -1
#define BY_FILE      -2

#define ERROR_SUMMARY_DELIMITER '/'
#define DATA_TYPE_STR "data_type"
#define FILE_STR      "data_file"

static std::vector<summary_def> summary_defs;

/*=======================================================================*/

struct error_total
{
    double ssr;
    long  count;
    int axis;
    int used;
};

/* Static data used for summing obs .. should be moved to dynamically allocated structure */

static int enu_period=0;
static error_total *total = NULL;
static int obstype_index[NOBSTYPE];
static int obstype_from_index[NOBSTYPE*4];
static int obsenu_from_index[NOBSTYPE*4];

/* Read a summary definition as a string of items separated by "/" characters,
   e.g. "file/data_type", "equipment/data_type", ... 
   
   data_type can be followed by :no_enu to suppress calculating summaries for
   east, north, up components. */

int define_error_summary( const std::string &definition )
{
    int nlevel, ilevel;
    int sts;

    nlevel = numeric_cast<int>( std::count( definition.begin(), definition.end(), ERROR_SUMMARY_DELIMITER ) ) + 1;

    summary_def sdf( numeric_cast<size_t>( nlevel ) );

    nlevel = 0;

    FieldScanner scanner(definition);
    sts = OK;
    bool lastField = false;
    while( sts == OK && ! lastField )
    {
        std::string_view field;
        if( auto f = scanner.next(ERROR_SUMMARY_DELIMITER) ) { field = *f; }
        else { field = scanner.remainder(); lastField = true; }

        if( ! field.empty() && field.size() < 32 )
        {
            const std::string fieldStr(field);
            if( boost::algorithm::iequals(fieldStr,DATA_TYPE_STR) )
            {
                sdf.level_id[nlevel] = BY_DATA_TYPE;
                sdf.enu_components = 1;
            }
            else if( boost::algorithm::istarts_with(fieldStr,DATA_TYPE_STR) &&
                     fieldStr.size() > strlen(DATA_TYPE_STR) && fieldStr[strlen(DATA_TYPE_STR)] == ':' )
            {
                sdf.level_id[nlevel] = BY_DATA_TYPE;
                sdf.enu_components = boost::algorithm::iequals(fieldStr.substr(strlen(DATA_TYPE_STR)),":no_enu") ? 0 : 1;
            }
            else if( boost::algorithm::iequals(fieldStr,FILE_STR) )
            {
                sdf.level_id[nlevel] = BY_FILE;
            }
            else
            {
                sdf.level_id[nlevel] = obs_classes.id( fieldStr, 1 );
            }
            for( ilevel = 0; ilevel < nlevel; ilevel++ )
            {
                if( sdf.level_id[ilevel] == sdf.level_id[nlevel] ) sts = INVALID_DATA;
            }
            nlevel++;
        }
        else
        {
            sts = INVALID_DATA;
        }
    }

    if( sts == OK && nlevel )
    {
        for( ilevel = 1; ilevel < nlevel; ilevel++ )
        {
            int jlevel;
            for( jlevel=ilevel-1; jlevel >= 0; jlevel-- )
            {
                if( sdf.level_id[ilevel] == sdf.level_id[jlevel] )
                {
                    sts=INVALID_DATA;
                    break;
                }
            }
        }
    }

    if( sts == OK && nlevel )
    {
        sdf.nlevel = nlevel;
        summary_defs.push_back( std::move( sdf ) );
    }
    else
    {
        sts = INVALID_DATA;
    }
    return sts;
}

/* Initialise a summary using a definition */

static int init_summary( summary_def &sdf )
{
    int i;
    int nobstype_used;
    int index_size;
    char is_vector;

    /* Count the number of observation types that are actually referenced
       in the data */

    nobstype_used = 0;
    for( i=0; i<NOBSTYPE; i++ )
    {
        int ncomp;
        int icomp;
        if( obstypecount[i] )
        {
            obstype_index[i] = nobstype_used;
            is_vector= datatype[i].isvector ? 1 : 0;
            ncomp=1;
            if( sdf.enu_components && is_vector ) ncomp=4;
            for( icomp=0; icomp < ncomp; icomp++ )
            {
                obstype_from_index[nobstype_used] = i;
                obsenu_from_index[nobstype_used]=icomp;
                nobstype_used++;
            }
        }
        else
        {
            obstype_index[i] = -1;
        }
    }
    if( !nobstype_used ) return MISSING_DATA;

    /* Now find the number of classifications in each level of the
       summary */

    index_size = 1;
    enu_period = 0;

    for( i = 0; i < sdf.nlevel; i++ )
    {
        switch( sdf.level_id[i] )
        {

        case BY_DATA_TYPE:
            sdf.level_count[i] = nobstype_used;
            if( sdf.enu_components ) enu_period = 1;
            break;

        case BY_FILE:
            sdf.level_count[i] = survey_data_file_count();
            break;

        default:
            sdf.level_count[i] = obs_classes.value_count( sdf.level_id[i] );
            break;
        }
        if( sdf.level_count[i] <= 0 ) sdf.level_count[i] = 1;

        index_size *= sdf.level_count[i];
        enu_period *= sdf.level_count[i];
    }

    /* Form the totals array with two elements for each array index - used and unused */

    enu_period /= nobstype_used;
    enu_period *= 2;
    index_size *= 2;

    total = new error_total[index_size];

    for( i=0; i < index_size; i++ )
    {
        total[i].ssr = 0.0;
        total[i].count = 0;
        total[i].axis = 0;
        total[i].used = 0;
    }

    return OK;
}

static void term_summary( void )
{
    delete [] total;
    total = nullptr;
}

static int get_obs_class_val( survdata *sd, trgtdata *t, int class_id )
{
    int name_id = 0;
    if( t->nclass )
    {
        classdata *cd;
        int i;
        cd = sd->clsf+t->iclass;
        for( i=t->nclass; i--; cd++ )
        {
            if( cd->class_id == class_id ) { name_id = cd->name_id; break; }
        }
    }
    return name_id;
}

static void sum_observation( const summary_def &sdf, survdata *sd )
{
    trgtdata  *t;
    int iobs, ilevel;
    int index;
    char is_vector;

    for( iobs = 0; iobs < sd->nobs; iobs++ )
    {

        /* Calculate the index into the total array at which the data are to
        be stored */

        t = get_trgtdata( sd, iobs );
        is_vector= datatype[t->type].isvector ? 1 : 0;
        index = 0;
        for( ilevel = 0; ilevel < sdf.nlevel; ilevel++ )
        {
            int class_id, class_val;

            class_id = sdf.level_id[ilevel];
            switch( class_id )
            {
            case BY_DATA_TYPE: class_val = obstype_index[t->type]; break;
            case BY_FILE:      class_val = sd->file; break;
            default:           class_val = get_obs_class_val( sd, t, class_id );
                break;
            }

            if( class_val < 0 || class_val >= sdf.level_count[ilevel] )
            {
                index = -1;
                break;
            }

            if( ilevel ) index *= sdf.level_count[ilevel];
            index += class_val;
        }

        if( index < 0 )
        {
            assert(0);
            continue;
        }

        /* Determine whether the observation was used or not */

        index *= 2;

        if( t->unused ||
                (sd->from && rejected_station( sd->from )) ||
                (t->to && rejected_station(t->to) ))
        {
            index++;
        }

        /* Sum the observation */

        switch( sd->format )
        {

        case SD_OBSDATA:
        {
            obsdata *od;
            od=&sd->obs.odata[iobs];
            if( od->sres >= 0 )
            {
                total[index].ssr += od->sres * od->sres;
                total[index].count++;
            }
            total[index].used=1;
        }
        break;

        case SD_VECDATA:
        {
            int vdopt=output_xyz_vector_residuals ? 0 : VD_TOPOCENTRIC;
            vecdata *vd;
            vd = &sd->obs.vdata[iobs];
            total[index].ssr += vd->vsres*vd->vsres*vd->rank;
            total[index].count += vd->rank;
            total[index].used = 1;
            if( is_vector && enu_period > 0 )
            {
                int iaxis;
                double sres[3], serr[3];
                calc_vecdata_vector( sd, VD_REF_STN, iobs,
                                     VD_RESVEC | VD_STDERR | vdopt,
                                     sres, serr );
                for( iaxis = 0; iaxis<3; iaxis++)
                {
                    index += enu_period;
                    if( serr[iaxis] > 1.0e-6 )
                    {
                        sres[iaxis] = sres[iaxis] / serr[iaxis];
                        total[index].ssr += sres[iaxis] * sres[iaxis];
                        total[index].count++;
                        /* Really should be setting this up in init_summary, but simpler here! */
                        total[index].axis=iaxis+1;
                    }
                    total[index].used = 1;
                }
            }
        }
        break;

        case SD_PNTDATA:
        {
            pntdata *pd;
            pd = &sd->obs.pdata[iobs];
            assert( ! datatype[t->type].isvector );
            if( pd->sres >= 0 )
            {
                total[index].ssr += pd->sres * pd->sres;
                total[index].count++;
            }
            total[index].used=1;
        }
        break;


        default: assert(0); break;
        }
    }

}


static void sum_summary( const summary_def &sdf )
{
    long nbin;
    bindata b;
    init_get_bindata( 0L );
    init_progress_meter( nbindata );
    nbin = 0;
    while( get_bindata( SURVDATA, b ) == OK )
    {
        nbin++;
        update_progress_meter( nbin );
        sum_observation( sdf, b.survey_data() );
    }
    end_progress_meter();
}

/* The print summary routine is called recursively */

#define TITLE_WIDTH  40
#define LEVEL_INDENT  5
#define AXIS_INDENT   8

static void print_summary_level( FILE *lst, const summary_def &sdf,
                                 int ilevel, int index, int axis, double semult )
{
    int ilvl, sublevel_count;
    int lastlevel= ilevel >= sdf.nlevel-1 && sdf.level_id[ilevel] == BY_DATA_TYPE;

    /* To avoid using unecessary space in the recursive call, enclose
       the working bit and its automatic variables in a block */

    sublevel_count = 1;
    for( ilvl = ilevel+1; ilvl < sdf.nlevel; ilvl++ )
    {
        sublevel_count *= sdf.level_count[ilvl];
    }
    sublevel_count *= 2;

    for( ilvl = 0; ilvl < sdf.level_count[ilevel]; ilvl++, index += sublevel_count )
    {
        int iaxis;

        /* Print the information for the current level */

        {
            error_total sum_total[3];
            int i, j;
            
            if( sdf.level_id[ilevel] == BY_DATA_TYPE )
            {
                iaxis=obsenu_from_index[ilvl];
                if( axis && iaxis != axis ) continue;
            }
            else
            {
                iaxis=axis;
            }
                
            for( j=0; j<3; j++ )
            {
                sum_total[j].ssr   = 0.0;
                sum_total[j].count = 0;
                sum_total[j].used = 0;
            }
            for( i=0; i<sublevel_count; i+=2 )
            {
                error_total *ttl;
                ttl = total + index + i;
                if( ttl[0].axis == iaxis )
                {
                    sum_total[0].ssr += ttl[0].ssr;
                    sum_total[0].count += ttl[0].count;
                    sum_total[2].ssr += ttl[0].ssr;
                    sum_total[2].count += ttl[0].count;
                    sum_total[2].used += ttl[0].used;
                }
                if( ttl[1].axis == iaxis )
                {
                    sum_total[1].ssr += ttl[1].ssr;
                    sum_total[1].count += ttl[1].count;
                    sum_total[2].ssr += ttl[1].ssr;
                    sum_total[2].count += ttl[1].count;
                    sum_total[2].used += ttl[1].used;
                }
            }

            /* Don't print data where there are no observations */
            if( sum_total[2].used == 0 ) continue;

            {
                int indent;
                int ttlwidth;
                std::string title;
                char ttlbuf[32];
                indent = ilevel * LEVEL_INDENT;
                if( iaxis && lastlevel ) indent += AXIS_INDENT;
                ttlwidth = TITLE_WIDTH - indent;
                if( indent ) fprintf(lst,"%*s",indent,""); else fprintf(lst,"\n");

                switch( sdf.level_id[ilevel] )
                    {

                    case BY_DATA_TYPE:
                        if( iaxis )
                        {
                            if( output_xyz_vector_residuals )
                            {
                                switch( iaxis )
                                {
                                case 1: title = "X component"; break;
                                case 2: title = "Y component"; break;
                                case 3: title = "Z component"; break;
                                }
                            }
                            else
                            {
                                switch( iaxis )
                                {
                                case 1: title = "East component"; break;
                                case 2: title = "North component"; break;
                                case 3: title = "Up component"; break;
                                }
                            }
                            if( ! lastlevel )
                            {
                                sprintf(ttlbuf,"%.4s %s",
                                    datatype[obstype_from_index[ilvl]].code.data(),
                                    title.c_str());
                                title=ttlbuf;
                            }
                        }
                        else
                        {
                            title = datatype[obstype_from_index[ilvl]].name;
                        }
                        break;

                    case BY_FILE:
                        title = survey_data_file_name(ilvl);
                        break;

                    default:
                        title = obs_classes.value_name( sdf.level_id[ilevel], ilvl );
                        break;
                    }

                if( ttlwidth > 0 )
                    fprintf(lst,"%-*.*s",ttlwidth,ttlwidth,title.c_str());

                for( j=0; j<3; j++)
                {
                    if( sum_total[j].count )
                    {
                        fprintf(lst," %7.2lf %4ld",
                                sqrt(sum_total[j].ssr/sum_total[j].count)/semult,
                                sum_total[j].count );
                    }
                    else
                    {
                        fprintf(lst,"     -      -");
                    }
                }
                fprintf(lst,"\n");
            }
        }

        /* Now print out the sub-levels */

        if( ilevel < sdf.nlevel-1 )
        {
            print_summary_level( lst, sdf, ilevel+1, index, iaxis, semult );
        }
    }
}


static void print_summary( FILE *lst, summary_def &sdf, double semult )
{
    int ilvl;
    if( init_summary( sdf ) != OK ) return;
    sum_summary( sdf );
    fprintf(lst,"\n\nSummary of residuals classified by ");
    for( ilvl = 0; ilvl < sdf.nlevel; ilvl++ )
    {
        if( ilvl ) fprintf(lst,", ");
        switch( sdf.level_id[ilvl] )
        {
        case BY_DATA_TYPE: fprintf(lst,"data type"); break;
        case BY_FILE:      fprintf(lst,"input file"); break;
        default:           fprintf(lst,"%s",
                                       obs_classes.name(sdf.level_id[ilvl]).c_str());
            break;
        }
    }
    fprintf(lst,"\n\n%-*s%s\n%-*s%s\n\n",
            TITLE_WIDTH,"Classification",
            "     Used        Unused       Total",
            TITLE_WIDTH,"",
            "    RMS  Count   RMS  Count   RMS  Count\n");
    print_summary_level( lst, sdf, 0, 0, 0, semult );
    term_summary();
}

void print_error_summary( FILE *lst )
{
    print_section_header( lst, "ERROR SUMMARY" );
    print_zero_inverse_warning( lst );
    print_convergence_warning( lst );
    if( summary_defs.empty() )
    {
        define_error_summary( DATA_TYPE_STR );
        if( survey_data_file_count() > 1 ) define_error_summary( FILE_STR );
    }
    for( summary_def &sdf : summary_defs )
    {
        print_summary( lst, sdf, apriori ? 1.0 : seu );
    }
    print_section_footer( lst );
}
