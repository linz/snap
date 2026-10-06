#include "snapconfig.h"
/* Routines for managing observation selection criteria */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
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

#define FILE_IGNORE_ERROR OK
#define FILE_WARN_ERROR   INFO_ERROR
#define FILE_FAIL_ERROR   INVALID_DATA

/* Criterion against which observations are tested.  obs_criterion holds one of these.
   Each is fully described by its constructor arguments; only the members
   marked as lazily filled change afterwards. */

struct obs_datatype_criterion
{
    explicit obs_datatype_criterion( const std::array<bool,NOBSTYPE> &select ) : select( select ) {}

    const std::array<bool,NOBSTYPE> select;
};

// The result of testing one data file's name against a filename pattern.
struct obs_datafile_match
{
    int file_id;
    bool matched;
};

struct obs_datafile_criterion
{
    obs_datafile_criterion( const bool wildcard, const int file_id, const std::string &filenamepattern )
        : wildcard( wildcard ), file_id( file_id ), filenamepattern( filenamepattern ) {}

    // A wildcard criterion matches the name of each file against
    // filenamepattern; otherwise file_id is the one file it matches.
    const bool wildcard;
    const int file_id;
    // The text the user gave for the data file: a pattern, or a plain file name.
    const std::string filenamepattern;
    // Lazily filled: the most recent file tested against a wildcard
    // criterion, so that consecutive observations from one file need not
    // repeat the match.
    std::optional<obs_datafile_match> last_match;
};

struct obs_classification_criterion
{
    obs_classification_criterion( const int class_id, const int value_id )
        : class_id( class_id ), value_id( value_id ) {}

    const int class_id;
    const int value_id;
};

struct obs_id_criterion
{
    explicit obs_id_criterion( const std::vector<int> &obs_ids ) : obs_ids( obs_ids ) {}

    const std::vector<int> obs_ids;
};

struct mult_obs_classification_criterion
{
    mult_obs_classification_criterion( const int class_id, const std::vector<int> &value_ids )
        : class_id( class_id ), value_ids( value_ids ) {}

    const int class_id;
    const std::vector<int> value_ids;
};

struct wildcard_obs_classification_criterion
{
    wildcard_obs_classification_criterion( const int class_id, const std::string &wildclass )
        : class_id( class_id ), wildclass( wildclass ) {}

    const int class_id;
    const std::string wildclass;
    // Lazily filled: the number of classification values already tested
    // against wildclass, and the ids of those that matched.
    int ntested = 0;
    std::vector<int> value_ids;
};

enum class obs_date_criterion_type
{
    unknown,
    before,
    after
};

struct obs_date_criterion
{
    obs_date_criterion( const obs_date_criterion_type date_criterion_type, const double date )
        : date_criterion_type( date_criterion_type ), date( date ) {}

    const obs_date_criterion_type date_criterion_type;
    const double date;
};

struct obs_stations_criterion
{
    obs_stations_criterion( const bool between, const std::string &station_list,
                            const std::string &config_filename, const std::string &config_loc )
        : between( between ), station_list( station_list ),
          config_filename( config_filename ), config_loc( config_loc ) {}

    // True if both ends of an observation must match, false if either may.
    const bool between;
    const std::string station_list;
    const std::string config_filename;
    const std::string config_loc;
    // Lazily filled: compiled from station_list on the first match. Owned
    // by the criterion and freed by delete_obs_criterion.
    void *criteria = nullptr;
};

using obs_criterion_type = std::variant<obs_datatype_criterion, obs_datafile_criterion,
                                        obs_classification_criterion, mult_obs_classification_criterion,
                                        wildcard_obs_classification_criterion, obs_id_criterion,
                                        obs_date_criterion, obs_stations_criterion>;

/* Single observation criterion. */

struct obs_criterion
{
    explicit obs_criterion( const obs_criterion_type &type ) : type( type ) {}

    obs_criterion_type type;
    // Set by the grouping code.
    bool groupmatch = false;
};

// One observation selection line: an action to apply to the observations
// that meet all of its criteria.
struct obs_criteria
{
    obs_criteria( const int action, const double factor, const double factor2, const int option )
        : action( action ), factor( factor ), factor2( factor2 ), option( option )
    {
        if( this->action & OBS_MOD_REWEIGHT_SET ) this->action &= ~ static_cast<int>( OBS_MOD_REWEIGHT );
    }

    std::vector<obs_criterion> criteria;
    int action;                ///< OBS_MOD_* bits, with the OBS_MOD_REWEIGHT bit cleared if OBS_MOD_REWEIGHT_SET is set
    const double factor;
    const double factor2;
    const int option;
    long setid = 0;            ///< The set of observations this was last applied to
};

// The criteria that obs_modifications applies to an observation with a given
// value of a classification, in the order they are applied. The criteria are
// owned by obs_modifications::criteria.
using obs_criteria_bucket = std::vector<obs_criteria *>;

#define DFLT_MAX_OFFSETS 256

struct obs_offset_error
{
    int iobs;
    double offsethv;
    double offsetvv;
};

struct obsmod_context;

/// The `obs_criteria` that modify observations, and the means of applying them.
///
/// To avoid having to check every observation against every `obs_criteria`,
/// the `obs_criteria` are reduced to buckets.
///
/// An `obs_criteria` holds a list of tests, all of which an observation must
/// meet. One of them can be its key test: a test that the observation's value
/// of a classification is a given value, or a test for a single data file.
/// The `obs_criteria` is then put in the bucket for that classification and
/// value, and the key test is taken as met (`obs_criterion::groupmatch`).
/// Data files are classification 0, with the file id as the value. If there are
/// several candidates the key test is the first one whose classification is
/// used by the most `obs_criteria`. An `obs_criteria` with no key test, or
/// one that sets options or antenna offsets, is not put in a bucket and is
/// held in `_ungrouped_criteria`.
///
/// The buckets are held in `_grouped_criteria`, by classification and then
/// value. `apply_criteria` visits the classifications in
/// `_grouped_criteria_order`. For each it looks up the bucket for the
/// observation's value of that classification, if there is one, and applies
/// the `obs_criteria` in it in order. It finishes by applying
/// `_ungrouped_criteria`. An `obs_criteria` is in only one bucket or in
/// `_ungrouped_criteria`, so it is applied at most once to an observation.
///
/// The buckets are built by `apply_criteria` when the first observation is
/// applied after an `obs_criteria` has been added with `add_criteria`.
struct obs_modifications
{
    obs_modifications( network *nw, classifications *classes ) : nw( nw ), classes( classes ) {}

    /// Adds an `obs_criteria`, to be applied after those already added
    void add_criteria( obs_criteria &&added );
    /// Applies the `obs_criteria` to one observation, using the buckets
    void apply_criteria( obsmod_context &oac );

    std::deque<obs_criteria> criteria;   ///< Every `obs_criteria`, in the order added. The buckets point to these
    network *nw;
    classifications *classes;
    fileid_func get_fileid = nullptr;
    filename_func get_filename = nullptr;
    long setid = 0;
    obs_offset_error *offsets = nullptr;
    int noffsets = 0;
    int maxoffsets = 0;

private:
    void _prepare_criteria();   ///< Builds the buckets from `criteria`

    std::map<int,std::map<int,obs_criteria_bucket>> _grouped_criteria;   ///< The buckets, by classification then value
    std::vector<int> _grouped_criteria_order;                            ///< One entry for each key of `_grouped_criteria`, in the order they are to be applied: descending count of non-empty buckets, then descending classification
    obs_criteria_bucket _ungrouped_criteria;                             ///< The `obs_criteria` that are not in a bucket, applied to every observation after those in the buckets
    bool _criteria_prepared = false;                                     ///< True if the buckets are up to date with `criteria`
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

/*===============================================================================*/

/// Adds a criterion for observations of the data types in a '/' separated
/// list of data type codes. Returns false if a code is invalid, after
/// reporting every invalid code with send_config_error.
static bool add_obs_datatype_criterion( CFG_FILE *cfg, const std::string &datatypes, std::vector<obs_criterion> &criteria )
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

    if( sts != OK ) return false;

    criteria.emplace_back( obs_datatype_criterion( select ) );
    return true;
}

static bool obs_datatype_match( const obs_datatype_criterion &datatype, obsmod_context *oac )
{
    return datatype.select[oac->tgt->type];
}

static void describe_obs_datatype_criterion( FILE *lst, const obs_datatype_criterion &datatype, const std::string &prefix )
{
    int ntype=0;
    int itype=0;

    for( int i=0; i<NOBSTYPE; i++ ){ if( datatype.select[i]){ ntype++; itype=i;}}

    if( ntype == 1 )
    {
        datatypedef *dtype=datatypedef_from_id(itype);
        fprintf(lst,"which are of type %s (%s)",dtype->code.data(),dtype->name.data());
    }
    else
    {
        fprintf(lst,"which are of types:");
        for( int i=0; i<NOBSTYPE; i++ )
        {
            if( datatype.select[i])
            {
                datatypedef *dtype=datatypedef_from_id(i);
                fprintf(lst,"\n%s        - %s (%s)",prefix.c_str(), dtype->code.data(), dtype->name.data());
            }
        }
    }
}

/// Adds a criterion for observations from a data file. A wildcard criterion
/// matches the names of files against filenamepattern, otherwise file_id is
/// the one file it matches (and file_id is ignored for a wildcard criterion).
static void add_obs_datafile_criterion( const bool wildcard, const int file_id, const std::string &filenamepattern, std::vector<obs_criterion> &criteria )
{
    criteria.emplace_back( obs_datafile_criterion( wildcard, file_id, filenamepattern ) );
}

static bool obs_datafile_match_fileid( obs_modifications *obsmod, obs_datafile_criterion &datafile, const int file_id )
{
    if( ! datafile.wildcard )
    {
        return file_id == datafile.file_id;
    }
    if( datafile.last_match && datafile.last_match->file_id == file_id ) return datafile.last_match->matched;
    const std::string filename = obsmod->get_filename( file_id );
    const bool matched = filename_wildcard_match( datafile.filenamepattern, filename );
    datafile.last_match = obs_datafile_match{ file_id, matched };
    return matched;
}

static bool obs_datafile_match( obs_datafile_criterion &datafile, obsmod_context *oac )
{
    return obs_datafile_match_fileid( oac->obsmod, datafile, oac->sd->file );
}

static void describe_obs_datafile_criterion( FILE *lst, const obs_datafile_criterion &datafile, const std::string & )
{
    if( datafile.wildcard )
    {
        fprintf(lst,"which are from files matching %s",datafile.filenamepattern.c_str());
    }
    else
    {
        fprintf(lst,"which are from file %s",datafile.filenamepattern.c_str());
    }
}

/// Adds a criterion for observations whose value of a classification is a
/// single value, one of a '/' separated list of values, or (unless
/// singlevalue) matches a wildcard pattern. Returns false if there are no
/// classifications.
static bool add_obs_classification_criterion( classifications *classes,
        const std::string &classification, const std::string &values, const bool singlevalue,
        std::vector<obs_criterion> &criteria )
{
    if( ! classes ) return false;
    const int class_id=classes->id( classification, 1 );
    /* If values string contains / then this is a list of multiple classes */
    if( ! singlevalue && values.find('/') != std::string::npos )
    {
        // A known, fixed field count (number of '/' plus one), so each of
        // the first nval-1 fields comes from next('/') (guaranteed to
        // succeed, since that many delimiters are known to exist) and the
        // last from remainder() - unlike add_obs_datatype_criterion's
        // unbounded split above, a trailing '/' here does produce an empty
        // final field (verified by hand-tracing "A/" against this same
        // precomputed-count algorithm the original char*-based loop used).
        const int nval=std::count( values.begin(), values.end(), '/' ) + 1;
        std::vector<int> value_ids( nval );

        FieldScanner scanner(values);
        for( int i=0; i<nval; i++ )
        {
            const std::string value( i+1<nval ? *scanner.next('/') : scanner.remainder() );
            value_ids[i]=classes->value_id( class_id, value, 1 );
        }
        criteria.emplace_back( mult_obs_classification_criterion( class_id, value_ids ) );
    }
    else if ( ! singlevalue && has_wildcard(values) )
    {
        criteria.emplace_back( wildcard_obs_classification_criterion( class_id, values ) );
    }
    /* Otherwise a single class */
    else
    {
        criteria.emplace_back( obs_classification_criterion( class_id, classes->value_id( class_id, values, 1 ) ) );
    }
    return true;
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

static bool obs_classification_match( const obs_classification_criterion &classification, obsmod_context *oac )
{
    const int value_id = get_context_obs_classification( oac, classification.class_id );
    return classification.value_id == value_id;
}

static bool obs_mult_classification_match( const mult_obs_classification_criterion &mult, obsmod_context *oac )
{
    const int value_id=get_context_obs_classification( oac, mult.class_id );
    return std::find( mult.value_ids.begin(), mult.value_ids.end(), value_id ) != mult.value_ids.end();
}

static bool obs_wildcard_classification_match( wildcard_obs_classification_criterion &wildcard, obsmod_context *oac )
{
    classifications *csf = oac->obsmod->classes;
    if( ! csf ) return false;
    const int class_count=csf->value_count(wildcard.class_id);

    // Test the values added to the classification since the last time
    for( int iv = wildcard.ntested; iv < class_count; iv++ )
    {
        if( wildcard_match(wildcard.wildclass,csf->value_name(wildcard.class_id,iv)) )
        {
            wildcard.value_ids.push_back( iv );
        }
    }
    if( class_count > wildcard.ntested ) wildcard.ntested = class_count;

    const int value_id=get_context_obs_classification( oac, wildcard.class_id );
    return std::find( wildcard.value_ids.begin(), wildcard.value_ids.end(), value_id ) != wildcard.value_ids.end();
}

static void describe_obs_classification_criterion( FILE *lst, const obs_classification_criterion &classification, const std::string &, classifications *classes )
{
    fprintf(lst,"where %s classification is \"%s\"",
            classes->name( classification.class_id).c_str(),
            classes->value_name( classification.class_id, classification.value_id).c_str());
}

static void describe_obs_mult_classification_criterion( FILE *lst, const mult_obs_classification_criterion &mult, const std::string &prefix, classifications *classes )
{
    fprintf(lst,"where %s classification is one of:",
            classes->name( mult.class_id ).c_str());
    for( const int value_id : mult.value_ids )
    {
        fprintf(lst,"\n%s    - \"%s\"",prefix.c_str(),
            classes->value_name( mult.class_id, value_id).c_str());
    }
}

static void describe_obs_wildcard_classification_criterion( FILE *lst, const wildcard_obs_classification_criterion &wildcard, const std::string &, classifications *classes )
{
    fprintf(lst,"where %s classification matches \"%s\"",
            classes->name( wildcard.class_id).c_str(),
            wildcard.wildclass.c_str() );
}


/// Adds a criterion for observations with one of the ids in a '/' separated
/// list. Returns false if an id is invalid, after reporting it with
/// send_config_error.
static bool add_obs_id_criterion( CFG_FILE *cfg, const std::string &idstr, std::vector<obs_criterion> &criteria )
{
    // Same fixed-field-count split as add_obs_classification_criterion's
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
            return false;
        }
    }

    criteria.emplace_back( obs_id_criterion( parsed ) );
    return true;
}

static bool obs_id_match( const obs_id_criterion &id, obsmod_context *oac )
{
    return std::find( id.obs_ids.begin(), id.obs_ids.end(), oac->tgt->id ) != id.obs_ids.end();
}

static void describe_obs_id_criterion( FILE *lst, const obs_id_criterion &id, const std::string &prefix )
{
    if( id.obs_ids.size() == 1 )
    {
        fprintf(lst,"where the observation id is %d",id.obs_ids[0]);
    }
    else
    {
        fprintf(lst,"where the observation id is one of:");
        for( const int obs_id : id.obs_ids )
        {
            fprintf(lst,"\n%s    - %d", prefix.c_str(), obs_id);
        }
    }
}

/// Adds a criterion for observations before or after a date, or with no date
/// (in which case datestr is ignored). Returns false if the date is invalid,
/// after reporting it with send_config_error.
static bool add_obs_date_criterion( CFG_FILE *cfg, const obs_date_criterion_type date_crit_type, const std::string &datestr, std::vector<obs_criterion> &criteria )
{
    double date=UNDEFINED_DATE;

    if( date_crit_type != obs_date_criterion_type::unknown )
    {
        date=snap_datetime_parse(datestr);
        if( date == UNDEFINED_DATE )
        {
            char errmsg[100];
            sprintf( errmsg,"Invalid date \"%.50s\" in observation date criteria",datestr.c_str());
            send_config_error( cfg, INVALID_DATA, errmsg );
            return false;
        }
    }
    criteria.emplace_back( obs_date_criterion( date_crit_type, date ) );
    return true;
}

static bool obs_date_match( const obs_date_criterion &date_criterion, obsmod_context *oac )
{
    const double date=oac->sd->date;

    switch( date_criterion.date_criterion_type )
    {
        case obs_date_criterion_type::before:
            return date != UNDEFINED_DATE && date < date_criterion.date;
        case obs_date_criterion_type::after:
            return date != UNDEFINED_DATE && date > date_criterion.date;
        case obs_date_criterion_type::unknown:
            break;
    }
    return date == UNDEFINED_DATE;
}

static void describe_obs_date_criterion( FILE *lst, const obs_date_criterion &date_criterion, const std::string & )
{
    if( date_criterion.date_criterion_type == obs_date_criterion_type::unknown )
    {
        fprintf(lst,"which have no observation date");
    }
    else
    {
        fprintf(lst,"which are observed %s %s",
            date_criterion.date_criterion_type == obs_date_criterion_type::before ?
            "before" : "after",
            date_as_string(date_criterion.date,DateStringFormat::timeIfNotMidnight).c_str() );
    }
}

static void init_obs_stations_criterion( obs_stations_criterion &stations, network *nw  )
{
    void *psc=new_station_criteria();
    set_error_location( stations.config_loc );
    int sts=compile_station_criteria( psc, nw,
            stations.station_list,
            stations.config_filename );
    set_error_location( NO_MESSAGE );
    if( sts != OK )
    {
        delete_station_criteria( psc );
        psc=new_station_criteria();
    }
    setup_station_criteria_cache( psc, number_of_stations( nw ) );
    stations.criteria = psc;
}

/// Adds a criterion for observations that use, or are between, stations in a
/// station list. The station criteria are compiled from the list when first
/// needed.
static void add_obs_stations_criterion( CFG_FILE *cfg, const bool between, const std::string &station_list, std::vector<obs_criterion> &criteria )
{
    criteria.emplace_back( obs_stations_criterion( between, station_list,
            get_config_filename(cfg), get_config_location(cfg) ) );
}

static bool obs_stations_match( obs_stations_criterion &stations, obsmod_context *oac )
{
    network *nw=oac->obsmod->nw;
    if( ! stations.criteria )
    {
        init_obs_stations_criterion( stations, nw  );
    }

    int fromstn=oac->sd->from;
    int tostn = oac->tgt->to;

    bool matchfrom=true;
    bool matchuse=false;
    if( fromstn > 0 )
    {
        matchfrom=station_criteria_match( stations.criteria, station_ptr( nw, fromstn ));
        matchuse=matchfrom;
    }
    bool matchto=true;
    if( tostn > 0 )
    {
        matchto=station_criteria_match( stations.criteria, station_ptr( nw, tostn ));
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
    if( ! stations.between )
    {
        return matchuse;
    }
    return matchfrom && matchto;
}

static void describe_obs_stations_criterion( FILE *lst, const obs_stations_criterion &stations, const std::string & )
{
    fprintf(lst,"which %s stations %s",
            stations.between ? "are between" : "use",
            stations.station_list.c_str() );
}

static bool obs_criterion_match( obs_criterion &oc, obsmod_context *oac )
{
    if( oc.groupmatch ) return true;
    if( auto *datatype=std::get_if<obs_datatype_criterion>( &oc.type ) ) return obs_datatype_match( *datatype, oac );
    if( auto *datafile=std::get_if<obs_datafile_criterion>( &oc.type ) ) return obs_datafile_match( *datafile, oac );
    if( auto *classification=std::get_if<obs_classification_criterion>( &oc.type ) ) return obs_classification_match( *classification, oac );
    if( auto *mult=std::get_if<mult_obs_classification_criterion>( &oc.type ) ) return obs_mult_classification_match( *mult, oac );
    if( auto *wildcard=std::get_if<wildcard_obs_classification_criterion>( &oc.type ) ) return obs_wildcard_classification_match( *wildcard, oac );
    if( auto *id=std::get_if<obs_id_criterion>( &oc.type ) ) return obs_id_match( *id, oac );
    if( auto *date=std::get_if<obs_date_criterion>( &oc.type ) ) return obs_date_match( *date, oac );
    if( auto *stations=std::get_if<obs_stations_criterion>( &oc.type ) ) return obs_stations_match( *stations, oac );
    return false;
}

// The stations criterion owns its compiled station criteria. Nothing else in
// a criterion needs freeing.
static void delete_obs_criterion( const obs_criterion &oc )
{
    const obs_stations_criterion *stations=std::get_if<obs_stations_criterion>( &oc.type );
    if( stations && stations->criteria ) delete_station_criteria( stations->criteria );
}

static void delete_obs_criteria( const obs_criteria &ocr )
{
    for( const obs_criterion &oc : ocr.criteria ) delete_obs_criterion( oc );
}

/* Cumulate action and reweight factor for list of criteria */

static void apply_obs_criteria_action( obs_criteria &ocr, obsmod_context *oac )
{
    int action=oac->action;
    if( ! (action & OBS_MOD_IGNORE ) )
    {
        oac->matchfrom=true;
        oac->matchto=true;
        for( obs_criterion &oc : ocr.criteria )
        {
            if( ! obs_criterion_match( oc, oac ) )
            {
                return;
            }
        }

        if( ocr.action & OBS_MOD_IGNORE )
        {
            action = OBS_MOD_IGNORE;
        }
        else if ( ocr.action & OBS_MOD_ANTENNA_OFFSET )
        {
            if( (oac->tgt->type == GB || oac->tgt->type == GX) && oac->matchto )
            {
                oac->tgt->tohgt += ocr.factor;
            }
            if( oac->tgt->type==GB && oac->matchfrom &&  ocr.setid != oac->obsmod->setid )
            {
                ocr.setid=oac->obsmod->setid;
                oac->sd->fromhgt += ocr.factor;
            }
        }
        else
        {
            action |= ocr.action;
            if( ocr.action & OBS_MOD_REWEIGHT ) oac->factor *= ocr.factor;
            else if( ocr.action & OBS_MOD_OFFSET_ERROR )
            {
                if( oac->matchto )
                {
                    oac->offsethv += ocr.factor*ocr.factor;
                    oac->offsetvv += ocr.factor2*ocr.factor2;
                }
                if( oac->tgt->type==GB && oac->matchfrom &&  ocr.setid != oac->obsmod->setid )
                {
                    ocr.setid=oac->obsmod->setid;
                    oac->centroidhv += (ocr.factor*ocr.factor);
                    oac->centroidvv += (ocr.factor2*ocr.factor2);
                }
            }
            else if( ocr.action & OBS_MOD_REWEIGHT_SET )
            {
                if( ocr.setid != oac->obsmod->setid )
                {
                    ocr.setid=oac->obsmod->setid;
                    oac->setfactor *= ocr.factor;
                }
            }
            else if( ocr.action & OBS_MOD_CENTROID_ERROR )
            {
                if( ocr.setid != oac->obsmod->setid )
                {
                    ocr.setid=oac->obsmod->setid;
                    oac->centroidhv += (ocr.factor*ocr.factor);
                    oac->centroidvv += (ocr.factor2*ocr.factor2);
                }
            }
        }
        if( ocr.action & OBS_MOD_SET_OPTION )
        {
            oac->sd->options |= ocr.option;
        }
        else if( ocr.action & OBS_MOD_UNSET_OPTION )
        {
            oac->sd->options &= ~(ocr.option);
        }
    }
    oac->action=action;
}

static bool obs_criteria_ignore_datafile( obs_modifications *obsmod, obs_criteria &ocr, const int file_id )
{
    if( ! (ocr.action & OBS_MOD_IGNORE ) ) return false;
    bool matched=false;
    for( obs_criterion &oc : ocr.criteria )
    {
        obs_datafile_criterion *datafile=std::get_if<obs_datafile_criterion>( &oc.type );
        if( ! datafile ) return false;
        if( obs_datafile_match_fileid( obsmod, *datafile, file_id ))
        {
            matched=true;
        }
    }
    return matched;
}

static void describe_obs_criterion( FILE *lst, const obs_criterion &oc, const std::string &prefix, classifications *classes )
{
    if( const auto *datatype=std::get_if<obs_datatype_criterion>( &oc.type ) ) describe_obs_datatype_criterion( lst, *datatype, prefix );
    else if( const auto *datafile=std::get_if<obs_datafile_criterion>( &oc.type ) ) describe_obs_datafile_criterion( lst, *datafile, prefix );
    else if( const auto *classification=std::get_if<obs_classification_criterion>( &oc.type ) ) describe_obs_classification_criterion( lst, *classification, prefix, classes );
    else if( const auto *mult=std::get_if<mult_obs_classification_criterion>( &oc.type ) ) describe_obs_mult_classification_criterion( lst, *mult, prefix, classes );
    else if( const auto *wildcard=std::get_if<wildcard_obs_classification_criterion>( &oc.type ) ) describe_obs_wildcard_classification_criterion( lst, *wildcard, prefix, classes );
    else if( const auto *id=std::get_if<obs_id_criterion>( &oc.type ) ) describe_obs_id_criterion( lst, *id, prefix );
    else if( const auto *date=std::get_if<obs_date_criterion>( &oc.type ) ) describe_obs_date_criterion( lst, *date, prefix );
    else if( const auto *stations=std::get_if<obs_stations_criterion>( &oc.type ) ) describe_obs_stations_criterion( lst, *stations, prefix );
}

static void summarize_obs_criteria( FILE *lst, const std::string &prefix, const obs_criteria &ocr, classifications *classes )
{
    if( ocr.criteria.empty() )
    {
        fprintf(lst,"%s  All observations\n", prefix.c_str());
        return;
    }

    const bool just1=ocr.criteria.size() == 1;

    if( ocr.action == OBS_MOD_REWEIGHT_SET )
    {
        fprintf(lst,"%s  Observations in sets including one or more observations%s", prefix.c_str(), just1 ? " " : ":");
    }
    else
    {
        fprintf(lst,"%s  Observations%s", prefix.c_str(), just1 ? " " : ":");
    }
    int ncrit=0;
    for( const obs_criterion &oc : ocr.criteria )
    {
        ncrit++;
        if( ncrit > 1 ) fprintf(lst,", and");
        if( ! just1 ) fprintf(lst,"\n%s    - ",prefix.c_str());
        describe_obs_criterion( lst, oc, prefix, classes );
    }
    fprintf(lst,"\n");
}

void *new_obs_modifications( network *nw, classifications *obs_classes )
{
    return new obs_modifications( nw, obs_classes );
}

void delete_obs_modifications( void *pobsmod )
{
    obs_modifications *obsmod=(obs_modifications *)pobsmod;
    for( const obs_criteria &ocr : obsmod->criteria ) delete_obs_criteria( ocr );
    if( obsmod->offsets )
    {
        check_free(obsmod->offsets);
    }
    delete obsmod;
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

void obs_modifications::add_criteria( obs_criteria &&added )
{
    criteria.push_back( std::move( added ) );
    _criteria_prepared=false;
}


static int get_file_id( obs_modifications *obsmod, CFG_FILE *cfg, std::string_view datafile, int missing_error )
{
    if( ! obsmod->get_fileid )
    {
        handle_error( INTERNAL_ERROR,
            "Program error: Survey file id function not initialised in observation modifications",
            NO_MESSAGE );
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
/// classification criterion instead. Adds the criterion to criteria, and
/// returns false if it is invalid or the value is missing/malformed; the
/// error is already reported via send_config_error before returning.
static bool parse_key_value_criterion(
    CFG_FILE *cfg,                 ///< current config file, for error reporting
    obs_modifications *obsmod,     ///< owns the classification map and data-file lookup used by some keys
    FieldScanner &scanner,         ///< field cursor; advanced past a multi-field quoted value
    std::string_view field,        ///< the whole "key=value" field, for error messages
    std::string_view key,          ///< the part of field before '='
    std::string_view valuePart,    ///< the part of field after '=', not yet unquoted
    int missing_error,             ///< error severity to use if a data_file value doesn't resolve
    std::vector<obs_criterion> &criteria )  ///< the criterion is added to these
{
    if( key.empty() || valuePart.empty() )
    {
        char errmess[100];
        std::string fieldText(field);
        sprintf(errmess,"Invalid observation selection criteria \"%.40s\"",fieldText.c_str());
        send_config_error(cfg,INVALID_DATA,errmess);
        return false;
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
            return false;
        }
        value.assign(*quotedResult);
    }
    else
    {
        value.assign(valuePart);
    }

    if( boost::algorithm::iequals(key,"data_type") )
    {
        return add_obs_datatype_criterion(cfg,value,criteria);
    }
    if( boost::algorithm::iequals(key,"data_file") )
    {
        const bool wildcard = ! quoted && has_wildcard(value);
        int file_id=0;
        if( ! wildcard )
        {
            file_id=get_file_id( obsmod, cfg, value, missing_error );
            if( file_id < 0 ) return false;
        }
        add_obs_datafile_criterion(wildcard,file_id,value,criteria);
        return true;
    }
    if( boost::algorithm::iequals(key,"id") )
    {
        return add_obs_id_criterion(cfg,value,criteria);
    }
    const std::string keyText(key);
    return add_obs_classification_criterion(obsmod->classes, keyText, value, quoted, criteria );
}

/// Parses a using_stations/between_stations ... end_stations span, capturing
/// the station list verbatim (preserving the original spacing between
/// names) between the two keywords. scanner is positioned right after the
/// opening keyword; it is advanced to just past end_stations (or to end of
/// text if end_stations is never found). Adds the criterion to criteria, and
/// returns false if the station list itself is missing; the error is already
/// reported via send_config_error before returning.
static bool parse_stations_criterion(
    CFG_FILE *cfg,              ///< current config file, for error reporting
    FieldScanner &scanner,      ///< field cursor, positioned after using_stations/between_stations
    const bool between,         ///< true for between_stations, false for using_stations
    std::vector<obs_criterion> &criteria )  ///< the criterion is added to these
{
    auto stationField = scanner.next();
    if( ! stationField )
    {
        send_config_error(cfg,INVALID_DATA,"Missing station list in observation selection criteria");
        return false;
    }
    auto start = stationField->data();
    while( stationField && ! boost::algorithm::iequals(*stationField,"end_stations") )
    {
        stationField = scanner.next();
    }
    auto stop = stationField ? stationField->data() : scanner.remainder().data();
    std::string stationList( start, stop - start );
    add_obs_stations_criterion( cfg, between, stationList, criteria );
    return true;
}

static int add_obs_modifications_imp( CFG_FILE *cfg, void *pobsmod, std::string_view criteria, int action, int option, double errval1, double errval2 )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obs_criteria ocr( action, errval1, errval2, option );
    FieldScanner scanner( criteria );
    int sts=OK;
    int missing_error=INVALID_DATA;
    bool have_criterion=false;
    std::optional<std::string_view> field;
    while( (field=scanner.next()) )
    {
        bool added=false;

        if( boost::algorithm::iequals(*field,"ignore_missing") ){ missing_error=OK; continue; }
        if( boost::algorithm::iequals(*field,"warn_missing") ){ missing_error=INFO_ERROR; continue; }
        if( boost::algorithm::iequals(*field,"fail_missing") ){ missing_error=INVALID_DATA; continue; }
        if( boost::algorithm::iequals(*field,"all_observations") ){ have_criterion=true; continue; }

        auto eqPos=field->find('=');
        if( eqPos != std::string_view::npos )
        {
            auto key=field->substr(0,eqPos);
            auto valuePart=field->substr(eqPos+1);
            added=parse_key_value_criterion(cfg,obsmod,scanner,*field,key,valuePart,missing_error,ocr.criteria);
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
                added=add_obs_date_criterion( cfg, obs_date_criterion_type::before, dateText, ocr.criteria );
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
                added=add_obs_date_criterion( cfg, obs_date_criterion_type::after, dateText, ocr.criteria );
            }
        }
        else if( boost::algorithm::iequals(*field,"date_unknown") )
        {
            added=add_obs_date_criterion( cfg, obs_date_criterion_type::unknown, std::string(), ocr.criteria );
        }
        else if( boost::algorithm::iequals(*field,"using_stations") ||
                boost::algorithm::iequals(*field,"between_stations") )
        {
            const bool between=boost::algorithm::iequals(*field,"between_stations");
            added=parse_stations_criterion(cfg,scanner,between,ocr.criteria);
        }
        else
        {
            char errmess[120];
            std::string fieldText(*field);
            sprintf(errmess,"Invalid specification %.50s in observation selection criteria",fieldText.c_str());
            send_config_error(cfg,INVALID_DATA,errmess);
            sts=INVALID_DATA;
        }
        if( added )
        {
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
        obsmod->add_criteria( std::move( ocr ) );
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
    obs_criteria ocr( action, err_factor, 0.0, 0 );
    bool added=false;

    if( boost::algorithm::iequals(classification,"data_type") )
    {
        added=add_obs_datatype_criterion(cfg,std::string(value),ocr.criteria);
    }
    else if( boost::algorithm::iequals(classification,"data_file") )
    {
        int file_id=get_file_id( obsmod, cfg, value, missing_error );
        if( file_id >= 0 )
        {
            add_obs_datafile_criterion( false, file_id, std::string(value), ocr.criteria );
            added=true;
        }
    }
    else if( boost::algorithm::iequals(classification,"id") )
    {
        added=add_obs_id_criterion(cfg,std::string(value),ocr.criteria);
    }
    else
    {
        added=add_obs_classification_criterion(obsmod->classes, std::string(classification), std::string(value), true, ocr.criteria );
    }
    if( ! added )
    {
        return INVALID_DATA;
    }
    obsmod->add_criteria( std::move( ocr ) );
    return OK;
}

int add_obs_modifications_datafile_factor( CFG_FILE *, void *pobsmod, int fileid, const std::string &filename, double err_factor )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obs_criteria ocr( OBS_MOD_REWEIGHT, err_factor, 0.0, 0 );
    add_obs_datafile_criterion( false, fileid, filename, ocr.criteria );
    obsmod->add_criteria( std::move( ocr ) );
    return OK;
}

// Applies the `obs_criteria` in a bucket to an observation, in order. Stops
// if the observation is ignored.
static void apply_obs_criteria_bucket( const obs_criteria_bucket &bucket, obsmod_context *oac )
{
    if( bucket.empty() ) return;
    for( obs_criteria *ocr : bucket )
    {
        apply_obs_criteria_action( *ocr, oac );
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

void obs_modifications::apply_criteria( obsmod_context &oac )
{
    if( oac.tgt->unused & IGNORE_OBS_BIT ) return;
    if( ! _criteria_prepared ) _prepare_criteria();

    for( const int classification : _grouped_criteria_order )
    {
        const std::map<int,obs_criteria_bucket> &buckets=_grouped_criteria.at( classification );
        const int value = classification > 0 ? get_context_obs_classification( &oac, classification ) : oac.sd->file;
        const auto bucket=buckets.find( value );
        if( bucket == buckets.end() ) continue;
        apply_obs_criteria_bucket( bucket->second, &oac );
        /* Return if observation is being ignored */
        if( oac.action & OBS_MOD_IGNORE ) return;
    }
    apply_obs_criteria_bucket( _ungrouped_criteria, &oac );
}

/// The classification and value of a test that could be the key test of an
/// `obs_criteria`, or `std::nullopt` if the test can't be a key test. Data
/// files are classification 0, with the file id as the value.
static std::optional<std::pair<int,int>> obs_criterion_key( const obs_criterion &oc )
{
    if( const auto *classification=std::get_if<obs_classification_criterion>( &oc.type ) )
    {
        return std::make_pair( classification->class_id, classification->value_id );
    }
    const auto *datafile=std::get_if<obs_datafile_criterion>( &oc.type );
    if( datafile && ! datafile->wildcard )
    {
        return std::make_pair( 0, datafile->file_id );
    }
    return std::nullopt;
}

void obs_modifications::_prepare_criteria()
{
    _grouped_criteria.clear();
    _grouped_criteria_order.clear();
    _ungrouped_criteria.clear();
    _criteria_prepared=true;

    // Count the tests that could be key tests for each classification, to
    // prefer the most used classification
    std::map<int,int> class_count;
    for( const obs_criteria &ocr : criteria )
    {
        for( const obs_criterion &oc : ocr.criteria )
        {
            const auto key=obs_criterion_key( oc );
            if( key ) class_count[key->first]++;
        }
    }

    for( obs_criteria &ocr : criteria )
    {
        // Option criteria are not put in buckets as they need to be applied in order
        if( ocr.action & (OBS_MOD_SET_OPTION | OBS_MOD_UNSET_OPTION | OBS_MOD_ANTENNA_OFFSET ))
        {
            _ungrouped_criteria.push_back( &ocr );
            continue;
        }

        // The key test is the first test using the most used classification
        obs_criterion *keytest=nullptr;
        std::pair<int,int> keyvalue;
        int maxcount=0;
        for( obs_criterion &oc : ocr.criteria )
        {
            oc.groupmatch=false;
            const auto key=obs_criterion_key( oc );
            if( ! key ) continue;
            if( class_count[key->first] > maxcount )
            {
                maxcount=class_count[key->first];
                keyvalue=*key;
                keytest=&oc;
            }
        }

        if( ! keytest )
        {
            _ungrouped_criteria.push_back( &ocr );
            continue;
        }
        keytest->groupmatch=true;
        // The newest is applied first
        obs_criteria_bucket &bucket=_grouped_criteria[keyvalue.first][keyvalue.second];
        bucket.insert( bucket.begin(), &ocr );
    }

    // Apply the classifications with the most buckets first, and the highest
    // classification first if they have the same number
    for( const auto &buckets : _grouped_criteria ) _grouped_criteria_order.push_back( buckets.first );
    std::sort( _grouped_criteria_order.begin(), _grouped_criteria_order.end(),
        [this]( const int classification1, const int classification2 )
        {
            const size_t nbuckets1=_grouped_criteria.at( classification1 ).size();
            const size_t nbuckets2=_grouped_criteria.at( classification2 ).size();
            if( nbuckets1 != nbuckets2 ) return nbuckets1 > nbuckets2;
            return classification1 > classification2;
        } );
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
                obsmod->apply_criteria( oac );
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
                obsmod->apply_criteria( oac );
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
                obsmod->apply_criteria( oac );
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
    for( obs_criteria &ocr : obsmod->criteria )
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
    for( const obs_criteria &ocr : obsmod->criteria )
    {
        for( const obs_criterion &oc : ocr.criteria )
        {
            const obs_stations_criterion *stations=std::get_if<obs_stations_criterion>( &oc.type );
            if( stations && stations->criteria )
            {
                set_error_location( stations->config_loc );
                int sts=check_station_criteria_codes( stations->criteria, nw );
                set_error_location( NO_MESSAGE );
                if( sts != OK ) return_sts=sts;
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
        const auto criteria_end=obsmod->criteria.cend();
        size_t ncriteria=0;
        const size_t maxcriteria=obsmod->criteria.size();

        while( 1 )
        {
            auto match=criteria_end;
            double errfct=0.0;
            double errfct2=0.0;
            for( auto ocr=obsmod->criteria.cbegin(); ocr != criteria_end; ++ocr )
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

            if( match == criteria_end ) break;
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
            while( match != criteria_end )
            {
                summarize_obs_criteria( lst, prefix, *match, obsmod->classes );
                ncriteria++;
                ++match;
                while( match != criteria_end )
                {
                    if( match->action & action )
                    {
                        if( ! ordered ) break;
                        if(  match->factor == errfct && match->factor2 == errfct2 ) break;
                    }
                    ++match;
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
