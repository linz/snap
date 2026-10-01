#include "snapconfig.h"
/* Routines for managing observation selection criteria */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>

#include "snapdata/obsmod.h"
#include "snapdata/datatype.h"
#include "snapdata/survdata.h"
#include "snapdata/gpscvr.h"
#include "network/network.h"
#include "util/chkalloc.h"
#include "util/errdef.h"
#include "util/dateutil.h"
#include "util/dstring.h"
#include "util/fieldscanner.hpp"
#include "util/snapctype.h"
#include "util/wildcard.h"

#define OBS_CRIT_NONE            0
#define OBS_CRIT_DATATYPE        1
#define OBS_CRIT_DATAFILE        2
#define OBS_CRIT_CLASSIFICATION  3
#define OBS_CRIT_MCLASSIFICATION 4
#define OBS_CRIT_WCLASSIFICATION 5
#define OBS_CRIT_ID              6
#define OBS_CRIT_DATE            7
#define OBS_CRIT_STATION_USES    8
#define OBS_CRIT_STATION_BETWEEN 9

#define OBS_CRIT_DATE_UNKNOWN 1
#define OBS_CRIT_DATE_BEFORE  2
#define OBS_CRIT_DATE_AFTER   3

#define OBS_CRIT_WILDCLASS_UNINIT -1
#define OBS_CRIT_WILDCARD_FILEID -1

#define FILE_IGNORE_ERROR OK
#define FILE_WARN_ERROR   INFO_ERROR
#define FILE_FAIL_ERROR   INVALID_DATA

#define OBS_CRIT_WILDCLASS_BUFFER 64

/* Criterion against which observations are tested.  obs_criterion is a union of these */

struct obs_datatype_criterion
{
    bool select[NOBSTYPE];
};

struct obs_datafile_criterion
{
    int file_id;
    bool wildcard;
    char *filename;
    int last_file_id;
    bool last_match;
};

struct obs_classification_criterion
{
    int class_id;
    int value_id;
};

struct obs_id_criterion
{
    int nobs_ids;
    int obs_id;
    int *obs_ids;
};

struct mult_obs_classification_criterion
{
    int class_id;
    int nvalues;
    int *value_ids;
};

struct wildcard_obs_classification_criterion
{
    int class_id;
    char *wildclass;
    int ntested;
    int nalloc;
    int nvalues;
    int *value_ids;
};

struct obs_date_criterion
{
    unsigned char date_criterion_type;
    double date;
};

struct obs_stations_criterion
{
    char *station_list;
    char *config_filename;
    char *config_loc;
    void *criteria;
};

/* Single observation criterion, which is configured as a linked list. */

struct obs_criterion
{
    unsigned char crit_type;   // Identifies the criterion type
    bool groupmatch;
    union
    {
        obs_datatype_criterion datatype;
        obs_datafile_criterion datafile;
        obs_classification_criterion classification;
        mult_obs_classification_criterion mult_classification;
        wildcard_obs_classification_criterion wildcard_classification;
        obs_id_criterion id;
        obs_date_criterion date;
        obs_stations_criterion stations;
    } c;
    struct obs_criterion *next;
};

struct obs_criteria
{
    obs_criterion *first;
    obs_criterion *last;
    int id;
    long setid;
    int action;
    int option;
    double factor;
    double factor2;
    struct obs_criteria *next;
    struct obs_criteria *pnext; /* Next criteria to process */
};


struct crit_group_id
{
    obs_criteria *criteria;
    int groupid;
    int valueid;
};

struct obs_criteria_group
{
    int class_id;
    int min_value_id;
    int max_value_id;
    int ncriteria; /* Used to assess how much discrimination provided by group */
    obs_criteria **criteria;
    struct obs_criteria_group *next;
};

#define DFLT_MAX_OFFSETS 256

struct obs_offset_error
{
    int iobs;
    double offsethv;
    double offsetvv;
};

struct obs_modifications
{
    obs_criteria *first;
    obs_criteria *last;
    obs_criteria_group *criteria_groups;
    obs_criteria *ungrouped_criteria;
    bool criteria_prepared;
    network *nw;
    classifications *classes;
    fileid_func get_fileid;
    filename_func get_filename;
    long setid;
    obs_offset_error *offsets;
    int noffsets;
    int maxoffsets;
};

struct obsmod_context
{
    obs_modifications *obsmod;
    survdata *sd; 
    trgtdata *tgt;
    int class_id; /* Cached class value */
    int value_id;
    int action;   /* Action to apply */
    double factor;    /* Obs scale factor */
    double setfactor; /* Obs set scale factor */
    int matchfrom;    /* From station matches criteria */
    int matchto;      /* To station matches criteria */
    double offsethv;   /* Horizontal station offset variance */
    double offsetvv;   /* Vertical station offset variance */
    double centroidhv; /* Horizontal station centroid/basestation variance */
    double centroidvv; /* Vertical station centroid/basestation variance */
};

static void delete_criteria_groups( obs_modifications *obsmod );

/*===============================================================================*/

static obs_criterion *new_obs_criterion()
{
    obs_criterion *oc=(obs_criterion *) check_malloc( sizeof(obs_criterion) );
    oc->crit_type=OBS_CRIT_NONE;
    oc->groupmatch=false;
    oc->next=nullptr;
    return oc;
}

static obs_criterion *new_obs_datatype_criterion( CFG_FILE *cfg, const std::string &datatypes )
{
    int sts=OK;
    std::array<bool,NOBSTYPE> select{};

    // Splits datatypes on '/', one code per field - unbounded field count,
    // so next('/') is called until it fails (no more delimiter found), with
    // remainder() supplying the final field. A trailing '/' at the very end
    // of datatypes does not produce a spurious empty final field, since the
    // loop condition (scanner.remainder().empty()) is already false by then
    // - matching the original char*-scanning loop's own "while(*typecode)"
    // termination exactly (verified by hand-tracing both against
    // "GB/", "GB//SD", "" and plain "GB").
    FieldScanner scanner(datatypes);
    while( ! scanner.remainder().empty() )
    {
        const auto tok=scanner.next('/');
        const std::string typecode( tok ? *tok : scanner.remainder() );
        const int id=datatype_from_code( typecode );
        if( id == NOBSTYPE )
        {
            char errmess[80];
            sprintf(errmess,"Invalid data type code %.20s in observation criteria",typecode.c_str());
            send_config_error( cfg, INVALID_DATA, errmess );
            sts=INVALID_DATA;
        }
        else
        {
            select[id]=true;
        }
        if( ! tok ) break;
    }

    if( sts != OK ) return nullptr;

    obs_criterion * const oc=new_obs_criterion();
    oc->crit_type=OBS_CRIT_DATATYPE;
    std::copy( select.begin(), select.end(), oc->c.datatype.select );
    return oc;
}

static bool obs_datatype_match( obs_criterion *oc, obsmod_context *oac )
{
    return oc->c.datatype.select[oac->tgt->type];
}

static void describe_obs_datatype_criterion( FILE *lst, obs_criterion *oc, const char *prefix )
{
    int ntype=0;
    int itype=0;

    for( int i=0; i<NOBSTYPE; i++ ){ if( oc->c.datatype.select[i]){ ntype++; itype=i;}}

    if( ntype == 1 )
    {
        datatypedef *dtype=datatypedef_from_id(itype);
        fprintf(lst,"which are of type %s (%s)",dtype->code,dtype->name);
    }
    else
    {
        fprintf(lst,"which are of types:");
        for( int i=0; i<NOBSTYPE; i++ )
        {
            if( oc->c.datatype.select[i])
            {
                datatypedef *dtype=datatypedef_from_id(i);
                fprintf(lst,"\n%s        - %s (%s)",prefix, dtype->code, dtype->name);
            }
        }
    }
}

static obs_criterion *new_obs_datafile_criterion( int file_id, const char *filename )
{
    obs_criterion *oc=new_obs_criterion();
    oc->crit_type=OBS_CRIT_DATAFILE;
    oc->c.datafile.file_id=file_id;
    oc->c.datafile.filename=copy_string(filename);
    oc->c.datafile.last_file_id=-1;
    oc->c.datafile.last_match=false;
    return oc;
}

static bool obs_datafile_match_fileid( obs_modifications *obsmod, obs_criterion *oc, int file_id )
{
    if( oc->c.datafile.file_id == OBS_CRIT_WILDCARD_FILEID )
    {
        int last_file_id = oc->c.datafile.last_file_id;
        if( file_id == last_file_id ) return oc->c.datafile.last_match;
        std::string filename = obsmod->get_filename( file_id );
        oc->c.datafile.last_match = filename_wildcard_match(oc->c.datafile.filename,filename);
        return oc->c.datafile.last_match;
    }
    else
    {
        return file_id == oc->c.datafile.file_id;
    }
}

static bool obs_datafile_match( obs_criterion *oc, obsmod_context *oac )
{
    return obs_datafile_match_fileid( oac->obsmod, oc, oac->sd->file );
}

static void delete_obs_datafile_criterion( obs_criterion *oc )
{
    check_free( oc->c.datafile.filename );
    oc->c.datafile.filename=nullptr;
}

static void describe_obs_datafile_criterion( FILE *lst, obs_criterion *oc, const char * )
{
    if( oc->c.datafile.file_id == OBS_CRIT_WILDCARD_FILEID )
    {
        fprintf(lst,"which are from files matching %s",oc->c.datafile.filename);
    }
    else
    {
        fprintf(lst,"which are from file %s",oc->c.datafile.filename);
    }
}

static obs_criterion *new_obs_classification_criterion( CFG_FILE *, classifications *classes,
        const std::string &classification, const std::string &values, const bool singlevalue )
{
    if( ! classes ) return nullptr;
    const int class_id=classes->id( classification, 1 );
    /* If values string contains / then this is a list of multiple classes */
    obs_criterion * const oc=new_obs_criterion();
    if( ! singlevalue && values.find('/') != std::string::npos )
    {
        // A known, fixed field count (number of '/' plus one), so each of
        // the first nval-1 fields comes from next('/') (guaranteed to
        // succeed, since that many delimiters are known to exist) and the
        // last from remainder() - unlike new_obs_datatype_criterion's
        // unbounded split above, a trailing '/' here does produce an empty
        // final field (verified by hand-tracing "A/" against this same
        // precomputed-count algorithm the original char*-based loop used).
        const int nval=std::count( values.begin(), values.end(), '/' ) + 1;

        oc->crit_type=OBS_CRIT_MCLASSIFICATION;
        oc->c.mult_classification.class_id=class_id;
        oc->c.mult_classification.nvalues=nval;
        oc->c.mult_classification.value_ids=(int *) check_malloc( nval*sizeof(int) );

        FieldScanner scanner(values);
        for( int i=0; i<nval; i++ )
        {
            const std::string value( i+1<nval ? *scanner.next('/') : scanner.remainder() );
            oc->c.mult_classification.value_ids[i]=classes->value_id( class_id, value, 1 );
        }
    }
    else if ( ! singlevalue && has_wildcard(values) )
    {
        oc->crit_type=OBS_CRIT_WCLASSIFICATION;
        oc->c.wildcard_classification.class_id=class_id;
        oc->c.wildcard_classification.wildclass=copy_string(values.c_str());
        oc->c.wildcard_classification.nvalues=0;
        oc->c.wildcard_classification.nalloc=0;
        oc->c.wildcard_classification.ntested=0;
        oc->c.wildcard_classification.value_ids=nullptr;
    }
    /* Otherwise a single class */
    else
    {
        oc->crit_type=OBS_CRIT_CLASSIFICATION;
        oc->c.classification.class_id=class_id;
        oc->c.classification.value_id=classes->value_id( class_id, values, 1 );
    }
    return oc;
}

static void delete_mult_obs_classification( obs_criterion *oc )
{
    check_free( oc->c.mult_classification.value_ids );
    oc->c.mult_classification.nvalues = 0;
    oc->c.mult_classification.value_ids = nullptr;
}

static void delete_wildcard_obs_classification( obs_criterion *oc )
{
    check_free( oc->c.wildcard_classification.wildclass );
    oc->c.wildcard_classification.wildclass = nullptr;
    check_free( oc->c.wildcard_classification.value_ids );
    oc->c.wildcard_classification.nvalues = 0;
    oc->c.wildcard_classification.value_ids = nullptr;
}

static int get_context_obs_classification( obsmod_context *oac, int class_id )
{
    if( class_id != oac->class_id ) 
    {
        oac->class_id=class_id;
        oac->value_id=get_obs_classification_id( oac->sd, oac->tgt, class_id );
    }
    return oac->value_id;
}

static bool obs_classification_match( obs_criterion *oc, obsmod_context *oac )
{
    int cclass_id=oc->c.classification.class_id;
    int cvalue_id=oc->c.classification.value_id;
    int value_id = get_context_obs_classification( oac, cclass_id );
    if( cvalue_id == value_id ) return 1;
    return 0;
}

static bool obs_mult_classification_match( obs_criterion *oc, obsmod_context *oac )
{
    int value_id=get_context_obs_classification( oac, oc->c.mult_classification.class_id );
    for( int i=0; i < oc->c.mult_classification.nvalues; i++ )
    {
        if( oc->c.mult_classification.value_ids[i] == value_id ) return true;
    }
    return false;
}

static bool obs_wildcard_classification_match( obs_criterion *oc, obsmod_context *oac )
{
    int cclass_id=oc->c.wildcard_classification.class_id;
    obs_modifications *obsmod = oac->obsmod;
    classifications *csf = obsmod->classes;
    if( ! csf ) return false;
    int class_count=csf->value_count(cclass_id);
    int ntested = oc->c.wildcard_classification.ntested;

    if( class_count > ntested )
    {
        const char *pattern = oc->c.wildcard_classification.wildclass;
        int class_count=csf->value_count(cclass_id);
        int nmatch=0;
        for( int iv = ntested; iv < class_count; iv++ )
        {
            if( wildcard_match(pattern,csf->value_name(cclass_id,iv)) )
            {
                nmatch++;
            }
        }
        if( nmatch > 0 )
        {
            int nvalues = oc->c.wildcard_classification.nvalues;
            int nreq = nvalues + nmatch;
            int *value_ids=oc->c.wildcard_classification.value_ids;

            if( nreq > oc->c.wildcard_classification.nalloc )
            {
                nreq += OBS_CRIT_WILDCLASS_BUFFER;
                value_ids=(int *) check_realloc( (void *) value_ids, nreq*sizeof(int) );
                oc->c.wildcard_classification.value_ids=value_ids;
                oc->c.wildcard_classification.nalloc=nreq;
            }
            for( int iv = ntested; iv < class_count; iv++ )
            {
                if( wildcard_match(pattern,csf->value_name(cclass_id,iv)) )
                {
                    value_ids[nvalues]=iv;
                    nvalues++;
                }
            }
            oc->c.wildcard_classification.nvalues = nvalues;
        }
        oc->c.wildcard_classification.ntested = class_count;
    }

    int value_id=get_context_obs_classification( oac, cclass_id );
    for( int i=0; i < oc->c.wildcard_classification.nvalues; i++ )
    {
        if( oc->c.wildcard_classification.value_ids[i] == value_id ) return true;
    }
    return false;
}

static void describe_obs_classification_criterion( FILE *lst, obs_criterion *oc, const char *, classifications *classes )
{
    fprintf(lst,"where %s classification is \"%s\"",
            classes->name( oc->c.classification.class_id).c_str(),
            classes->value_name( oc->c.classification.class_id, oc->c.classification.value_id).c_str());
}

static void describe_obs_mult_classification_criterion( FILE *lst, obs_criterion *oc, const char *prefix, classifications *classes )
{
    int class_id=oc->c.mult_classification.class_id;

    fprintf(lst,"where %s classification is one of:",
            classes->name( class_id ).c_str());
    for( int i=0; i < oc->c.mult_classification.nvalues; i++ )
    {
        fprintf(lst,"\n%s    - \"%s\"",prefix,
            classes->value_name( class_id, oc->c.mult_classification.value_ids[i]).c_str());
    }
}

static void describe_obs_wildcard_classification_criterion( FILE *lst, obs_criterion *oc, const char *, classifications *classes )
{
    fprintf(lst,"where %s classification matches \"%s\"",
            classes->name( oc->c.wildcard_classification.class_id).c_str(),
            oc->c.wildcard_classification.wildclass );
}


static obs_criterion *new_obs_id_criterion( CFG_FILE *cfg, const std::string &idstr )
{
    // Same fixed-field-count split as new_obs_classification_criterion's
    // multi-value branch above (a trailing '/' does produce an empty final
    // field, parsed and rejected below as an invalid id).
    const int nval=std::count( idstr.begin(), idstr.end(), '/' ) + 1;
    std::vector<int> parsed(nval);

    FieldScanner scanner(idstr);
    for( int i=0; i<nval; i++ )
    {
        const std::string token( i+1<nval ? *scanner.next('/') : scanner.remainder() );
        char chk[2];
        chk[0]=0;
        if( sscanf( token.c_str(), "%d%1s", &parsed[i], chk ) < 1 || chk[0] )
        {
            char errmsg[100];
            sprintf( errmsg,"Invalid observation id \"%.50s\" in observation criteria",token.c_str());
            send_config_error( cfg, INVALID_DATA, errmsg );
            return nullptr;
        }
    }

    obs_criterion * const oc=new_obs_criterion();
    oc->crit_type=OBS_CRIT_ID;
    oc->c.id.obs_id=parsed[0];
    if( nval == 1 )
    {
        oc->c.id.obs_ids=&(oc->c.id.obs_id);
    }
    else
    {
        oc->c.id.obs_ids=new int[nval];
        std::copy( parsed.begin(), parsed.end(), oc->c.id.obs_ids );
    }
    oc->c.id.nobs_ids=nval;
    return oc;
}

static void delete_obs_id_criterion( obs_criterion *oc )
{
    if( oc->c.id.nobs_ids > 1 ){delete[] oc->c.id.obs_ids; oc->c.id.obs_ids=nullptr; }
}

static bool obs_id_match( obs_criterion *oc, obsmod_context *oac )
{
    for( int i=0; i < oc->c.id.nobs_ids; i++ ){ if( oc->c.id.obs_ids[i] == oac->tgt->id ) return true; }
    return false;
}

static void describe_obs_id_criterion( FILE *lst, obs_criterion *oc, const char *prefix )
{
    if( oc->c.id.nobs_ids == 1 )
    {
        fprintf(lst,"where the observation id is %d",oc->c.id.obs_id);
    }
    else
    {
        fprintf(lst,"where the observation id is one of:");
        for( int i=0; i < oc->c.id.nobs_ids; i++ )
        {
            fprintf(lst,"\n%s    - %d", prefix, oc->c.id.obs_ids[i]);
        }
    }
}

static obs_criterion *new_obs_date_criterion( CFG_FILE *cfg, unsigned char date_crit_type, const std::string &datestr )
{
    obs_criterion *oc;
    double date=UNDEFINED_DATE;

    if( date_crit_type == OBS_CRIT_DATE_BEFORE || date_crit_type == OBS_CRIT_DATE_AFTER )
    {
        date=snap_datetime_parse(datestr);
        if( date == UNDEFINED_DATE )
        {
            char errmsg[100];
            sprintf( errmsg,"Invalid date \"%.50s\" in observation date criteria",datestr.c_str());
            send_config_error( cfg, INVALID_DATA, errmsg );
            return nullptr;
        }
    }
    else
    {
        date_crit_type = OBS_CRIT_DATE_UNKNOWN;
    }
    oc=new_obs_criterion();
    oc->crit_type=OBS_CRIT_DATE;
    oc->c.date.date_criterion_type=date_crit_type;
    oc->c.date.date=date;
    return oc;
}

static bool obs_date_match( obs_criterion *oc, obsmod_context *oac )
{
    bool result=false;
    double date=oac->sd->date;

    switch( oc->c.date.date_criterion_type )
    {
        case OBS_CRIT_DATE_BEFORE: 
            result=date != UNDEFINED_DATE && date < oc->c.date.date; 
            break;
        case OBS_CRIT_DATE_AFTER: 
            result=date != UNDEFINED_DATE && date > oc->c.date.date; 
            break;
        default: 
            result=date == UNDEFINED_DATE; 
            break;
    }
    return result;
}

static void describe_obs_date_criterion( FILE *lst, obs_criterion *oc, const char * )
{
    if( oc->c.date.date_criterion_type == OBS_CRIT_DATE_UNKNOWN )
    {
        fprintf(lst,"which have no observation date");
    }
    else
    {
        fprintf(lst,"which are observed %s %s",
            oc->c.date.date_criterion_type == OBS_CRIT_DATE_BEFORE ? 
            "before" : "after",
            date_as_string(oc->c.date.date,"DT?",0) );
    }
}

static void init_obs_stations_criterion( obs_criterion *oc, network *nw  )
{
    void *psc=new_station_criteria();
    set_error_location( oc->c.stations.config_loc );
    int sts=compile_station_criteria( psc, nw, 
            oc->c.stations.station_list,
            oc->c.stations.config_filename );
    set_error_location( nullptr );
    if( sts != OK ) 
    { 
        delete_station_criteria( psc ); 
        psc=new_station_criteria(); 
    }
    setup_station_criteria_cache( psc, number_of_stations( nw ) );
    oc->c.stations.criteria = psc;
}

static obs_criterion *new_obs_stations_criterion( CFG_FILE *cfg, unsigned char station_crit_type, const std::string &station_list )
{
    obs_criterion *oc;
    if( station_crit_type != OBS_CRIT_STATION_BETWEEN ) station_crit_type=OBS_CRIT_STATION_USES;
    oc=new_obs_criterion();
    oc->crit_type=station_crit_type;
    oc->c.stations.config_loc=copy_string(get_config_location(cfg).c_str());
    oc->c.stations.config_filename=copy_string(get_config_filename(cfg).c_str());
    oc->c.stations.station_list=copy_string(station_list.c_str());
    oc->c.stations.criteria = nullptr;
    return oc;
}

static void delete_obs_stations_criterion( obs_criterion *oc )
{
    check_free( oc->c.stations.station_list );
    check_free( oc->c.stations.config_loc );
    check_free( oc->c.stations.config_filename );
    if( oc->c.stations.criteria ) delete_station_criteria( oc->c.stations.criteria );
    oc->c.stations.station_list = nullptr;
    oc->c.stations.config_loc = nullptr;
    oc->c.stations.config_filename = nullptr;
    oc->c.stations.criteria=nullptr;
}
    
static bool obs_stations_match( obs_criterion *oc, obsmod_context *oac )
{
    network *nw=oac->obsmod->nw;
    if( ! oc->c.stations.criteria )
    {
        init_obs_stations_criterion( oc, nw  );
    }

    int fromstn=oac->sd->from;
    int tostn = oac->tgt->to;

    bool matchfrom=true;
    bool matchuse=false;
    if( fromstn > 0 )
    {
        matchfrom=station_criteria_match( oc->c.stations.criteria, station_ptr( nw, fromstn ));
        matchuse=matchfrom;
    }
    bool matchto=true;
    if( tostn > 0 )
    {
        matchto=station_criteria_match( oc->c.stations.criteria, station_ptr( nw, tostn ));
        matchuse=matchuse || matchto;
    }
    if( ! matchfrom )
    {
        oac->matchfrom=false;
    }
    if( ! matchto )
    {
        oac->matchto=false;
    }
    if( oc->crit_type == OBS_CRIT_STATION_USES )
    {
        return matchuse;
    }
    return matchfrom && matchto;
}

static void describe_obs_stations_criterion( FILE *lst, obs_criterion *oc, const char * )
{
    fprintf(lst,"which %s stations %s",
            oc->crit_type == OBS_CRIT_STATION_USES ? "use" : "are between",
            oc->c.stations.station_list );
}

static bool obs_criterion_match( obs_criterion *oc, obsmod_context *oac )
{
    if( oc->groupmatch ) return true;
    switch( oc->crit_type )
    {
        case OBS_CRIT_DATATYPE: return  obs_datatype_match( oc, oac );
        case OBS_CRIT_DATAFILE: return  obs_datafile_match( oc, oac );
        case OBS_CRIT_CLASSIFICATION: return  obs_classification_match( oc, oac );
        case OBS_CRIT_MCLASSIFICATION: return  obs_mult_classification_match( oc, oac );
        case OBS_CRIT_WCLASSIFICATION: return  obs_wildcard_classification_match( oc, oac );
        case OBS_CRIT_ID: return  obs_id_match( oc, oac );
        case OBS_CRIT_DATE: return  obs_date_match( oc, oac );
        case OBS_CRIT_STATION_USES: return  obs_stations_match( oc, oac );
        case OBS_CRIT_STATION_BETWEEN: return  obs_stations_match( oc, oac );
    }
    return false;
}

static void delete_obs_criterion( obs_criterion *oc )
{
    switch( oc->crit_type )
    {
        case OBS_CRIT_DATAFILE:        delete_obs_datafile_criterion( oc ); break;
        case OBS_CRIT_STATION_USES: 
        case OBS_CRIT_STATION_BETWEEN: delete_obs_stations_criterion( oc ); break;
        case OBS_CRIT_MCLASSIFICATION: delete_mult_obs_classification( oc ); break;
        case OBS_CRIT_WCLASSIFICATION: delete_wildcard_obs_classification( oc ); break;
        case OBS_CRIT_ID: delete_obs_id_criterion( oc ); break;
    }
    check_free( oc );
}

static obs_criteria *new_obs_criteria( int action, double factor, double factor2, int option )
{
    obs_criteria *ocr=(obs_criteria *) check_malloc( sizeof( obs_criteria ) );
    if( action & OBS_MOD_REWEIGHT_SET ){ action &= ~ (int) OBS_MOD_REWEIGHT; }
    ocr->action=action;
    ocr->factor=factor;
    ocr->factor2=factor2;
    ocr->option=option;
    ocr->first=nullptr;
    ocr->last=nullptr;
    ocr->next=nullptr;
    ocr->pnext=nullptr;
    ocr->setid=0;
    return ocr;
}

static void delete_obs_criteria( obs_criteria *ocr )
{
    while( ocr->first )
    {
        obs_criterion *oc=ocr->first;
        ocr->first=oc->next;
        delete_obs_criterion( oc );
    }
    ocr->last=nullptr;
    check_free( ocr );
}

static void add_obs_criterion_to_criteria( obs_criteria *ocr, obs_criterion *oc )
{
    if( ocr->last )
    {
        ocr->last->next=oc;
        ocr->last=oc;
    }
    else
    {
        ocr->first=ocr->last=oc;
    }
}

/* Cumulate action and reweight factor for list of criteria */

static void apply_obs_criteria_action( obs_criteria *ocr, obsmod_context *oac )
{
    int action=oac->action;
    if( ! (action & OBS_MOD_IGNORE ) )
    {
        oac->matchfrom=true;
        oac->matchto=true;
        for( obs_criterion *oc=ocr->first; oc; oc=oc->next )
        {
            if( ! obs_criterion_match( oc, oac ) )
            {
                return;
            }
        }

        if( ocr->action & OBS_MOD_IGNORE )
        {
            action = OBS_MOD_IGNORE;
        }
        else if ( ocr->action & OBS_MOD_ANTENNA_OFFSET )
        {
            if( (oac->tgt->type == GB || oac->tgt->type == GX) && oac->matchto )
            {
                oac->tgt->tohgt += ocr->factor;
            }
            if( oac->tgt->type==GB && oac->matchfrom &&  ocr->setid != oac->obsmod->setid ) 
            {
                ocr->setid=oac->obsmod->setid;
                oac->sd->fromhgt += ocr->factor;
            }            
        }
        else
        {
            action |= ocr->action;
            if( ocr->action & OBS_MOD_REWEIGHT ) oac->factor *= ocr->factor;
            else if( ocr->action & OBS_MOD_OFFSET_ERROR ) 
            {
                if( oac->matchto )
                {
                    oac->offsethv += ocr->factor*ocr->factor;
                    oac->offsetvv += ocr->factor2*ocr->factor2;
                }
                if( oac->tgt->type==GB && oac->matchfrom &&  ocr->setid != oac->obsmod->setid ) 
                {
                    ocr->setid=oac->obsmod->setid;
                    oac->centroidhv += (ocr->factor*ocr->factor);
                    oac->centroidvv += (ocr->factor2*ocr->factor2);
                }
            }
            else if( ocr->action & OBS_MOD_REWEIGHT_SET )
            {
                if( ocr->setid != oac->obsmod->setid ) 
                {
                    ocr->setid=oac->obsmod->setid;
                    oac->setfactor *= ocr->factor;
                }
            }
            else if( ocr->action & OBS_MOD_CENTROID_ERROR )
            {
                if( ocr->setid != oac->obsmod->setid ) 
                {
                    ocr->setid=oac->obsmod->setid;
                    oac->centroidhv += (ocr->factor*ocr->factor);
                    oac->centroidvv += (ocr->factor2*ocr->factor2);
                }
            }
        }
        if( ocr->action & OBS_MOD_SET_OPTION )
        {
            oac->sd->options |= ocr->option;
        }
        else if( ocr->action & OBS_MOD_UNSET_OPTION )
        {
            oac->sd->options &= ~(ocr->option);
        }
    }
    oac->action=action;
}

static bool obs_criteria_ignore_datafile( obs_modifications *obsmod, obs_criteria *ocr, int file_id )
{
    if( ! (ocr->action & OBS_MOD_IGNORE ) ) return false;
    bool matched=false;
    for( obs_criterion *oc=ocr->first; oc; oc=oc->next )
    {
        if( oc->crit_type != OBS_CRIT_DATAFILE ) return false;
        if( obs_datafile_match_fileid( obsmod, oc, file_id ))
        { 
            matched=true;
        }
    }
    return matched;
}

static void summarize_obs_criteria( FILE *lst, const char *prefix, obs_criteria *ocr, classifications *classes )
{
    if( ! ocr->first )
    {
        fprintf(lst,"%s  All observations\n", prefix);
        return;
    }

    bool just1=ocr->first->next == nullptr;

    if( ocr->action == OBS_MOD_REWEIGHT_SET )
    {
        fprintf(lst,"%s  Observations in sets including one or more observations%s", prefix, just1 ? " " : ":");
    }
    else
    {
        fprintf(lst,"%s  Observations%s", prefix, just1 ? " " : ":");
    }
    int ncrit=0;
    for( obs_criterion *oc=ocr->first; oc; oc=oc->next )
    {
        ncrit++;
        if( ncrit > 1 ) fprintf(lst,", and");
        if( ! just1 ) fprintf(lst,"\n%s    - ",prefix);
        switch( oc->crit_type )
        {
            case OBS_CRIT_DATATYPE: 
                describe_obs_datatype_criterion( lst, oc, prefix );
                break;
            case OBS_CRIT_DATAFILE: 
                describe_obs_datafile_criterion( lst, oc, prefix );
                break;
            case OBS_CRIT_CLASSIFICATION: 
                describe_obs_classification_criterion( lst, oc, prefix, classes );
                break;
            case OBS_CRIT_MCLASSIFICATION: 
                describe_obs_mult_classification_criterion( lst, oc, prefix, classes );
                break;
            case OBS_CRIT_WCLASSIFICATION: 
                describe_obs_wildcard_classification_criterion( lst, oc, prefix, classes );
                break;
            case OBS_CRIT_ID: 
                describe_obs_id_criterion( lst, oc, prefix );
                break;
            case OBS_CRIT_DATE: 
                describe_obs_date_criterion( lst, oc, prefix );
                break;
            case OBS_CRIT_STATION_USES: 
            case OBS_CRIT_STATION_BETWEEN: 
                describe_obs_stations_criterion( lst, oc, prefix ); 
                break;
        }
    }
    fprintf(lst,"\n");
}

void *new_obs_modifications( network *nw, classifications *obs_classes )
{
    obs_modifications *obsmod = (obs_modifications *) check_malloc( sizeof( obs_modifications ) );
    obsmod->first=nullptr;
    obsmod->last=nullptr;
    obsmod->criteria_groups=nullptr;
    obsmod->ungrouped_criteria=nullptr;
    obsmod->criteria_prepared=false;
    obsmod->nw=nw;
    obsmod->classes=obs_classes;
    obsmod->get_fileid=nullptr;
    obsmod->get_filename=nullptr;
    obsmod->setid=0;
    obsmod->offsets=nullptr;
    obsmod->noffsets=0;
    obsmod->maxoffsets=0;
    return (void *) obsmod;
}

void delete_obs_modifications( void *pobsmod )
{
    obs_modifications *obsmod=(obs_modifications *)pobsmod;
    obs_criteria *ocr=obsmod->first;
    while( ocr )
    {
        obs_criteria *next=ocr->next;
        delete_obs_criteria( ocr );
        ocr=next;
    }
    obsmod->first=nullptr;
    obsmod->last=nullptr;
    delete_criteria_groups( obsmod );
    if( obsmod->offsets )
    {
        check_free(obsmod->offsets);
    }
    obsmod->maxoffsets=0;
    obsmod->maxoffsets=0;
}

void set_obs_modifications_network( void *pobsmod, network *nw )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obsmod->nw=nw;
}

void set_obs_modifications_file_func( void *pobsmod, fileid_func idfunc, filename_func namefunc )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obsmod->get_fileid=idfunc;
    obsmod->get_filename=namefunc;
}

static void add_obs_criteria_to_modifications( obs_modifications *obsmod, obs_criteria *ocr )
{
    if( obsmod->last )
    {
        obsmod->last->next=ocr;
        obsmod->last=ocr;
    }
    else
    {
        obsmod->first=obsmod->last=ocr;
    }
    obsmod->criteria_prepared=false;
}


static int get_file_id( obs_modifications *obsmod, CFG_FILE *cfg, std::string_view datafile, int missing_error )
{
    if( ! obsmod->get_fileid )
    {
        handle_error( INTERNAL_ERROR,
            "Program error: Survey file id function not initialised in observation modifications",
            nullptr );
        return -1;
    }

    int file_id = obsmod->get_fileid( datafile, current_file_context() );
    if( file_id < 0 && missing_error != OK )
    {
        char errmess[120];
        sprintf(errmess,"Invalid data_file %.60s in classification command",std::string(datafile).c_str());
        send_config_error( cfg, missing_error, errmess );
    }
    return file_id;
}

/// Parses a "key=value" observation selection criteria field, e.g.
/// data_type=GB, data_file=*1.dat (unquoted - '*' is a wildcard), or
/// data_file='t??t1.dat' (quoted - '?' is literal, not a wildcard).
/// A quoted value may itself contain whitespace (e.g. equpt=' quote '),
/// spanning further next() fields to find its closing quote - see
/// quotedValue(). Any key other than data_type/data_file/id becomes a
/// classification criterion instead. Returns nullptr if the criterion is
/// invalid or the value is missing/malformed; the error is already
/// reported via send_config_error before returning.
static obs_criterion *parse_key_value_criterion(
    CFG_FILE *cfg,                 ///< current config file, for error reporting
    obs_modifications *obsmod,     ///< owns the classification map and data-file lookup used by some keys
    FieldScanner &scanner,         ///< field cursor; advanced past a multi-field quoted value
    std::string_view field,        ///< the whole "key=value" field, for error messages
    std::string_view key,          ///< the part of field before '='
    std::string_view valuePart,    ///< the part of field after '=', not yet unquoted
    int missing_error )            ///< error severity to use if a data_file value doesn't resolve
{
    if( key.empty() || valuePart.empty() )
    {
        char errmess[100];
        std::string fieldText(field);
        sprintf(errmess,"Invalid observation selection criteria \"%.40s\"",fieldText.c_str());
        send_config_error(cfg,INVALID_DATA,errmess);
        return nullptr;
    }

    // quoted disables wildcard-pattern interpretation for this value - e.g.
    // data_file=t??t1.dat matches multiple files via wildcard, but
    // data_file='t??t1.dat' means a file literally named t??t1.dat.
    auto quoteStart = FieldScanner::isQuoted( valuePart, false );
    const bool quoted = quoteStart != valuePart.end();
    std::string value;
    if( quoted )
    {
        static const std::optional<std::vector<QuoteFollowOption>> quoteMustBeFollowedBy{
            std::vector<QuoteFollowOption>{ QuoteFollowOption::Whitespace, QuoteFollowOption::End } };
        char quoteChar=*quoteStart;
        auto beforeQuote=scanner.remainder();
        auto quotedResult=scanner.quotedValue( quoteStart+1, quoteChar, quoteMustBeFollowedBy );
        if( ! quotedResult )
        {
            char errmess[100];
            // quotedValue() can leave the scanner further along, or further
            // back, than it was before the call: key="my value
            // (unterminated) runs off the end, further along; key="ab"x
            // leaves it further back, since the closing quote is rejected
            // by quoteMustBeFollowedBy (not followed by whitespace).
            // Whichever remainder is shorter is further along.
            auto afterQuote = scanner.remainder();
            auto messageEnd = afterQuote.size() < beforeQuote.size() ? afterQuote : beforeQuote;
            std::string fieldText( field.data(), messageEnd.data() - field.data() );
            sprintf(errmess,"Invalid observation selection criteria for \"%.40s\"",fieldText.c_str());
            send_config_error(cfg,INVALID_DATA,errmess);
            return nullptr;
        }
        value.assign(*quotedResult);
    }
    else
    {
        value.assign(valuePart);
    }

    if( boost::algorithm::iequals(key,"data_type") )
    {
        return new_obs_datatype_criterion(cfg,value);
    }
    if( boost::algorithm::iequals(key,"data_file") )
    {
        int file_id=OBS_CRIT_WILDCARD_FILEID;
        if( quoted || ! has_wildcard(value) )
        {
            file_id=get_file_id( obsmod, cfg, value.data(), missing_error );
            if( file_id < 0 ) return nullptr;
        }
        return new_obs_datafile_criterion(file_id,value.c_str());
    }
    if( boost::algorithm::iequals(key,"id") )
    {
        return new_obs_id_criterion(cfg,value);
    }
    const std::string keyText(key);
    return new_obs_classification_criterion(cfg, obsmod->classes, keyText, value, quoted );
}

/// Parses a using_stations/between_stations ... end_stations span, capturing
/// the station list verbatim (preserving the original spacing between
/// names) between the two keywords. scanner is positioned right after the
/// opening keyword; it is advanced to just past end_stations (or to end of
/// text if end_stations is never found). Returns nullptr if the station
/// list itself is missing; the error is already reported via
/// send_config_error before returning.
static obs_criterion *parse_stations_criterion(
    CFG_FILE *cfg,              ///< current config file, for error reporting
    FieldScanner &scanner,      ///< field cursor, positioned after using_stations/between_stations
    int station_crit_type )     ///< OBS_CRIT_STATION_USES or OBS_CRIT_STATION_BETWEEN
{
    auto stationField = scanner.next();
    if( ! stationField )
    {
        send_config_error(cfg,INVALID_DATA,"Missing station list in observation selection criteria");
        return nullptr;
    }
    auto start = stationField->data();
    while( stationField && ! boost::algorithm::iequals(*stationField,"end_stations") )
    {
        stationField = scanner.next();
    }
    auto stop = stationField ? stationField->data() : scanner.remainder().data();
    std::string stationList( start, stop - start );
    return new_obs_stations_criterion( cfg, station_crit_type, stationList );
}

static int add_obs_modifications_imp( CFG_FILE *cfg, void *pobsmod, std::string_view criteria, int action, int option, double errval1, double errval2 )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obs_criteria *ocr=new_obs_criteria( action, errval1, errval2, option );
    FieldScanner scanner( criteria );
    int sts=OK;
    int missing_error=INVALID_DATA;
    bool have_criterion=false;
    std::optional<std::string_view> field;
    while( (field=scanner.next()) )
    {
        obs_criterion *oc=nullptr;

        if( boost::algorithm::iequals(*field,"ignore_missing") ){ missing_error=OK; continue; }
        if( boost::algorithm::iequals(*field,"warn_missing") ){ missing_error=INFO_ERROR; continue; }
        if( boost::algorithm::iequals(*field,"fail_missing") ){ missing_error=INVALID_DATA; continue; }
        if( boost::algorithm::iequals(*field,"all_observations") ){ have_criterion=true; continue; }

        auto eqPos=field->find('=');
        if( eqPos != std::string_view::npos )
        {
            auto key=field->substr(0,eqPos);
            auto valuePart=field->substr(eqPos+1);
            oc=parse_key_value_criterion(cfg,obsmod,scanner,*field,key,valuePart,missing_error);
        }
        else if( boost::algorithm::iequals(*field,"before") )
        {
            auto dateField=scanner.next();
            if( ! dateField )
            {
                send_config_error(cfg,INVALID_DATA,"Missing date in before observation selection criteria");
                sts=INVALID_DATA;
            }
            else
            {
                std::string dateText(*dateField);
                oc=new_obs_date_criterion( cfg, OBS_CRIT_DATE_BEFORE, dateText );
            }
        }
        else if( boost::algorithm::iequals(*field,"after") )
        {
            auto dateField=scanner.next();
            if( ! dateField )
            {
                send_config_error(cfg,INVALID_DATA,"Missing date in after observation selection criteria");
                sts=INVALID_DATA;
            }
            else
            {
                std::string dateText(*dateField);
                oc=new_obs_date_criterion( cfg, OBS_CRIT_DATE_AFTER, dateText );
            }
        }
        else if( boost::algorithm::iequals(*field,"date_unknown") )
        {
            oc=new_obs_date_criterion( cfg, OBS_CRIT_DATE_UNKNOWN, std::string() );
        }
        else if( boost::algorithm::iequals(*field,"using_stations") ||
                boost::algorithm::iequals(*field,"between_stations") )
        {
            int station_crit_type= boost::algorithm::iequals(*field,"between_stations") ?
                OBS_CRIT_STATION_BETWEEN : OBS_CRIT_STATION_USES;
            oc=parse_stations_criterion(cfg,scanner,station_crit_type);
        }
        else
        {
            char errmess[120];
            std::string fieldText(*field);
            sprintf(errmess,"Invalid specification %.50s in observation selection criteria",fieldText.c_str());
            send_config_error(cfg,INVALID_DATA,errmess);
            sts=INVALID_DATA;
        }
        if( oc )
        {
            add_obs_criterion_to_criteria( ocr, oc );
            have_criterion=true;
        }
        else
        {
            sts=INVALID_DATA;
        }
    }
    if( sts == OK && ! have_criterion )
    {
        send_config_error(cfg,INVALID_DATA,"Missing observation selection criteria");
        sts=INVALID_DATA;
    }
    if( sts == OK )
    {
        add_obs_criteria_to_modifications( obsmod, ocr );
    }
    else
    {
        delete_obs_criteria( ocr );
    }
    /* All errors reported so return OK */
    return OK;
}

int add_obs_modifications( CFG_FILE *cfg, void *pobsmod, std::string_view criteria, int action, double errval1, double errval2 )
{
    return add_obs_modifications_imp( cfg, pobsmod, criteria, action, 0, errval1, errval2);
}

int add_obs_option_modification( CFG_FILE *cfg, void *pobsmod, std::string_view criteria, int set, int option )
{
    int action=set ? OBS_MOD_SET_OPTION : OBS_MOD_UNSET_OPTION;
    return add_obs_modifications_imp( cfg, pobsmod, criteria,action,option,1.0,0.0);
}

int add_obs_modifications_classification( CFG_FILE *cfg, void *pobsmod, std::string_view classification, std::string_view value, int action, double err_factor, int missing_error )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obs_criterion *oc=nullptr;

    if( boost::algorithm::iequals(classification,"data_type") )
    {
        oc=new_obs_datatype_criterion(cfg,std::string(value));
    }
    else if( boost::algorithm::iequals(classification,"data_file") )
    {
        int file_id=get_file_id( obsmod, cfg, value, missing_error );
        if( file_id >= 0 ) oc=new_obs_datafile_criterion( file_id,std::string(value).c_str());
    }
    else if( boost::algorithm::iequals(classification,"id") )
    {
        oc=new_obs_id_criterion(cfg,std::string(value));
    }
    else
    {
        oc=new_obs_classification_criterion(cfg, obsmod->classes, std::string(classification), std::string(value), true );
    }
    if( ! oc )
    {
        return INVALID_DATA;
    }
    obs_criteria *ocr=new_obs_criteria( action, err_factor, 0.0, 0 );
    add_obs_criterion_to_criteria( ocr, oc );
    add_obs_criteria_to_modifications( obsmod, ocr );
    return OK;
}

int add_obs_modifications_datafile_factor( CFG_FILE *, void *pobsmod, int fileid, const std::string &filename, double err_factor )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obs_criterion *oc = new_obs_datafile_criterion( fileid, filename.c_str());
    obs_criteria *ocr=new_obs_criteria( OBS_MOD_REWEIGHT, err_factor, 0.0, 0 );
    add_obs_criterion_to_criteria( ocr, oc );
    add_obs_criteria_to_modifications( obsmod, ocr );
    return OK;
}

static obs_criteria *criteria_group_match( obs_criteria_group *ocg, obsmod_context *oac )
{
    int cclass_id=ocg->class_id;
    int value_id;
    if( cclass_id > 0 )
    {
        value_id=get_context_obs_classification( oac, cclass_id );
    }
    else
    {
        value_id=oac->sd->file;
    }
    if( value_id >= ocg->min_value_id && value_id <= ocg->max_value_id )
    {
        return ocg->criteria[value_id-ocg->min_value_id];
    }
    return nullptr;
}


/* Apply action for a group of criteria linked by pnext.  Return 0 if ignoring */

static void apply_obs_group_action( obs_criteria *ocr, obsmod_context *oac )
{
    if( ! ocr ) return;
    for( ; ocr; ocr = ocr->pnext )
    {
        apply_obs_criteria_action(ocr,oac);
        if( oac->action & OBS_MOD_IGNORE ) 
        {
            oac->tgt->unused |= IGNORE_OBS_BIT;
            oac->factor=1.0;
            return;
        }
    }

    if( oac->action & OBS_MOD_REJECT )
    {
        oac->tgt->unused |= REJECT_OBS_BIT;
    }
}

static void apply_obs_modification_action( obs_modifications *obsmod, obsmod_context *oac )
{
    if( oac->tgt->unused & IGNORE_OBS_BIT ) return;

    for( obs_criteria_group *ocg=obsmod->criteria_groups; ocg; ocg=ocg->next )
    {
        obs_criteria *ocr=criteria_group_match( ocg, oac );
        if( ocr ) 
        {
            /* Return if observation is being ignored */
            apply_obs_group_action( ocr, oac );
            if( oac->action & OBS_MOD_IGNORE ) return;
        }
    }
    if( obsmod->ungrouped_criteria ) apply_obs_group_action( obsmod->ungrouped_criteria, oac );
    return;
}

/* Sort the criteria into groups based on classification tests and file id
 * tests. Then only need to go through these groups to find matching criteria,
 * which avoids testing every different value of each one.
 * To get best efficiency choose group in which criteria goes based on
 * which classifications are most used.
 * 
 * 
 */

static obs_criteria_group *create_obs_criteria_group(int groupid,int minval,int maxval)
{
    obs_criteria_group *ocg=(obs_criteria_group *) check_malloc( sizeof(obs_criteria_group) );
    ocg->class_id=groupid;
    ocg->min_value_id=minval;
    ocg->max_value_id=maxval;
    int ncrit=maxval-minval+1;
    ocg->ncriteria=0;
    ocg->criteria=(obs_criteria **) check_malloc( ncrit * sizeof(obs_criteria *) );
    for( int i = 0; i < ncrit; i++ ) ocg->criteria[i]=nullptr;
    return ocg;
}

static void add_criteria_to_group( obs_criteria_group *ocg, obs_criteria *ocr, int valueid )
{
    if( valueid < ocg->min_value_id || valueid > ocg->max_value_id ) return;
    valueid -= ocg->min_value_id;
    if( ! ocg->criteria[valueid] ) ocg->ncriteria++;
    ocr->pnext=ocg->criteria[valueid];
    ocg->criteria[valueid]=ocr;
}

static void delete_criteria_groups( obs_modifications *obsmod )
{
    obs_criteria_group *ocg=obsmod->criteria_groups;
    obsmod->criteria_groups=nullptr;
    obsmod->ungrouped_criteria=nullptr;
    while( ocg )
    {
        obs_criteria_group *next_group=ocg->next;
        check_free(ocg->criteria);
        check_free(ocg);
        ocg=next_group;
    }
}

static void prepare_obs_modifications( obs_modifications *obsmod )
{
    if( obsmod->criteria_prepared ) return;
    delete_criteria_groups( obsmod );
    obsmod->criteria_prepared=true;
    if( ! obsmod->first ) return;

    /* Count class usage to select preferred class for grouping criteria */
    /* Note: classification ids are 1 based.  Use 0 for file_id */

    int nclass=obsmod->classes->count()+1;
    int *class_count=(int *) check_malloc(nclass*sizeof(int));
    for( int i = 0; i < nclass; i++ ) class_count[i]=0;
    int ncriteria=0;
    for( obs_criteria *ocr=obsmod->first; ocr; ocr=ocr->next )
    {
        ncriteria++;
        for( obs_criterion *oc=ocr->first; oc; oc=oc->next )
        {
            if( oc->crit_type == OBS_CRIT_CLASSIFICATION )
            {
                class_count[oc->c.classification.class_id]++;
            }   
            else if( oc->crit_type == OBS_CRIT_DATAFILE  && oc->c.datafile.file_id != OBS_CRIT_WILDCARD_FILEID )
            {
                class_count[0]++;
            }
           }
    }

    /* Identify the group and id for each criteria */
    /* Don't add option criteria to groups as need to be processed in order */

    crit_group_id *grpid = (crit_group_id *) check_malloc( ncriteria *sizeof(crit_group_id) );
    ncriteria=0;
    for( obs_criteria *ocr=obsmod->first; ocr; ocr=ocr->next, ncriteria++ )
    {
        int maxcount=0;
        int groupid=-1;
        int valueid=0;
        grpid[ncriteria].criteria=ocr;
        grpid[ncriteria].groupid=groupid;
        grpid[ncriteria].valueid=valueid;
        if( ocr->action & (OBS_MOD_SET_OPTION | OBS_MOD_UNSET_OPTION | OBS_MOD_ANTENNA_OFFSET )) continue;
        obs_criterion *groupoc=nullptr;
        ocr->pnext=nullptr; 
        for( obs_criterion *oc=ocr->first; oc; oc=oc->next )
        {
            int clsid;
            int clsval;
            oc->groupmatch=false;
            if( oc->crit_type == OBS_CRIT_CLASSIFICATION )
            {
                clsid=oc->c.classification.class_id;
                clsval=oc->c.classification.value_id;
            }
            else if( oc->crit_type == OBS_CRIT_DATAFILE && oc->c.datafile.file_id != OBS_CRIT_WILDCARD_FILEID )
            {
                clsid=0;
                clsval=oc->c.datafile.file_id;
            }
            else
            {
                continue;
            }
            if( class_count[clsid] > maxcount )
            {
                maxcount=class_count[clsid];
                groupid=clsid;
                valueid=clsval;
                groupoc=oc;
            }
        }
        grpid[ncriteria].groupid=groupid;
        grpid[ncriteria].valueid=valueid;
        if( groupoc ) groupoc->groupmatch=true;
    }

    check_free(class_count);

    /* Create the group for each classid */

    obs_criteria_group **group = (obs_criteria_group **) check_malloc(sizeof(obs_criteria_group *)*nclass);
    for( int i=0; i<nclass; i++ ) group[i]=nullptr;

    obs_criteria *nogroup=nullptr;

    /* Assign criteria to groups, creating when needed */

    for( int i=0; i<ncriteria; i++ )
    {
        crit_group_id *grpi=&(grpid[i]);
        int groupid=grpi->groupid;
        obs_criteria *ocr=grpi->criteria;
        if( groupid < 0 ) 
        {
            ocr->pnext=nogroup;
            nogroup=ocr;
            continue;
        }
        if( ! group[groupid] )
        {
            int minval=grpid[i].valueid;
            int maxval=minval;
            for( int j = i+1; j<ncriteria; j++ )
            {
                crit_group_id *grpj=&(grpid[j]);
                if( grpj->groupid == groupid )
                {
                    int valj=grpj->valueid;
                    if( valj < minval ) { minval=valj; }
                    else if (valj > maxval ) { maxval=valj; }
                }
            }
            group[groupid]=create_obs_criteria_group(groupid,minval,maxval);
        }
        if( group[groupid] )
        {
            add_criteria_to_group( group[groupid], ocr, grpi->valueid );
        }
        else
        {
            ocr->pnext=nogroup;
            nogroup=ocr;
        }
    }

    check_free(grpid);

    /* Ungrouped set is reversed. Undo this for set criteria so they
     * are applied in the correct order 
     */

    if( nogroup )
    {
        obs_criteria *last=0;
        while( nogroup )
        {
            obs_criteria *ocr=nogroup;
            nogroup=ocr->pnext;
            ocr->pnext=last;
            last=ocr;
        }
        nogroup=last;
    }

    /* Sort criteria groups by reverse count of criteria  and add to obsmod */

    for( int i = 0; i < nclass; i++ )
    {
        obs_criteria_group *grpi=group[i];
        if( ! grpi ) continue;
        obs_criteria_group **grp = &(obsmod->criteria_groups);
        while( *grp && (*grp)->ncriteria > grpi->ncriteria ) grp=&((*grp)->next);
        grpi->next=*grp;
        *grp=grpi;
    }

    check_free(group);

    obsmod->ungrouped_criteria=nogroup;
}

static void init_obsmod_context_set( obsmod_context *oac, obs_modifications *obsmod, survdata *sd )
{
    oac->obsmod=obsmod;
    oac->setfactor=1.0;
    oac->centroidhv=0.0;
    oac->centroidvv=0.0;
    oac->sd=sd;
}

static void obsmod_add_offset( obs_modifications *obsmod, int iobs, double offsethv, double offsetvv )
{
    obs_offset_error *ooe;
    if( obsmod->maxoffsets == 0 )
    {
        obsmod->maxoffsets=DFLT_MAX_OFFSETS;
        obsmod->offsets=(obs_offset_error *) check_malloc( sizeof(obs_offset_error)*DFLT_MAX_OFFSETS );
    }
    else if( obsmod->noffsets >= obsmod->maxoffsets )
    {
        obsmod->maxoffsets *= 2;
        obsmod->offsets=(obs_offset_error *) check_realloc( obsmod->offsets, sizeof(obs_offset_error)*obsmod->maxoffsets );
    }
    ooe=obsmod->offsets+obsmod->noffsets;
    obsmod->noffsets++;
    ooe->iobs=iobs;
    ooe->offsethv=offsethv;
    ooe->offsetvv=offsetvv;
}

static void init_obsmod_context_target( obsmod_context *oac, trgtdata *tgt )
{
    oac->tgt=tgt;
    oac->factor=1.0;
    oac->offsethv=0.0;
    oac->offsetvv=0.0;
    oac->action=0;
    oac->class_id=-1;
    oac->value_id=-1;
}

int apply_obs_modifications( void *pobsmod, survdata *sd )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    if( ! obsmod->criteria_prepared )
    {
        prepare_obs_modifications( obsmod );
    }
    int nignored=0;
    int i;
    obsmod_context oac;
    init_obsmod_context_set( &oac, obsmod, sd );

    obsmod->setid++;  /* Id used to identify when setfactor has been applied */

    switch (sd->format)
    {

    case SD_OBSDATA:
    {
        obsdata *od;
        for( i = 0, od=sd->obs.odata; i<sd->nobs; i++, od++ )
        {
            trgtdata *tgt=&(od->tgt);
            init_obsmod_context_target( &oac, tgt );
            if( obsmod && ! (tgt->unused & IGNORE_OBS_BIT) )
            {
                apply_obs_modification_action( obsmod, &oac );
                if( tgt->unused & IGNORE_OBS_BIT ) nignored++;
            }
            od->error  *= oac.factor;
            tgt->errfct  *= oac.factor;
        }
        if( oac.setfactor != 1.0 )
        {
            for( i = 0, od=sd->obs.odata; i<sd->nobs; i++, od++ )
            {
                od->error *= oac.setfactor;
                od->tgt.errfct *= oac.setfactor;
            }
        }
    }
    break;

    case SD_VECDATA:
    {
        vecdata *vd;
        obsmod->noffsets=0;
        int obstype=sd->obs.vdata[0].tgt.type;

        for( i = 0, vd=sd->obs.vdata; i<sd->nobs; i++, vd++ )
        {
            trgtdata *tgt=&(vd->tgt);
            init_obsmod_context_target( &oac, tgt );
            if( obsmod && ! (tgt->unused & IGNORE_OBS_BIT) )
            {
                apply_obs_modification_action( obsmod, &oac );
                if( tgt->unused & IGNORE_OBS_BIT ) { nignored++; }
            }
            if( sd->cvr && ! (tgt->unused & IGNORE_OBS_BIT) )
            {
                if( oac.factor != 1.0 )
                {
                    gps_covar_apply_obs_error_factor( sd, i, oac.factor );
                }
                if( oac.offsethv > 0.0 || oac.offsetvv > 0.0 ) 
                {
                    obsmod_add_offset(obsmod,i,oac.offsethv,oac.offsetvv);
                }
            }
        }
        if( sd->cvr && oac.setfactor != 1.0 )
        {
            gps_covar_apply_set_error_factor( sd, oac.setfactor );
        }
        /* Apply offsets after all scaling has been done... */
        if( obstype==GX || obstype==GB )
        {
            if( obsmod->noffsets )
            {
                obs_offset_error *offset=obsmod->offsets;
                for( i = 0; i<obsmod->noffsets; i++, offset++ )
                {
                    gps_covar_apply_obs_offset_error( sd, offset->iobs, offset->offsethv, offset->offsetvv );
                }
            }
            if( oac.centroidhv > 0.0 || oac.centroidvv > 0.0 )
            {
                if( obstype==GX )
                {
                    gps_covar_apply_centroid_error( sd, oac.centroidhv, oac.centroidvv );
                }
                else
                {
                    gps_covar_apply_basestation_offset_error( sd, oac.centroidhv, oac.centroidvv );
                }
            }
        }
    }
    break;

    case SD_PNTDATA:
    {
        pntdata *pd;

        for( i = 0, pd=sd->obs.pdata; i<sd->nobs; i++, pd++ )
        {
            trgtdata *tgt=&(pd->tgt);
            init_obsmod_context_target( &oac, tgt );
            if( obsmod && ! (tgt->unused & IGNORE_OBS_BIT) )
            {
                apply_obs_modification_action( obsmod, &oac );
                if( tgt->unused & IGNORE_OBS_BIT ) { nignored++; }
            }
            pd->error  *= oac.factor;
            tgt->errfct *= oac.factor;
        }
        if( oac.setfactor != 1.0 )
        {
            for( i = 0, pd=sd->obs.pdata; i<sd->nobs; i++, pd++ )
            {
                pd->error *= oac.setfactor;
                pd->tgt.errfct *= oac.setfactor;
            }
        }
    }
    break;
    }

    return nignored;
}

bool obsmod_ignore_datafile( void *pobsmod, int file_id )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    if( ! obsmod ) return false;
    for( obs_criteria *ocr=obsmod->first; ocr; ocr=ocr->next )
    {
        if( obs_criteria_ignore_datafile( obsmod, ocr, file_id ) ) return true;
    }
    return false;
}


int check_obsmod_station_criteria_codes( void *pobsmod, network *nw )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    int return_sts = OK;
    if( ! obsmod ) return return_sts;
    for( obs_criteria *ocr=obsmod->first; ocr; ocr=ocr->next )
    {
        for( obs_criterion *oc=ocr->first; oc; oc=oc->next )
        {
            if( oc->crit_type == OBS_CRIT_STATION_USES || 
                    oc->crit_type == OBS_CRIT_STATION_BETWEEN )
            {
                void *psc = oc->c.stations.criteria;
                if( psc )
                {
                    set_error_location( oc->c.stations.config_loc );
                    int sts=check_station_criteria_codes( psc, nw );
                    set_error_location( nullptr );
                    if( sts != OK ) return_sts=sts;
                }
            }
        }
    }
    return return_sts;
}

void summarize_obs_modifications( void *pobsmod, FILE *lst, const std::string &prefix )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    if( ! obsmod ) return;

    for( int modtype=0; modtype < 7; modtype++ )
    {
        int action= modtype==0 ? OBS_MOD_IGNORE : 
                    modtype==1 ? OBS_MOD_REJECT :
                    modtype==2 ? OBS_MOD_REWEIGHT :
                    modtype==3 ? OBS_MOD_REWEIGHT_SET :
                    modtype==4 ? OBS_MOD_OFFSET_ERROR :
                    modtype==5 ? OBS_MOD_CENTROID_ERROR :
                    OBS_MOD_ANTENNA_OFFSET;
        int ordered=modtype > 1;
        bool firsterr=true;
        double minerrfct=0.0;
        double minerrfct2=0.0;
        obs_criteria *ocr;
        int ncriteria=0;
        int maxcriteria=0;
        for( ocr=obsmod->first; ocr; ocr=ocr->next ){ maxcriteria++; }

        while( 1 )
        {
            obs_criteria* match=nullptr;
            double errfct=0.0;
            double errfct2=0.0;
            for( ocr=obsmod->first; ocr; ocr=ocr->next )
            {
                if( ocr->action & action )
                {
                    if( ordered )
                    {
                        if( (firsterr 
                             || ocr->factor < minerrfct 
                             || (ocr->factor == minerrfct && ocr->factor < minerrfct2 ) )
                            &&
                            (
                            ocr->factor > errfct ||
                            (ocr->factor == errfct && ocr->factor2 > errfct2)
                            )
                          )
                        {
                            errfct=ocr->factor;
                            errfct2=ocr->factor2;
                            match=ocr;
                        }
                    }
                    else
                    {
                        match=ocr;
                        break;
                    }
                }
            }

            if( ! match ) break;
            if( action == OBS_MOD_IGNORE )
            {
                fprintf(lst,"\n%sThe following observations are ignored:\n",prefix.c_str());
            }
            else if( action == OBS_MOD_REJECT )
            {
                fprintf(lst,"\n%sThe following observations are rejected\n",prefix.c_str());
            }
            else if( action == OBS_MOD_REWEIGHT )
            {
                fprintf(lst,"\n%sErrors of the following observations are scaled by %.3lf\n",
                        prefix.c_str(), errfct);
            }
            else if( action == OBS_MOD_REWEIGHT_SET )
            {
                fprintf(lst,"\n%sErrors of the following observations are scaled by set by %.3lf\n",
                        prefix.c_str(), errfct);
            }
            else if( action == OBS_MOD_OFFSET_ERROR )
            {
                fprintf(lst,"\n%sOffset error %0.3lf %0.3lf m applied to the following observations\n",
                        prefix.c_str(), match->factor, match->factor2 );
            }
            else if( action == OBS_MOD_CENTROID_ERROR )
            {
                fprintf(lst,"\n%sCentroid error %0.3lf %0.3lf m applied to the following observations\n",
                        prefix.c_str(), match->factor, match->factor2 );
            }
            else if( action == OBS_MOD_ANTENNA_OFFSET )
            {
                fprintf(lst,"\n%sAntenna offset %0.3lf m applied to the following GX/GB observations\n",
                        prefix.c_str(), match->factor );

            }
            while( match )
            {
                summarize_obs_criteria( lst, prefix.c_str(), match, obsmod->classes );
                ncriteria++;
                match=match->next;
                while( match )
                {
                    if( match->action & action )
                    {
                        if( ! ordered ) break; 
                        if(  match->factor == errfct && match->factor2 == errfct2 ) break;
                    }
                    match=match->next;
                }
            }
            if( ! ordered) break;
            /* Break out just in case reweighting matching doesn't work */
            if( ncriteria >= maxcriteria ) break;
            firsterr=false;
            minerrfct=errfct;
            minerrfct2=errfct2;
        }

        if( action & (OBS_MOD_REWEIGHT | OBS_MOD_REWEIGHT_SET)  && ncriteria > 1 )
        {
            fprintf(lst,"\n%sNote: error factors are multiplied for observations meeting several criteria\n",prefix.c_str());
        }
    }
}
