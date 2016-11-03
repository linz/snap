#include "snapconfig.h"
/* Routines for managing observation selection criteria */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "snapdata/obsmod.h"
#include "snapdata/datatype.h"
#include "snapdata/survdata.h"
#include "network/network.h"
#include "util/chkalloc.h"
#include "util/errdef.h"
#include "util/dateutil.h"
#include "util/dstring.h"

#define OBS_CRIT_NONE            0
#define OBS_CRIT_DATATYPE        1
#define OBS_CRIT_DATAFILE        2
#define OBS_CRIT_CLASSIFICATION  3
#define OBS_CRIT_MCLASSIFICATION 4
#define OBS_CRIT_ID              5
#define OBS_CRIT_DATE            6
#define OBS_CRIT_STATION_USES    7
#define OBS_CRIT_STATION_BETWEEN 8

#define OBS_CRIT_DATE_UNKNOWN 1
#define OBS_CRIT_DATE_BEFORE  2
#define OBS_CRIT_DATE_AFTER   3

#define FILE_IGNORE_ERROR OK
#define FILE_WARN_ERROR   INFO_ERROR
#define FILE_FAIL_ERROR   INVALID_DATA

typedef struct 
{
    bool select[NOBSTYPE];
} obs_datatype_criterion;

typedef struct 
{
    int file_id;
    char *filename;
} obs_datafile_criterion;

typedef struct 
{
    int class_id;
    int value_id;
} obs_classification_criterion;

typedef struct 
{
    int nobs_ids;
    int obs_id;
    int *obs_ids;
} obs_id_criterion;

typedef struct 
{
    int class_id;
    int nvalues;
    int *value_ids;
} mult_obs_classification_criterion;

typedef struct 
{
    unsigned char date_criterion_type;
    double date;
} obs_date_criterion;

typedef struct 
{
    char *station_list;
    char *config_filename;
    char *config_loc;
    void *criteria;
} obs_stations_criterion;

typedef struct obs_criterion_s
{
    unsigned char crit_type;
    union
    {
        obs_datatype_criterion datatype;
        obs_datafile_criterion datafile;
        obs_classification_criterion classification;
        mult_obs_classification_criterion mult_classification;
        obs_id_criterion id;
        obs_date_criterion date;
        obs_stations_criterion stations;
    } c;
    obs_criterion_s *next;
} obs_criterion;

typedef struct obs_criteria_s
{
    obs_criterion *first;
    obs_criterion *last;
    int action;
    double factor;
    struct obs_criteria_s *next;
} obs_criteria;

typedef struct
{
    obs_criteria *first;
    obs_criteria *last;
    network *nw;
    classifications *classes;
    fileid_func get_fileid;

} obs_modifications;


/*===============================================================================*/

static obs_criterion *new_obs_criterion()
{
    obs_criterion *oc=(obs_criterion *) check_malloc( sizeof(obs_criterion) );
    oc->crit_type=OBS_CRIT_NONE;
    oc->next=nullptr;
    return oc;
}

static obs_criterion *new_obs_datatype_criterion( CFG_FILE *cfg, char *datatypes )
{
    int sts=OK;
    char *typecode=datatypes;
    bool select[NOBSTYPE];
    obs_criterion *oc;
    if( ! datatypes ) return 0;

    for( int i=0; i<NOBSTYPE; i++ ) select[i]=false;

    while( *typecode)
    {
        char *end=typecode;
        int id;
        char savechr;
        while( *end && *end != '/' ) end++;
        savechr=*end;
        *end=0;
        id=datatype_from_code( typecode );
        if( id == NOBSTYPE )
        {
            char errmess[80];
            sprintf(errmess,"Invalid data type code %.20s in observation criteria",typecode);
            send_config_error( cfg, INVALID_DATA, errmess );
            sts=INVALID_DATA;
        }
        *end=savechr;
        typecode=end;
        if( *typecode ) typecode++;
        if( id < NOBSTYPE )
        {
            select[id]=true;
        }
    }

    if( sts != OK ) return nullptr;
    
    oc=new_obs_criterion();
    oc->crit_type=OBS_CRIT_DATATYPE;
    memcpy( &(oc->c.datatype.select), &(select[0]), sizeof(select));
    return oc;
}

static bool obs_datatype_match( obs_criterion *oc, survdata *sd, trgtdata *tgt )
{
    return oc->c.datatype.select[tgt->type];
}

static void describe_obs_datatype_criterion( FILE *lst, obs_criterion *oc, const char *prefix )
{
    int ntype=0;
    int itype=0;

    for( int i=0; i<NOBSTYPE; i++ ){ if( oc->c.datatype.select[i]){ ntype++; itype=i;}}

    if( ntype == 1 )
    {
        datatypedef *dtype=datatypedef_from_id(itype);
        fprintf(lst,"are of type %s (%s)\n",dtype->code,dtype->name);
    }
    else
    {
        fprintf(lst,"are of types:\n");
        for( int i=0; i<NOBSTYPE; i++ )
        {
            if( oc->c.datatype.select[i])
            {
                datatypedef *dtype=datatypedef_from_id(i);
                fprintf(lst,"%s        - %s (%s)\n",prefix, dtype->code, dtype->name);
            }
        }
    }
}

static obs_criterion *new_obs_datafile_criterion( int file_id, char *filename )
{
    obs_criterion *oc=new_obs_criterion();
    oc->crit_type=OBS_CRIT_DATAFILE;
    oc->c.datafile.file_id=file_id;
    oc->c.datafile.filename=copy_string(filename);
    return oc;
}

static bool obs_datafile_match( obs_criterion *oc, survdata *sd, trgtdata *tgt )
{
    return sd->file == oc->c.datafile.file_id;
}

static void delete_obs_datafile_criterion( obs_criterion *oc )
{
    check_free( oc->c.datafile.filename );
    oc->c.datafile.filename=nullptr;
}

static void describe_obs_datafile_criterion( FILE *lst, obs_criterion *oc, const char *prefix )
{
    fprintf(lst,"are from file %s\n",oc->c.datafile.filename);
}

static obs_criterion *new_obs_classification_criterion( CFG_FILE *cfg, classifications *classes, 
        char *classification, char *values, bool singlevalue )
{
    int class_id;
    obs_criterion *oc;

    if( ! classes ) return nullptr;
    class_id=classification_id( classes, classification, 1 );
    /* If values string contains / then this is a list of multiple classes */
    oc=new_obs_criterion();
    if( ! singlevalue && strchr(values,'/') )
    {
        char *pval;
        char *pend;
        int nval=1;
        for( pval=values; *pval; pval++ ){ if( *pval=='/' ) nval++; }

        oc->crit_type=OBS_CRIT_MCLASSIFICATION;
        oc->c.mult_classification.class_id=class_id;
        oc->c.mult_classification.nvalues=nval;
        oc->c.mult_classification.value_ids=(int *) check_malloc( nval*sizeof(int) );
        pval=values;
        nval=0;
        while( 1 )
        {
            char savechr;
            for( pend=pval; *pend; pend++ ){ if( *pend=='/' ) break; }
            savechr=*pend;
            *pend=0;
            oc->c.mult_classification.value_ids[nval]=class_value_id( classes, class_id, pval, 1 );
            nval++;
            if( ! savechr ) break;
            *pend=savechr;
            pval=pend+1;
        }
    }
    /* Otherwise a single class */
    else
    {
        oc->crit_type=OBS_CRIT_CLASSIFICATION;
        oc->c.classification.class_id=class_id;
        oc->c.classification.value_id=class_value_id( classes, class_id, values, 1 );
    }
    return oc;
}

static void delete_mult_obs_classification( obs_criterion *oc )
{
    check_free( oc->c.mult_classification.value_ids );
    oc->c.mult_classification.value_ids = nullptr;
}

static bool obs_classification_match( obs_criterion *oc, survdata *sd, trgtdata *tgt )
{
    int value_id=get_obs_classification_id( sd, tgt, oc->c.classification.class_id );
    return value_id == oc->c.classification.value_id;
}

static bool obs_mult_classification_match( obs_criterion *oc, survdata *sd, trgtdata *tgt )
{
    int value_id=get_obs_classification_id( sd, tgt, oc->c.mult_classification.class_id );
    for( int i=0; i < oc->c.mult_classification.nvalues; i++ )
    {
        if( oc->c.mult_classification.value_ids[i] == value_id ) return true;
    }
    return false;
}

static void describe_obs_classification_criterion( FILE *lst, obs_criterion *oc, const char *prefix, classifications *classes )
{
    fprintf(lst,"where %s classification is \"%s\"\n",
            classification_name( classes, oc->c.classification.class_id),
            class_value_name( classes, oc->c.classification.class_id, oc->c.classification.value_id));
}

static void describe_obs_mult_classification_criterion( FILE *lst, obs_criterion *oc, const char *prefix, classifications *classes )
{
    int class_id=oc->c.mult_classification.class_id;

    fprintf(lst,"where %s classification is one of:\n",
            classification_name( classes, class_id ));
    for( int i=0; i < oc->c.mult_classification.nvalues; i++ )
    {
        fprintf(lst,"%s    - \"%s\"\n",prefix,
            class_value_name( classes, class_id, oc->c.mult_classification.value_ids[i]));
    }
}


static obs_criterion *new_obs_id_criterion( CFG_FILE *cfg, char *idstr )
{
    int obs_id=0;
    int *obs_ids=&obs_id;
    char *pval;
    char *pend;
    int nval=1;
    obs_criterion *oc;
    for( pval=idstr; *pval; pval++ ){ if( *pval=='/' ) nval++; }
    if( nval > 1 ) obs_ids=(int *) check_malloc( nval*sizeof(int) );
    pval=idstr;
    nval=0;
    while( 1 )
    {
        char savechr;
        char chk[2];
        int ival;
        for( pend=pval; *pend; pend++ ){ if( *pend=='/' ) break; }
        savechr=*pend;
        *pend=0;
        chk[0]=0;
        if( sscanf( pval, "%d%1s", &ival , chk ) < 1 || chk[0] ) 
        {
            char errmsg[100];
            sprintf( errmsg,"Invalid observation id \"%.50s\" in observation criteria",pval);
            send_config_error( cfg, INVALID_DATA, errmsg );
            if( nval > 1 ) check_free( obs_ids );
            return nullptr;
        }
        obs_ids[nval]=ival;
        nval++;
        if( ! savechr ) break;
        *pend=savechr;
        pval=pend+1;
    }
    oc=new_obs_criterion();
    oc->crit_type=OBS_CRIT_ID;
    oc->c.id.obs_id=obs_id;
    oc->c.id.obs_ids=nval == 1 ? &(oc->c.id.obs_id) : obs_ids;
    oc->c.id.nobs_ids=nval;
    return oc;
}

static void delete_obs_id_criterion( obs_criterion *oc )
{
    if( oc->c.id.nobs_ids > 1 ){check_free( oc->c.id.obs_ids ); oc->c.id.obs_ids=0; }
}

static bool obs_id_match( obs_criterion *oc, survdata *sd, trgtdata *tgt )
{
    for( int i=0; i < oc->c.id.nobs_ids; i++ ){ if( oc->c.id.obs_ids[i] == tgt->id ) return true; }
    return false;
}

static void describe_obs_id_criterion( FILE *lst, obs_criterion *oc, const char *prefix )
{
    if( oc->c.id.nobs_ids == 1 )
    {
        fprintf(lst,"where the observation id is %d\n",oc->c.id.obs_id);
    }
    else
    {
        fprintf(lst,"where the observation id is one of:\n");
        for( int i=0; i < oc->c.id.nobs_ids; i++ )
        {
            fprintf(lst,"%s    - %d\n", prefix, oc->c.id.obs_ids[i]);
        }
    }
}

static obs_criterion *new_obs_date_criterion( CFG_FILE *cfg, unsigned char date_crit_type, char *datestr  )
{
    obs_criterion *oc;
    double date=UNDEFINED_DATE;

    if( date_crit_type == OBS_CRIT_DATE_BEFORE || date_crit_type == OBS_CRIT_DATE_AFTER )
    {
        date=snap_datetime_parse(datestr,0);
        if( date == UNDEFINED_DATE )
        {
            char errmsg[100];
            sprintf( errmsg,"Invalid date \"%.50s\" in observation date criteria",datestr);
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

static bool obs_date_match( obs_criterion *oc, survdata *sd, trgtdata *tgt )
{
    bool result=false;

    switch( oc->c.date.date_criterion_type )
    {
        case OBS_CRIT_DATE_BEFORE: 
            result=sd->date != UNDEFINED_DATE && sd->date < oc->c.date.date; 
            break;
        case OBS_CRIT_DATE_AFTER: 
            result=sd->date != UNDEFINED_DATE && sd->date > oc->c.date.date; 
            break;
        default: 
            result=sd->date == UNDEFINED_DATE; 
            break;
    }
    return result;
}

static void describe_obs_date_criterion( FILE *lst, obs_criterion *oc, const char *prefix )
{
    if( oc->c.date.date_criterion_type == OBS_CRIT_DATE_UNKNOWN )
    {
        fprintf(lst,"have no observation date\n");
    }
    else
    {
        fprintf(lst,"are observed %s %s\n",
            oc->c.date.date_criterion_type == OBS_CRIT_DATE_BEFORE ? 
            "before" : "after",
            date_as_string(oc->c.date.date,0,0) );
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

static obs_criterion *new_obs_stations_criterion( CFG_FILE *cfg, unsigned char station_crit_type, char *station_list )
{
    obs_criterion *oc;
    if( station_crit_type != OBS_CRIT_STATION_BETWEEN ) station_crit_type=OBS_CRIT_STATION_USES;
    oc=new_obs_criterion();
    oc->crit_type=station_crit_type;
    oc->c.stations.config_loc=copy_string(get_config_location(cfg));
    oc->c.stations.config_filename=copy_string(get_config_filename(cfg));
    oc->c.stations.station_list=copy_string(station_list);
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
    
static bool obs_stations_match( obs_criterion *oc, network *nw, survdata *sd, trgtdata *tgt )
{

    if( ! oc->c.stations.criteria )
    {
        init_obs_stations_criterion( oc, nw  );
    }

    if( sd->from > 0 )
    {
        bool match=station_criteria_match( oc->c.stations.criteria, station_ptr( nw, sd->from ));
        if( oc->crit_type == OBS_CRIT_STATION_USES )
        {
            if( match ) return true;
        }
        else
        {
            if( ! match ) return false;
        }
    }
    if( tgt->to )
    {
        return station_criteria_match( oc->c.stations.criteria, station_ptr( nw, tgt->to ));
    }
    return false;
}

static void describe_obs_stations_criterion( FILE *lst, obs_criterion *oc, const char *prefix )
{
    fprintf(lst,"%s stations %s\n",
            oc->crit_type == OBS_CRIT_STATION_USES ? "use" : "are between",
            oc->c.stations.station_list );
}

static bool obs_criterion_match( obs_criterion *oc, network *nw, survdata *sd, trgtdata *tgt )
{
    switch( oc->crit_type )
    {
        case OBS_CRIT_DATATYPE: return  obs_datatype_match( oc, sd, tgt );
        case OBS_CRIT_DATAFILE: return  obs_datafile_match( oc, sd, tgt );
        case OBS_CRIT_CLASSIFICATION: return  obs_classification_match( oc, sd, tgt );
        case OBS_CRIT_MCLASSIFICATION: return  obs_mult_classification_match( oc, sd, tgt );
        case OBS_CRIT_ID: return  obs_id_match( oc, sd, tgt );
        case OBS_CRIT_DATE: return  obs_date_match( oc, sd, tgt );
        case OBS_CRIT_STATION_USES: return  obs_stations_match( oc, nw, sd, tgt );
        case OBS_CRIT_STATION_BETWEEN: return  obs_stations_match( oc, nw, sd, tgt );
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
        case OBS_CRIT_ID: delete_obs_id_criterion( oc ); break;
    }
    check_free( oc );
}

static obs_criteria *new_obs_criteria( int action, double factor )
{
    obs_criteria *ocr=(obs_criteria *) check_malloc( sizeof( obs_criteria ) );
    ocr->action=action;
    ocr->factor=factor;
    ocr->first=nullptr;
    ocr->last=nullptr;
    ocr->next=nullptr;
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

static void apply_obs_criteria_action( obs_criteria *ocr, network *nw, survdata *sd, trgtdata *tgt, int *paction, double *factor )
{
    int action=*paction;
    if( ! (action & OBS_MOD_IGNORE ) )
    {

        for( obs_criterion *oc=ocr->first; oc; oc=oc->next )
        {
            if( ! obs_criterion_match( oc, nw, sd, tgt ) )
            {
                return;
            }
        }

        if( ocr->action & OBS_MOD_IGNORE )
        {
            action = OBS_MOD_IGNORE;
            *factor = 1.0;
        }
        else
        {
            action |= ocr->action;
            if( ocr->action & OBS_MOD_REWEIGHT ) *factor *= ocr->factor;
        }
    }
    *paction=action;
}

static bool obs_criteria_ignore_datafile( obs_criteria *ocr, int file_id )
{
    if( ! (ocr->action & OBS_MOD_IGNORE ) ) return false;
    bool matched=false;
    for( obs_criterion *oc=ocr->first; oc; oc=oc->next )
    {
        if( oc->crit_type != OBS_CRIT_DATAFILE ) return false;
        if( oc->c.datafile.file_id == file_id ) matched=true;
    }
    return matched;
}

static void summarize_obs_criteria( FILE *lst, const char *prefix, obs_criteria *ocr, classifications *classes )
{
    bool just1=ocr->first->next == nullptr;

    fprintf(lst,"%s  Observations which%s", prefix, just1 ? " " : ":\n");
    for( obs_criterion *oc=ocr->first; oc; oc=oc->next )
    {
        if( ! just1 ) fprintf(lst,"%s    - ",prefix);
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
}

void *new_obs_modifications( network *nw, classifications *obs_classes )
{
    obs_modifications *obsmod = (obs_modifications *) check_malloc( sizeof( obs_modifications ) );
    obsmod->first=nullptr;
    obsmod->last=nullptr;
    obsmod->nw=nw;
    obsmod->classes=obs_classes;
    obsmod->get_fileid=nullptr;
    return (void *) obsmod;
}

void set_obs_modifications_network( void *pobsmod, network *nw )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obsmod->nw=nw;
}

void set_obs_modifications_file_func( void *pobsmod, fileid_func idfunc )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obsmod->get_fileid=idfunc;
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
}


static int get_file_id( obs_modifications *obsmod, CFG_FILE *cfg, char *datafile, int missing_error )
{
    if( ! obsmod->get_fileid )
    {
        handle_error( INTERNAL_ERROR, 
            "Program error: Survey file id function not initiallized in observation modifications",
            nullptr );
        return -1;
    }

    int file_id = obsmod->get_fileid( datafile, get_config_directory(cfg) );
    if( file_id < 0 && missing_error != OK )
    { 
        char errmess[120];
        sprintf(errmess,"Invalid data_file %.60s in classification command",datafile);
        send_config_error( cfg, missing_error, errmess );
    }
    return file_id;
}

int add_obs_modifications( CFG_FILE *cfg, void *pobsmod, char *criteria, int action, double err_factor )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obs_criteria *ocr=new_obs_criteria( action, err_factor );
    char *strptr=criteria;
    char *field;
    char *fptr;
    int sts=OK;
    int missing_error=INVALID_DATA;
    bool have_criterion=false;
    while( (field=next_field(&strptr)) )
    {
        obs_criterion *oc=nullptr;

        if( _stricmp(field,"ignore_missing") == 0 ){ missing_error=OK; continue; }
        if( _stricmp(field,"warn_missing") == 0 ){ missing_error=INFO_ERROR; continue; }
        if( _stricmp(field,"fail_missing") == 0 ){ missing_error=INVALID_DATA; continue; }

        if( (fptr = strchr(field,'=')) )
        {
            *fptr=0;
            char quote=0;
            char *vptr=fptr+1;
            if( fptr == field || ! *vptr )
            {
                char errmess[100];
                *fptr='=';
                sprintf(errmess,"Invalid observation selection criteria \"%.40s\"",field);
                send_config_error(cfg,INVALID_DATA,errmess);
                sts=INVALID_DATA;
                continue;
            }
            if( *vptr == '"' || *vptr == '\'')
            {
                char *vend=vptr+1;
                quote=*vptr;
                vptr=vend;
                while( vend < strptr )
                {
                    if( ! *vend ) *vend=' ';
                    vend++;
                }
                vend=vptr;
                while( *vend && *vend != quote ) vend++;
                if( ! *vend  || !(*(vend+1) == 0 || isspace(*(vend+1))) )
                {
                    char errmess[100];
                    strptr=vend;
                    if( *strptr ) strptr++;
                    *fptr='=';
                    sprintf(errmess,"Invalid observation selection criteria for \"%.40s\"",field);
                    send_config_error(cfg,INVALID_DATA,errmess);
                    sts=INVALID_DATA;
                    continue;
                }
                *vend=0;
                strptr=vend+1;
            }
            if( _stricmp(field,"data_type") == 0 )
            {
                oc=new_obs_datatype_criterion(cfg,vptr);
            }
            else if( _stricmp(field,"data_file") == 0 )
            {
                int file_id=get_file_id( obsmod, cfg, vptr, missing_error );
                if( file_id >= 0 ) oc=new_obs_datafile_criterion(file_id,vptr );
            }
            else if( _stricmp(field,"id") == 0 )
            {
                oc=new_obs_id_criterion(cfg,vptr );
            }
            else
            {
                oc=new_obs_classification_criterion(cfg, obsmod->classes, field, vptr, quote != 0 );
            }
            *fptr='=';
        }
        else if( _stricmp(field,"before") == 0 )
        {
            field=next_field(&strptr);
            if( ! field )
            {
                send_config_error(cfg,INVALID_DATA,"Missing date in before observation selection criteria");
                sts=INVALID_DATA;
            }
            else
            {
                oc=new_obs_date_criterion( cfg, OBS_CRIT_DATE_BEFORE, field );
            }
        }
        else if( _stricmp(field,"after") == 0 )
        {
            field=next_field(&strptr);
            if( ! field )
            {
                send_config_error(cfg,INVALID_DATA,"Missing date in after observation selection criteria");
                sts=INVALID_DATA;
            }
            else
            {
                oc=new_obs_date_criterion( cfg, OBS_CRIT_DATE_AFTER, field );
            }
        }
        else if( _stricmp(field,"date_unknown") == 0 )
        {
            oc=new_obs_date_criterion( cfg, OBS_CRIT_DATE_UNKNOWN, nullptr );
        }
        else if( _stricmp(field,"using_stations")==0 || 
                _stricmp(field,"between_stations")==0 )
        {
            int station_crit_type= _stricmp(field,"between_stations") == 0 ?
                OBS_CRIT_STATION_BETWEEN : OBS_CRIT_STATION_USES;
            field=next_field(&strptr);
            if( ! field )
            {
                send_config_error(cfg,INVALID_DATA,"Missing station list in observation selection criteria");
                sts=INVALID_DATA;
            }
            else
            {
                fptr=field;
                while( field && _stricmp(field,"end_stations") != 0 ) field=next_field(&strptr);
                for( char *c=fptr; c < strptr; c++ ) { if( ! *c ) (*c)=' '; }
                if( field ){ *field = 0; }
                oc=new_obs_stations_criterion( cfg, station_crit_type, fptr );
            }
        }
        else
        {
            char errmess[120];
            sprintf(errmess,"Invalid specification %.50s in observation selection criteria",field);
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

int add_obs_modifications_classification( CFG_FILE *cfg, void *pobsmod, char *classification, char *value, int action, double err_factor, int missing_error )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obs_criterion *oc=nullptr;
    
    if( _stricmp(classification,"data_type") == 0 )
    {
        oc=new_obs_datatype_criterion(cfg,value);
    }
    else if( _stricmp(classification,"data_file") == 0 )
    {
        int file_id=get_file_id( obsmod, cfg, value, missing_error );
        if( file_id >= 0 ) oc=new_obs_datafile_criterion( file_id,value );
    }
    else if( _stricmp(classification,"id") == 0 )
    {
        oc=new_obs_id_criterion(cfg,value);
    }
    else
    {
        oc=new_obs_classification_criterion(cfg, obsmod->classes, classification, value, true );
    }
    if( ! oc )
    {
        return INVALID_DATA;
    }
    obs_criteria *ocr=new_obs_criteria( action, err_factor );
    add_obs_criterion_to_criteria( ocr, oc );
    add_obs_criteria_to_modifications( obsmod, ocr );
    return OK;
}

int add_obs_modifications_datafile_factor( CFG_FILE *cfg, void *pobsmod, int fileid, char *filename, double err_factor )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    obs_criterion *oc = new_obs_datafile_criterion( fileid, filename );
    obs_criteria *ocr=new_obs_criteria( OBS_MOD_REWEIGHT, err_factor );
    add_obs_criterion_to_criteria( ocr, oc );
    add_obs_criteria_to_modifications( obsmod, ocr );
    return OK;
}

static double apply_obs_modification_action( obs_modifications *obsmod, survdata *sd, trgtdata *tgt )
{
    int action=0;
    double factor=1.0;

    if( tgt->unused & IGNORE_OBS_BIT ) return factor;
    
    for( obs_criteria *ocr=obsmod->first; ocr; ocr=ocr->next )
    {
        apply_obs_criteria_action(ocr,obsmod->nw,sd,tgt,&action,&factor);
    }

    if( action & OBS_MOD_IGNORE ) 
    {
        tgt->unused |= IGNORE_OBS_BIT;
    }
    else if( action & OBS_MOD_REJECT )
    {
        tgt->unused |= REJECT_OBS_BIT;
    }
    return factor;
}

int apply_obs_modifications( void *pobsmod, survdata *sd )
{
    double factor=1.0;
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    int nignored=0;
    int i;

    switch (sd->format)
    {

    case SD_OBSDATA:
    {
        obsdata *od;
        for( i = 0, od=sd->obs.odata; i<sd->nobs; i++, od++ )
        {
            trgtdata *tgt=&(od->tgt);
            if( obsmod && ! (tgt->unused & IGNORE_OBS_BIT) )
            {
                factor=apply_obs_modification_action( obsmod, sd, tgt );
                if( tgt->unused & IGNORE_OBS_BIT ) nignored++;
            }
            od->error  *= factor;
        }
    }
    break;

    case SD_VECDATA:
    {
        vecdata *vd;
        int ncvr=sd->nobs*3;

        for( i = 0, vd=sd->obs.vdata; i<sd->nobs; i++, vd++ )
        {
            trgtdata *tgt=&(vd->tgt);
            factor=1.0;
            if( obsmod && ! (tgt->unused & IGNORE_OBS_BIT) )
            {
                factor=apply_obs_modification_action( obsmod, sd, tgt );
                if( tgt->unused & IGNORE_OBS_BIT ) { nignored++; }
            }
            if( sd->cvr && factor != 1.0 && ! (tgt->unused & IGNORE_OBS_BIT) )
            {
                int i3=i*3;
                double factor2=factor*factor;
                for( int row=0; row<i3; row++ )
                {
                    for( int col=i3; col < i3+3; col++ ) Lij(sd->cvr,row,col) *= factor;
                }
                for( int row=i3; row<i3+3; row++ )
                {
                    for( int col=i3; col <= row; col++ ) Lij(sd->cvr,row,col) *= factor2;
                }
                for( int row=i3+3; row<ncvr; row++ )
                {
                    for( int col=i3; col < i3+3; col++ ) Lij(sd->cvr,row,col) *= factor;
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
            if( obsmod && ! (tgt->unused & IGNORE_OBS_BIT) )
            {
                factor=apply_obs_modification_action( obsmod, sd, tgt );
                if( tgt->unused & IGNORE_OBS_BIT ) { nignored++; }
            }
            pd->error  *= factor;
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
        if( obs_criteria_ignore_datafile( ocr, file_id ) ) return true;
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

void summarize_obs_modifications( void *pobsmod, FILE *lst, const char *prefix )
{
    obs_modifications *obsmod = (obs_modifications *) pobsmod;
    if( ! obsmod ) return;

    for( int modtype=0; modtype < 3; modtype++ )
    {
        int action= modtype==0 ? OBS_MOD_IGNORE : 
                    modtype==1 ? OBS_MOD_REJECT : OBS_MOD_REWEIGHT;

        bool firsterr=true;
        double minerrfct=0.0;
        obs_criteria *ocr;
        int ncriteria=0;
        int maxcriteria=0;
        for( ocr=obsmod->first; ocr; ocr=ocr->next ){ maxcriteria++; }

        while( 1 )
        {
            obs_criteria* match=nullptr;
            double errfct=0.0;
            for( ocr=obsmod->first; ocr; ocr=ocr->next )
            {
                if( ocr->action & action )
                {
                    if( action == OBS_MOD_REWEIGHT ) 
                    {
                        if( (firsterr || ocr->factor < minerrfct) && ocr->factor > errfct )
                        {
                            errfct=ocr->factor;
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
                fprintf(lst,"\n%sThe following observations are ignored:\n",prefix);
            }
            else if( action == OBS_MOD_REJECT )
            {
                fprintf(lst,"\n%sThe following observations are rejected\n",prefix);
            }
            else
            {
                fprintf(lst,"\n%sErrors of the following observations are scaled by %.3lf\n",
                        prefix,errfct);
                minerrfct=errfct;
                firsterr=false;
            }
            while( match )
            {
                summarize_obs_criteria( lst, prefix, match, obsmod->classes );
                ncriteria++;
                match=match->next;
                while( match )
                {
                    if( match->action & action )
                    {
                        if( action != OBS_MOD_REWEIGHT || match->factor == errfct ) break;
                    }
                    match=match->next;
                }
            }
            if( action != OBS_MOD_REWEIGHT ) break;
            /* Break out just in case reweighting matching doesn't work */
            if( ncriteria >= maxcriteria ) break;
        }

        if( action == OBS_MOD_REWEIGHT && ncriteria > 1 )
        {
            fprintf(lst,"\n%sNote: error factors are multiplied for observations meeting several critera\n",prefix);
        }
    }
}
