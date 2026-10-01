#include "snapconfig.h"
/* Routines for reading SNAP format data files */

/* Tidy up handling of errors, especially use of group_err and
   definition_err, and aborting data sent to loaddata.c functions */

/* Make sure rejections work correctly */

/* Look at data_file routines returning simply a pointer to the text, not
   the text itself.  Would be considerably more efficient.. */

/* Correct use of NOBSTYPE when we really mean number of snap_data_type
   entries... */

/* Handling of vector covariance type is very messy..
   see sd->dfltcvrtype, sd->cvrtype, sd->nveccvr */

/*
   $Log: snapdata.c,v $
   Revision 1.4  2004/04/22 02:35:15  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.3  1997/06/16 06:47:18  CHRIS
   Fixed errors with reading errors of GPS data - ignored error xx xx xx on
   data line, and problem if gps_error_type not defined (default not correctly
   set).

   Revision 1.2  1996/02/19 19:02:33  CHRIS
   Fixed a bug reading #distance_scale_error commands which caused the command
   to be ignored.

   Revision 1.1  1995/12/22 18:48:22  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include "util/snapctype.h"

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif
#include <assert.h>

#include "snapdata/snapdata.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "snapdata/datatype.h"
#include "util/datafile.h"
#include "snapdata/loaddata.h"
#include "snapdata/survdata.h"
#include "util/dateutil.h"
#include "util/errdef.h"
#include "util/linklist.h"
#include "util/symmatrx.h"
#include "util/progress.h"
#include "util/pi.h"

/* Some useful names and numbers */

#define MAXVECERR  6    /* Maximum number of vector errors components per line */

/* The most characters read for each kind of word - longer words are cut short */

static constexpr size_t MAX_COMMAND_LEN = 79;
static constexpr size_t MAX_NOTE_LEN = 79;
static constexpr size_t MAX_NAME_LEN = 19;
static constexpr size_t MAX_DATE_FIELD_LEN = 31;
static constexpr size_t MAX_WORD_LEN = 9;


#define COMMAND_PREFIX '#'
#define REJECT_CHAR    '*'
#define SKIPOBS_CHAR   '-'

#define ERROR1         "error"
#define ERROR2         "+/-"

using boost::numeric_cast;

/* Definition of a data field in the file */

struct data_field
{
    int type;   /* Data type */
    int id;     /* Data type id */
    int sec_id; /* Secondary id - eg for classified systematic errors */
};

/* Valid types of data field */

enum { DFT_START,          /* Start of a group relating to an observation
                 id     = type number in snap_type array
                 sec_id = combination of flags
                      FLG_DFLT_DATA and FLG_DFLT_ERROR */
       DFT_DATA,           /* Data item for the group
                 id     = type number in snap type array */
       DFT_ERROR,          /* Error item for the group
                 id     = type number in snap type array */
       DFT_TIME,           /* Time of the observation */
       DFT_DATE,           /* Date of the observation */
       DFT_OBSID,          /* Id of the observation */
       DFT_CLASS,          /* Classification item for the group
                 id     = index into array of cclass types */
       DFT_SYSERR
     };       /* Systematic error
                 id     = systematic error id
                 sec_id = id of classification it depends on,
                      or -1. */

#define FLG_DFLT_DATA  1
#define FLG_DFLT_ERROR 2

/* Structures used to hold lists of valid classifications and systematic errors */

struct data_class
{
    std::string name;
    int class_id;
    int id[NOBSTYPE];
    int data_id;        /* Id for the current data field group */
};

struct data_syserr
{
    std::string name;
    int syserr_id;
    int class_id;
    int class_name_id;  /* Cache the last value */
    bool defined;         /* Defined for current data field group */
    bool vector;          /* True if the current systematic error is a vector */
    double influence[3];  /* Influence for current data field */
};

/* Structure for holding a vector error specification */

struct vecerr_def
{
    int nvecobs;
    double vecerr[MAXVECERR];
};

/* Structure holding the current state of the file */

struct snapfile_def
{
    explicit snapfile_def( DATAFILE *datafile );
    snapfile_def( const snapfile_def & ) = delete;
    snapfile_def &operator=( const snapfile_def & ) = delete;
    ~snapfile_def();

    /// The scanner over the current record of the data file
    FieldScanner &scanner() { return df->input_string().scanner; }

    /// Reports an error in the current record of the data file
    int error( int sts, std::string_view errmsg ) { return df->error( sts, errmsg ); }

    DATAFILE *df;

    std::vector<data_field> fields;     /* Definitions of data fields */
    std::vector<data_class> clsf;       /* Definitions of classifications */
    std::vector<data_syserr> syserr;    /* Definitions of systematic errors */

    int coef_class_id[N_COEF_CLASSES];

    double dserr = -1, dsppmerr = -1;    /* Definitions of errors */
    double haerr = -1, hammerr = -1;
    double azerr = -1, azmmerr = -1;
    double zderr = -1, zdmmherr = -1, zdmmverr = -1;
    double lverr = -1;
    double lnerr = -1, lterr = -1;
    double oherr = -1;
    double eherr = -1;
    double gpserr[9] = { -1 };
    double gpterr[9] = { -1 };

    int gotdflterr[NERRTYPE] = {};

    /* Definitions of coefficients */
    int dmsformat;
    int refcoef = 0;      /* Default values - 0 = none */
    int distsf = 0;
    int brngref = 0;

    int refframe = 0;     /* Definition of the reference frame */
    int projctn = 0;      /* Projection id */
    int usereffrm = 0;    /* True if current data uses reffrm/projection */
    int useprojctn = 0;

    int obs_refcoef = 0;  /* As applying to the current observation */
    int obs_distsf = 0;
    int obs_brngref = 0;


    double date = UNDEFINED_DATE;        /* Miscellaneous data */
    double value = 0.0;       /* Used for default error of scalar data */
    snap_data_type *obstype = nullptr;      /* Current observation type */
    int stn_id_inst = 0;
    int stn_id_trgt = 0;
    int obsclass = 0;
    int rejobs = 0;
    int skipobs = 0;
    int inobs = 0;
    int noinststn = 0;
    int grouped = 0;
    int ingroup = 0;
    int rejgroup = 0;
    int goterr = 0;
    int endset = 0;       /* Requires definition of end of group */
    int heights = 0;
    int nveccvr = 0;
    int cvrupper = 0;
    int nvecobs = 0;
    int nvecgood = 0;
    void  *vecerrlst = nullptr;   /* Link list used to hold vector errors as they are read */
    std::vector<int> cvrrow;
    vecerr_def *currvecerr = nullptr;
    int dfltcvrtype = CVR_FULL;
    int cvrtype = 0;

    int definition_err = 0;    /* Error status */
    int group_err = 0;

};


/* Structure used to define valid specification commands within the data file */

static int read_error_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_angle_type_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_date_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_time_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_proj_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_coef_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_gps_errtype_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_syserr_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_classification( snapfile_def *sd, int id, std::string_view cmd );
static int read_classify_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_endset_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_note_command( snapfile_def *sd, int id, std::string_view cmd );
static int read_data_command( snapfile_def *sd, int id, std::string_view cmd );

struct command
{
    std::string_view command;
    int id;
    int (*action)( snapfile_def *sd, int id, std::string_view cmd );
    int flags;
};

#define CMD_ENDDATA 0x01
#define CMD_ENDSET  0x02

#define AF_HP  0
#define AF_DMS 1
#define AF_DEG 2

static command commands[] =
{
    {"ds_error",DS_ERR,read_error_command,CMD_ENDDATA},
    {"ha_error",HA_ERR,read_error_command,CMD_ENDDATA},
    {"az_error",AZ_ERR,read_error_command,CMD_ENDDATA},
    {"zd_error",ZD_ERR,read_error_command,CMD_ENDDATA},
    {"lv_error",LV_ERR,read_error_command,CMD_ENDDATA},
    {"lt_error",LT_ERR,read_error_command,CMD_ENDDATA},
    {"ln_error",LN_ERR,read_error_command,CMD_ENDDATA},
    {"oh_error",OH_ERR,read_error_command,CMD_ENDDATA},
    {"eh_error",EH_ERR,read_error_command,CMD_ENDDATA},
    {"gps_enu_error",GB_ERR,read_error_command,CMD_ENDDATA},
    {"gb_enu_error",GB_ERR,read_error_command,CMD_ENDDATA},
    {"gx_enu_error",GX_ERR,read_error_command,CMD_ENDDATA},
    {"hp_angles", AF_HP, read_angle_type_command,CMD_ENDDATA},
    {"dms_angles", AF_DMS, read_angle_type_command,CMD_ENDDATA},
    {"deg_angles", AF_DEG, read_angle_type_command,CMD_ENDDATA},
    {"date", 0, read_date_command,CMD_ENDDATA},
    {"time", 0, read_time_command,CMD_ENDDATA},
    {"refraction_coefficient",COEF_CLASS_REFCOEF, read_coef_command,CMD_ENDDATA},
    {"distance_scale_error",COEF_CLASS_DISTSF, read_coef_command,CMD_ENDDATA},
    {"bearing_orientation_error",COEF_CLASS_BRNGREF, read_coef_command,CMD_ENDDATA},
    {"reference_frame",COEF_CLASS_REFFRM, read_coef_command,CMD_ENDDATA},
    {"projection",0, read_proj_command,CMD_ENDDATA},
    {"gps_error_type",0,read_gps_errtype_command,CMD_ENDDATA},
    {"systematic_error",0,read_syserr_command,CMD_ENDDATA},
    {"classification",0,read_classification,CMD_ENDDATA},
    {"classify",0,read_classify_command,CMD_ENDDATA},
    {"end_set",0,read_endset_command,CMD_ENDDATA | CMD_ENDSET},
    {"",0,read_endset_command,CMD_ENDDATA | CMD_ENDSET},
    {"note",0,read_note_command,0},
    {"data",0,read_data_command,CMD_ENDDATA}
};

snapfile_def::snapfile_def( DATAFILE *datafile ) :
    df( datafile ),
    dmsformat( AF_DMS )
{
    std::fill( std::begin( coef_class_id ), std::end( coef_class_id ), -1 );
}

snapfile_def::~snapfile_def()
{
    if( vecerrlst ) free_list( vecerrlst, NO_ACTION );
}

/*===============================================================*/
/* Get observation snap type from its code                       */

static snap_data_type *obstype_from_code( std::string_view code )
{
    snap_data_type *sdt = 0;
    datatypedef *dt = datatypedef_from_code( code );
    if( dt )
    {
        sdt = snap_data_type_from_id( dt->id );
    }
    return sdt;
}


/*===============================================================*/
/* Management of classifications                                 */

static int get_classification( snapfile_def *sd, std::string_view cclass )
{
    for( size_t i = 0; i < sd->clsf.size(); i++ )
    {
        if( boost::algorithm::iequals( cclass, sd->clsf[i].name ) ) return numeric_cast<int>( i );
    }
    return -1;
}

static int create_classification( snapfile_def *sd, std::string_view cclass )
{
    const int ic = get_classification( sd, cclass );
    if( ic >= 0 ) return ic;

    data_class cd{};
    cd.name = cclass;
    cd.class_id = numeric_cast<int>( ldt_get_id( ID_CLASSTYPE, 0, cclass ) );
    sd->clsf.push_back( cd );

    return numeric_cast<int>( sd->clsf.size() ) - 1;
}

static void set_obstype_classification( snapfile_def *sd, int type,
                                        int clsf_id, std::string_view name )
{
    data_class &cd = sd->clsf[clsf_id];
    int name_id = 0;

    if( ! boost::algorithm::iequals( name, "none" ) )
    {
        name_id = numeric_cast<int>( ldt_get_id( ID_CLASSNAME, cd.class_id, name ) );
    }
    if( type >= 0 )
    {
        cd.id[type] = name_id;
    }
    else
    {
        std::fill( std::begin( cd.id ), std::end( cd.id ), name_id );
    }
}


/* Loading classifications for an observation.  First call
   init_data_classifications for the type, then read_data_classification
   for each DFT_CLASS field for the observation,
   then load_data_classifications when done */

static void init_data_classifications( snapfile_def *sd )
{
    for( data_class &cd : sd->clsf )
    {
        cd.data_id = cd.id[sd->obstype->type];
    }
}

static int read_data_obs_id( snapfile_def *sd )
{
    int id = 0;
    if( read_int_field( sd->scanner(), id ) == FieldResult::Ok )
    {
        ldt_obs_id( id );
    }
    else
    {
        sd->error( INVALID_DATA,
                   "Observation id is missing or invalid - must be an integer number");
    }

    return OK;
}

static int read_data_classification( snapfile_def *sd, data_field *fld )
{
    std::string name;
    int name_id = 0;

    if( read_string_field( sd->scanner(), name, MAX_NAME_LEN ) != FieldResult::Ok )
    {
        sd->error( MISSING_DATA,
                   "Classification " + ldt_get_code( ID_CLASSTYPE, 0, sd->clsf[fld->id].class_id ) + " is missing" );
        ldt_cancel_data();
        return 0;
    }

    assert( fld->type == DFT_CLASS );
    assert( fld->id >= 0 && static_cast<size_t>( fld->id ) <= sd->clsf.size() );

    data_class &cd = sd->clsf[fld->id];
    if( ! boost::algorithm::iequals( name, "none" ) )
    {
        name_id = numeric_cast<int>( ldt_get_id( ID_CLASSNAME, cd.class_id, name ) );
    }
    cd.data_id = name_id;
    return 1;
}

static void load_data_classifications( snapfile_def *sd )
{
    for( const data_class &cd : sd->clsf )
    {
        if( cd.data_id > 0 ) ldt_classification( cd.class_id, cd.data_id );
    }
}

/*============================================================*/
/* Management of systematic errors                            */


static int get_syserr( snapfile_def *sd, std::string_view sename )
{
    for( size_t i = 0; i < sd->syserr.size(); i++ )
    {
        if( boost::algorithm::iequals( sename, sd->syserr[i].name ) ) return numeric_cast<int>( i );
    }
    return -1;
}

static int create_syserr( snapfile_def *sd, std::string_view sename, int class_id )
{
    const int ic = get_syserr( sd, sename );
    if( ic >= 0 )
    {
        if( sd->syserr[ic].class_id != class_id )
        {
            sd->error( INVALID_DATA, "Cannot redefine systematic error " + std::string( sename ) );
        }
        return ic;
    }

    data_syserr ds{};
    ds.name = sename;
    ds.syserr_id = class_id >= 0 ? 0 : numeric_cast<int>( ldt_get_id( ID_SYSERR, 0, sename ) );
    ds.class_id = class_id;
    ds.class_name_id = -1;
    sd->syserr.push_back( ds );

    return numeric_cast<int>( sd->syserr.size() ) - 1;
}


/* Procedure for loading systematic errors is analogous to that for
   classifications, ie init_data_syserrs for each observations, then
   read_data_syserr for each DFT_SYSERR field, then load_data_syserrs */

static void init_data_syserrs( snapfile_def *sd )
{
    for( data_syserr &ds : sd->syserr )
    {
        ds.defined = false;
    }
}

static int read_data_syserr( snapfile_def *sd, data_field *fld )
{
    assert( fld->type == DFT_SYSERR );
    assert( fld->id >= 0 && static_cast<size_t>( fld->id ) <= sd->syserr.size() );

    data_syserr &ds = sd->syserr[fld->id];
    ds.defined = true;
    ds.vector = sd->obstype->datatype->isvector;
    const bool isangle = sd->obstype->datatype->isangle;

    double *inf = ds.influence;
    for( int i = ds.vector ? 3 : 1; i--; inf++)
    {
        ds.defined = ds.defined && read_double_field( sd->scanner(), *inf ) == FieldResult::Ok;
        if( isangle ) *inf *= STOR;
    }

    if( !ds.defined )
    {
        sd->error( MISSING_DATA,
                   "Influence of " + ldt_get_code( ID_SYSERR, 0, ds.syserr_id ) + " is missing or invalid" );
        ldt_cancel_data();
        return 0;
    }

    return 1;
}


static void load_data_syserrs( snapfile_def *sd )
{
    for( data_syserr &ds : sd->syserr )
    {
        if( !ds.defined ) continue;

        /* If the systematic error is classification dependent, get the
        value for the class */

        if( ds.class_id >= 0 )
        {
            const int class_name_id = sd->clsf[ds.class_id].data_id;
            if( class_name_id != ds.class_name_id )
            {
                ds.class_name_id = class_name_id;
                const std::string clsf_code = class_name_id
                    ? ldt_get_code( ID_CLASSNAME, sd->clsf[ds.class_id].class_id, class_name_id )
                    : "default";
                const std::string syserrname = ds.name + "/" + clsf_code;
                ds.syserr_id = numeric_cast<int>( ldt_get_id( ID_SYSERR, 0, syserrname ) );
            }
        }
        if( ds.vector )
        {
            ldt_vecsyserr( ds.syserr_id, ds.influence );
        }
        else
        {
            ldt_syserr( ds.syserr_id, ds.influence[0] );
        }
    }
}

/*=====================================================================*/
/* Handling of refraction coefficients, distance scale errors, and     */
/* bearing orientation errors                                          */

// #pragma warning(disable: 4100)

/* A command as it is written in a data file, for example "#projection" */

static std::string command_text( std::string_view cmd )
{
    return COMMAND_PREFIX + std::string( cmd );
}

static int read_proj_command( snapfile_def *sd, int, std::string_view cmd )
{
    std::string name;

    if( read_string_field( sd->scanner(), name, MAX_NAME_LEN ) != FieldResult::Ok )
    {
        sd->error( MISSING_DATA, "Name missing - use syntax " + command_text( cmd ) + " name" );
        return OK; // Already reported
    }
    sd->projctn = numeric_cast<int>( ldt_get_id( ID_PROJCTN, 0, name ) );
    return OK;
}

static int get_coef_class_id( snapfile_def *sd, int id )
{
    int classid = sd->coef_class_id[id];
    if( classid < 0 )
    {
        coef_class_info *cinfo = coef_class( id );
        classid = create_classification( sd, cinfo->default_classname );
        sd->coef_class_id[id] = classid;
    }
    return classid;
}

static int read_coef_command( snapfile_def *sd, int id, std::string_view cmd )
{
    std::string name;

    coef_class_info *cinfo = coef_class( id );
    const int classid = get_coef_class_id( sd, id );

    if( read_string_field( sd->scanner(), name, MAX_NAME_LEN ) != FieldResult::Ok )
    {
        sd->error( MISSING_DATA, "Name missing - use syntax " + command_text( cmd ) + " name" );
        return OK;
    }

    /* Set the classification for all data types that it applies to */
    for( int idtype = 0; idtype < NOBSTYPE; idtype++ )
    {
        datatypedef *dt = datatypedef_from_id( idtype );
        if( dt->*(cinfo->useclass) )
        {
            set_obstype_classification( sd, idtype, classid, name );
        }
    }

    return OK;  /* As possible errors are already reported */
}

static void load_projection( snapfile_def *sd )
{
    if( sd->useprojctn )
    {
        if( !sd->projctn )
        {
            sd->error( MISSING_DATA,
                       "Missing or invalid projection - check #projection in data file");
            ldt_cancel_inst();
            sd->definition_err = 1;
        }
        ldt_projection( sd->projctn );
    }
}


/*=====================================================================*/

// #pragma warning(disable: 4100)

static int read_error_command( snapfile_def *sd, int errtype, std::string_view cmd )
{
    double value[3];

    constexpr std::string_view ppm = "ppm";
    constexpr std::string_view mm  = "mm";
    constexpr std::string_view mmh = "mmh";
    constexpr std::string_view mmv = "mmv";
    constexpr std::string_view mmr = "mmr";
    constexpr std::string_view ppmr = "ppmr";
    constexpr std::string_view secs = "sec";
    int nerrvals;
    int goterr = 0;
    int status;

    struct
    {
        std::string_view code;
        double fact;
        double *value;
        int  found;
    } errcodes[3];
    int nerrcodes;

    int ic, iv;

    nerrvals = 1;
    nerrcodes = 1;
    errcodes[0].code = secs;
    errcodes[0].fact = STOR;
    errcodes[0].found = errcodes[1].found = errcodes[2].found = 0;

    switch( errtype )
    {
    case DS_ERR:  errcodes[0].code = mm;
        errcodes[0].fact = 0.001;
        errcodes[0].value = &sd->dserr;
        errcodes[1].code = ppm;
        errcodes[1].fact = 1.0e-6;
        errcodes[1].value = &sd->dsppmerr;
        nerrcodes = 2;
        break;

    case LV_ERR:  errcodes[0].value = &sd->lverr;
        errcodes[0].fact = 0.001;
        errcodes[0].code = mm;
        break;

    case HA_ERR:
        errcodes[0].value = &sd->haerr;
        errcodes[1].code = mm;
        errcodes[1].fact = 0.001;
        errcodes[1].value = &sd->hammerr;
        nerrcodes = 2;
        break;

    case AZ_ERR:
        errcodes[0].value = &sd->azerr;
        errcodes[1].code = mm;
        errcodes[1].fact = 0.001;
        errcodes[1].value = &sd->azmmerr;
        nerrcodes = 2;
        break;

    case ZD_ERR:
        errcodes[0].value = &sd->zderr;
        errcodes[1].code = mmh;
        errcodes[1].fact = 0.001;
        errcodes[1].value = &sd->zdmmherr;
        errcodes[2].code = mmv;
        errcodes[2].fact = 0.001;
        errcodes[2].value = &sd->zdmmverr;
        nerrcodes = 3;
        break;

    case LT_ERR:  errcodes[0].value = &sd->lterr; break;
    case LN_ERR:  errcodes[0].value = &sd->lnerr; break;

    case OH_ERR:  errcodes[0].value = &sd->oherr;
        errcodes[0].code = mm;
        errcodes[0].fact=0.001;
        break;

    case EH_ERR:  errcodes[0].value = &sd->eherr;
        errcodes[0].code = mm;
        errcodes[0].fact=0.001;
        break;

    case GB_ERR: errcodes[0].code = mm;
        errcodes[0].fact = 1.0; /* 0.001; */
        errcodes[0].value = sd->gpserr;
        errcodes[1].code = ppm;
        errcodes[1].fact = 1.0; /* 1.0e-6;*/
        errcodes[1].value = sd->gpserr+3;
        nerrcodes = 2;
        nerrvals = 3;
        break;

    case GX_ERR: errcodes[0].code = mm;
        errcodes[0].fact = 1.0; /* 0.001; */
        errcodes[0].value = sd->gpterr;
        errcodes[1].code = mmr;
        errcodes[1].fact = 1.0; /* 1.0e-6;*/
        errcodes[1].value = sd->gpterr+3;
        errcodes[2].code = ppmr;
        errcodes[2].fact = 1.0; /* 1.0e-6;*/
        errcodes[2].value = sd->gpterr+6;
        nerrcodes = 3;
        nerrvals = 3;
        break;


    default: handle_error(INTERNAL_ERROR,
                              "Invalid error type code passed to read_error_command",
                              "Occurred in module snapdata.c");
        return 0;
    }

    for( ic = 0; ic < nerrcodes; ic++ ) for( iv = 0; iv<nerrvals; iv++ )
        {
            errcodes[ic].value[iv] = 0.0;
        }

    goterr = 0;

    while( true )
    {
        FieldScanner &scanner = sd->scanner();
        std::string name;
        status = OK;
        if( scanner.atEnd() ) break;
        if( read_double_field( scanner, value[0] ) != FieldResult::Ok ) { status = INVALID_DATA; break; }
        for( iv = 1; iv < nerrvals; iv++ )
        {
            status = MISSING_DATA;
            if( read_double_field( scanner, value[iv] ) != FieldResult::Ok ) break;
            status = OK;
        }
        if( status != OK ) break;
        if( read_string_field( scanner, name, MAX_WORD_LEN ) != FieldResult::Ok ) {status = MISSING_DATA; break; }
        status = INVALID_DATA;
        for( ic = 0; ic < nerrcodes; ic++ )
        {
            if( !errcodes[ic].found && boost::algorithm::iequals( errcodes[ic].code, name ) )
            {
                errcodes[ic].found = 1;
                goterr = 1;
                for( iv = 0; iv < nerrvals; iv++ )
                {
                    errcodes[ic].value[iv] = value[iv] * errcodes[ic].fact;
                }
                status = OK;
                break;
            }
        }
        if( status != OK ) break;
    }

    if( status == OK && !goterr ) status = MISSING_DATA;

    if( status != OK )
    {
        std::string errmsg = std::string( status == MISSING_DATA ? "Missing" : "Invalid" ) +
                             " error definition - use syntax " + command_text( cmd );
        for( ic = 0; ic < nerrcodes; ic++ )
        {
            errmsg += ' ';
            for( iv = 0; iv < nerrvals; iv++ )
            {
                errmsg += "#.# ";
            }
            errmsg += errcodes[ic].code;
        }

        sd->error( status, errmsg );
    }

    else
    {
        sd->gotdflterr[errtype] = 1;
    }

    return OK; /* As any errors have already been reported */
}


static void report_missing_default_error( snapfile_def *sd, snap_data_type *obstype )
{
    std::string errmsg = std::string( "Error of " ) + datatype[obstype->type].name + " not defined";
    const auto errcmd = std::find_if( std::begin( commands ), std::end( commands ),
        [obstype]( const command &cmd ) { return cmd.action == read_error_command && cmd.id == obstype->errortype; } );
    if( errcmd != std::end( commands ) ) errmsg += " - use " + command_text( errcmd->command );
    sd->error( MISSING_DATA, errmsg );

    ldt_cancel_inst();
    sd->definition_err = 1;
}

static void load_default_error( snapfile_def *sd, snap_data_type *obstype, double value )
{
    int errtype;
    double error;

    errtype = obstype->errortype;
    if( !sd->gotdflterr[errtype] )
    {
        report_missing_default_error( sd, obstype );
        return;
    }

    switch( errtype )
    {

    case DS_ERR:
        error = _hypot( sd->dserr, value * sd->dsppmerr );
        ldt_error( &error );
        break;

    case HA_ERR:
        error = sd->haerr;
        if( error < 0.0 ) error = 0.0;
        if( sd->hammerr > 0 )
        {
            double mmerr, dist;
            mmerr = sd->hammerr;
            dist = ldt_calc_value( CALC_HDIST, sd->stn_id_inst, sd->stn_id_trgt );
            /* Ensure no div/0 error - mm component cannot be greater than 2 radians */

            if( dist < mmerr/2.0 ) dist = mmerr/2.0;
            error = hypot(error, mmerr/dist);
        }
        ldt_error( &error );
        break;

    case AZ_ERR:
        error = sd->azerr;
        if( error < 0.0 ) error = 0.0;
        if( sd->azmmerr > 0 )
        {
            double mmerr, dist;
            mmerr = sd->azmmerr;
            dist = ldt_calc_value( CALC_HDIST, sd->stn_id_inst, sd->stn_id_trgt );
            /* Ensure no div/0 error - mm component cannot be greater than 2 radians */

            if( dist < mmerr/2.0 ) dist = mmerr/2.0;
            error = hypot(error, mmerr/dist);
        }
        ldt_error( &error );
        break;

    case ZD_ERR:
        error = sd->zderr;
        if( error < 0.0 ) error = 0.0;
        if( sd->zdmmherr > 0 || sd->zdmmverr > 0)
        {
            double mmherr, mmverr, dist;
            mmherr = sd->zdmmherr;
            mmverr = sd->zdmmverr;
            if( mmherr < 0 ) mmherr = 0.0;
            if( mmverr < 0 ) mmverr = 0.0;

            dist = ldt_calc_value( CALC_DISTANCE, sd->stn_id_inst, sd->stn_id_trgt );
            /* Ensure no div/0 error - mm component cannot be greater than 2 radians */
            if( mmherr < dist/2.0 ) mmherr/=dist; else mmherr = 2;
            if( mmverr < dist/2.0 ) mmverr/=dist; else mmverr = 2;
            mmherr *= cos(value);
            mmverr *= sin(value);
            error = sqrt(error*error+mmherr*mmherr+mmverr*mmverr);
        }
        ldt_error( &error );
        break;

    case LT_ERR: ldt_error( &sd->lterr ); break;
    case LN_ERR: ldt_error( &sd->lnerr ); break;

    case LV_ERR: ldt_error( &sd->lverr ); break;

    case GB_ERR: break;
    case GX_ERR: break;

    case OH_ERR: ldt_error( &sd->oherr ); break;
    case EH_ERR: ldt_error( &sd->eherr ); break;

    default: handle_error( INTERNAL_ERROR,
                               "Invalid error type code passed to load_default_error",
                               "Occurred in module snapdata.c");
        break;
    }
}


/*===================================================================*/
/* Routines to define and read a data field element                  */

// #pragma warning(disable: 4100)

static int read_angle_type_command( snapfile_def *sd, int id, std::string_view )
{
    sd->dmsformat = id;
    return OK;
}

// #pragma warning(disable: 4100)

static int read_date_command( snapfile_def *sd, int, std::string_view cmd )
{
    std::string datestr;
    const bool ok = read_remaining_text( sd->scanner(), datestr, MAX_DATE_FIELD_LEN ) == FieldResult::Ok;
    boost::algorithm::to_upper( datestr );
    if( ! ok )
    {
        const std::string command = command_text( cmd );
        sd->error( INVALID_DATA,
                   "Missing date definition, use \"" + command + " unknown\" or eg " + command + " 5 MAY 1993" );
        return OK;
    }

    if( boost::algorithm::iequals( datestr, "unknown" ) )
    {
        sd->date = UNDEFINED_DATE;
        return OK;
    }

    const double date = snap_datetime_parse( datestr );

    if( date == 0.0 )
    {
        const std::string command = command_text( cmd );
        sd->error( INVALID_DATA,
                   "Invalid date definition, use \"" + command + " unknown\" or eg " + command + " 1993-05-13" );
    }
    else
    {
        sd->date=date;
    }

    return OK; /* As errors are handled */
}


static bool read_time( FieldScanner &scanner, double &obstime )
{
    std::string time;
    int hr = 0;
    int min = 0;

    if( read_string_field( scanner, time, MAX_WORD_LEN ) != FieldResult::Ok ) return false;

    if( (sscanf(time.c_str(),"%d:%d", &hr, &min ) < 2 &&
            sscanf(time.c_str(),"%d.%d", &hr, &min ) < 2 ) ||
            hr < 0 || hr > 24 || min < 0 || min > 59 ) return false;

    obstime = (hr + min/60.0)/24.0;
    return true;
}


static bool read_date( FieldScanner &scanner, double &obsdate )
{
    std::string datestr;

    if( read_string_field( scanner, datestr, MAX_DATE_FIELD_LEN ) != FieldResult::Ok ) return false;

    obsdate = snap_datetime_parse( datestr );
    return obsdate != 0;
}


// #pragma warning(disable: 4100)

static int read_time_command( snapfile_def *sd, int, std::string_view cmd )
{
    double obstime = 0.0;
    if( read_time( sd->scanner(), obstime ) )
    {
        if( sd->date != UNDEFINED_DATE ) sd->date = floor( sd->date) + obstime;
        ldt_date( sd->date );
    }
    else
    {
        sd->error( INVALID_DATA, "Invalid time - use syntax eg \"" + command_text( cmd ) + " 15:20\"" );
    }
    return OK; /* Since errors are already handled */
}

// #pragma warning(disable: 4100)

static int read_data_time( snapfile_def *sd, data_field * )
{
    double obstime = 0.0;
    if( read_time( sd->scanner(), obstime ) )
    {
        ldt_time( obstime );
    }
    else
    {
        sd->error( INVALID_DATA,
                   "Invalid time - use syntax eg \"15:20\"");
    }
    return OK; /* Since errors are already handled */
}

static int read_data_date( snapfile_def *sd, data_field * )
{
    double obsdate = 0.0;
    if( read_date( sd->scanner(), obsdate ) )
    {
        ldt_date( obsdate );
    }
    else
    {
        sd->error( INVALID_DATA,
                   "Invalid date - use syntax eg \"2012-03-25\"");
    }
    return OK; /* Since errors are already handled */
}

static void calc_nveccvr( snapfile_def *sd )
{
    sd->nveccvr = 3;
    if( !sd->grouped )
    {
        if( sd->cvrtype == CVR_FULL || sd->cvrtype == CVR_CORRELATION || sd->cvrtype == CVR_ENU_CORRELATION )
            sd->nveccvr = 6;
    }
    else if( sd->cvrtype == CVR_FULL )
    {
        sd->nveccvr = 0;
    }
    return;
}

// #pragma warning(disable: 4100)

static int read_gps_errtype_command( snapfile_def *sd, int, std::string_view )
{
    FieldScanner &scanner = sd->scanner();
    std::string option;
    bool ok = false;

    if( read_string_field( scanner, option, MAX_NAME_LEN ) == FieldResult::Ok )
    {
        ok = true;
        if( boost::algorithm::iequals( option, "diagonal" ) )  sd->dfltcvrtype = CVR_DIAGONAL;
        else if( boost::algorithm::iequals( option, "full" ) ) sd->dfltcvrtype = CVR_FULL;
        else if( boost::algorithm::iequals( option, "correlation" ) )
            sd->dfltcvrtype = CVR_CORRELATION;
        else if( boost::algorithm::iequals( option, "enu" ) )  sd->dfltcvrtype = CVR_TOPOCENTRIC;
        else ok = false;
    }
    sd->cvrupper = 0;
    if( read_string_field( scanner, option, MAX_NAME_LEN ) == FieldResult::Ok )
    {
        if( boost::algorithm::iequals( option, "upper" ) ) sd->cvrupper = 1; else ok = false;
    }

    if( !ok )
    {
        sd->error( INVALID_DATA,
                   "Invalid gps error type - must be \"diagonal\", \"full\", \"correlation\", or \"enu\"");
    }

    return OK; /* As error is already handled */
}

// #pragma warning(disable: 4100)

static int read_syserr_command( snapfile_def *sd, int, std::string_view cmd )
{
    FieldScanner &scanner = sd->scanner();
    std::string name;
    std::string classname;

    if( read_string_field( scanner, name, MAX_NAME_LEN ) != FieldResult::Ok )
    {
        sd->error( MISSING_DATA,
                   "Name missing - use " + command_text( cmd ) + " name [classification_name]" );
        return OK;
    }

    const int class_id = read_string_field( scanner, classname, MAX_NAME_LEN ) == FieldResult::Ok
        ? create_classification( sd, classname )
        : -1;

    create_syserr( sd, name, class_id );
    return OK;
}


// #pragma warning(disable: 4100)

static int read_classification( snapfile_def *sd, int, std::string_view cmd )
{
    std::string name;
    if( read_string_field( sd->scanner(), name, MAX_NAME_LEN ) != FieldResult::Ok )
    {
        sd->error( MISSING_DATA, "Name missing - use " + command_text( cmd ) + " name" );
    }
    else
    {
        create_classification( sd, name );
    }
    return OK;
}

// #pragma warning(disable: 4100)

static int read_classify_command( snapfile_def *sd, int, std::string_view cmd )
{
    FieldScanner &scanner = sd->scanner();
    std::string fields[3];
    size_t nfields = 0;

    while( nfields < 3 && read_string_field( scanner, fields[nfields], MAX_NAME_LEN ) == FieldResult::Ok )
    {
        nfields++;
    }

    if( nfields < 2 )
    {
        sd->error( MISSING_DATA,
                   "Missing information - use " + command_text( cmd ) + " [type/type...] class value" );
        return OK;
    }

    /* Without the observation types the classification applies to all of them */

    const bool has_types = nfields == 3;
    const std::string &clsf = fields[has_types ? 1 : 0];
    const std::string &value = fields[has_types ? 2 : 1];

    const int class_id = create_classification( sd, clsf );

    if( has_types )
    {
        std::string_view types( fields[0] );
        bool end = false;
        while( !end )
        {
            const size_t slash = types.find( '/' );
            end = slash == std::string_view::npos;
            const std::string_view code = types.substr( 0, slash );
            snap_data_type *obstype = obstype_from_code( code );
            if( ! obstype )
            {
                sd->error( INVALID_DATA, "Invalid observation type " + std::string( code ) );
            }
            else
            {
                set_obstype_classification( sd, obstype->type, class_id, value );
            }
            if( ! end ) types.remove_prefix( slash + 1 );
        }
    }

    else
    {
        set_obstype_classification( sd, -1, class_id, value );
    }
    return OK; /* All errors are already reported */
}

// #pragma warning(disable: 4100)

static int read_endset_command( snapfile_def *, int, std::string_view )
{
    return OK; /* Nothing to do - just marks end of the group */
}

// #pragma warning(disable: 4100)

static int read_note_command( snapfile_def *sd, int, std::string_view )
{
    if( sd->definition_err ) return OK;
    std::string note;
    read_remaining_text( sd->scanner(), note, MAX_NOTE_LEN );
    ldt_prefix_note( note );
    return OK;
}

/**********************************************************************/
/* Reading a data command -                                           */

static void next_data_field( snapfile_def *sd, int dftype, int id, int sec_id )
{
    sd->fields.push_back( { dftype, id, sec_id } );
}


// #pragma warning(disable: 4100)

static int read_data_command( snapfile_def *sd, int id, std::string_view cmd )
{
    int nobs = 0;
    int oneonly = 0;
    size_t startno = 0;
    snap_data_type *obstype = nullptr;
    std::string name;

    /* Set up the defaults */

    sd->noinststn = 0;
    sd->grouped = 0;
    sd->heights = 1;
    sd->definition_err = 0;
    sd->group_err = 0;
    sd->fields.clear();
    sd->endset = 0;
    sd->usereffrm = 0;
    sd->useprojctn = 0;

    /* Now read each item in turn.   The valid items are
    type_code    Starts a group of fields relating to an observation
    "value"      Reads the value relating to the observation
             If not present, the value is assumed to be the
             first item.
    "error"      Defines the error of the observation.  If not
             present then the default error for the type
             of observation is used
    "distance_scale_error"   Reads the name of the sf
    "refraction_coefficient"  Reads the name of the rc
    "bearing_orientation_error"  Reads the name of the boe
    classification_type   Reads the value for the classification
    systematic_error_type Reads the influence for the se.

    grouped      Defines that the observations are in grouped
             format (forced for some data types)
    no_heights   Specifies that instrument heights are not defined
    time         Defines the time of the observation eg 12.30 or 12:30

    */

    while( read_string_field( sd->scanner(), name, MAX_COMMAND_LEN ) == FieldResult::Ok )
    {

        /* Commands which can precede an observation type */

        if( boost::algorithm::iequals( name, "grouped" ) ) { sd->grouped = 1; continue; }
        if( boost::algorithm::iequals( name, "no_heights" ) ) { sd->heights = 0; continue; }
        if( boost::algorithm::iequals( name, "time" ) ) { next_data_field( sd, DFT_TIME, 0, 0 ); continue; }
        if( boost::algorithm::iequals( name, "date" ) ) { next_data_field( sd, DFT_DATE, 0, 0 ); continue; }

        /* An observation type */

        obstype = obstype_from_code( name );

        if( ! obstype && nobs == 0 )
        {
            sd->definition_err = 1;
            break;
        }

        else if ( obstype )
        {
            snap_data_type *st = obstype;
            datatypedef *dt = datatype + st->type;
            if( st->datatype->reffrm ) sd->usereffrm = 1;
            if (st->datatype->projctn) sd->useprojctn = 1;
            if( nobs && st->obsclass != sd->obsclass )
            {
                sd->error( INCONSISTENT_DATA,
                           std::string( datatype[sd->fields[0].id].name ) + " obs not compatible with " + dt->name );
                sd->definition_err = 1;
                break;
            }

            sd->obsclass = st->obsclass;
            /* Point vectors are defined using a target station and no inst station,
               so they can be grouped!? */
            sd->noinststn = dt->isvector && dt->ispoint;
            sd->obstype = obstype;
            startno = sd->fields.size();
            next_data_field( sd, DFT_START, obstype->type,
                             FLG_DFLT_DATA | FLG_DFLT_ERROR );
            nobs++;
            if( st->datatype->needsgroup ) sd->grouped = 1;
            if( st->oneonly ) oneonly = obstype->type+1;
            continue;
        }

        if( boost::algorithm::iequals( name, "value" ) )
        {
            if( !(sd->fields[startno].sec_id & FLG_DFLT_DATA) )
            {
                sd->definition_err = 1;
                break;
            }
            next_data_field( sd, DFT_DATA, sd->obstype->type, 0 );
            sd->fields[startno].sec_id &= ~ FLG_DFLT_DATA;
            continue;
        }

        if( boost::algorithm::iequals( name, "error" ) )
        {
            if( !(sd->fields[startno].sec_id & FLG_DFLT_ERROR) )
            {
                sd->definition_err = 1;
                break;
            }
            next_data_field( sd, DFT_ERROR, sd->obstype->type, 0 );
            sd->fields[startno].sec_id &= ~ FLG_DFLT_ERROR;
            continue;
        }

        if( boost::algorithm::iequals( name, "distance_scale_error" ) )
        {
            next_data_field( sd, DFT_CLASS, get_coef_class_id(sd,COEF_CLASS_DISTSF), 0 );
            continue;
        }

        if( boost::algorithm::iequals( name, "refraction_coefficient" ) )
        {
            next_data_field( sd, DFT_CLASS, get_coef_class_id(sd,COEF_CLASS_REFCOEF), 0 );
            continue;
        }

        if( boost::algorithm::iequals( name, "bearing_orientation_error" ) )
        {
            next_data_field( sd, DFT_CLASS, get_coef_class_id(sd,COEF_CLASS_BRNGREF), 0 );
            continue;
        }

        if( boost::algorithm::iequals( name, "id" ) )
        {
            next_data_field( sd, DFT_OBSID, 0, 0 );
            continue;
        }

        id = get_classification( sd, name );
        if( id >= 0 )
        {
            next_data_field( sd, DFT_CLASS, id, 0 );
            continue;
        }

        id = get_syserr( sd, name );
        if( id >= 0 )
        {
            next_data_field( sd, DFT_SYSERR, id, 0 );
            continue;
        }

        /* If its not one of these, then it must be an error */

        sd->definition_err = 1;
        break;
    }


    if( sd->definition_err )
    {
        sd->error( INVALID_DATA,
                   "Field \"" + name + "\" invalid or out of place in " + command_text( cmd ) );
    }

    else if( sd->usereffrm && sd->useprojctn )
    {
        sd->error( INVALID_DATA,
                   "Cannot mix data using reference frames and projections");
        sd->definition_err = 1;
    }

    else if( nobs < 1 )
    {
        sd->error( MISSING_DATA, "No data is specified in " + command_text( cmd ) );
        sd->definition_err = 1;
    }

    else if( nobs > 1 && oneonly )
    {
        sd->error( INCONSISTENT_DATA,
                   std::string( "Cannot combine " ) + datatype[oneonly-1].name + " with other observations" );
        sd->definition_err = 1;
    }

    /* Grouped format is not possible with point data. */

    if( sd->obsclass == SD_PNTDATA ) sd->grouped = 0;
    calc_nveccvr( sd );

    /* If using multistation vector data then may need explicit
       definition of end of set */

    if( sd->grouped && sd->obsclass == SD_VECDATA &&
            ( sd->dfltcvrtype == CVR_CORRELATION ||
              sd->dfltcvrtype == CVR_FULL ) )
        sd->endset = 1;

    return OK;  /* As all errors are already handled */
}


static void setup_cvr_rows( snapfile_def *sd )
{
    const int nc = sd->nvecobs*3;
    int i, i3;
    if( nc > numeric_cast<int>( sd->cvrrow.size() ) )
    {
        sd->cvrrow.resize( nc+30 );
    }
    reset_list_pointer( sd->vecerrlst );
    int *row = sd->cvrrow.data();
    for( i = sd->nvecobs, i3 = 0; i--; i3 += 3 )
    {
        vecerr_def *ve = (vecerr_def *) next_list_item( sd->vecerrlst );
        if( !ve || ve->nvecobs < 0 )
        {
            row[i3] = row[i3+1] = row[i3+2] = -1;
        }
        else
        {
            int r = (ve->nvecobs - 1)*3;
            row[i3] = r; row[i3+1] = r+1; row[i3+2] = r+2;
        }
    }
}

static int read_vector_covariance( snapfile_def *sd, int data_available )
{
    ltmat cvr;
    double val = 0.0;
    int i, i3, j3, ok, cvrtype;
    int grouped;
    int cvrused;
    int errtype;

    /* First get a pointer to the covariance matrix */

    cvrtype = sd->cvrtype;
    grouped = sd->grouped;
    errtype = sd->obstype->errortype;

    if( cvrtype == CVR_DEFAULT )
    {
        if( !sd->gotdflterr[errtype] )
        {
            report_missing_default_error( sd, sd->obstype );
            return 0;
        }
    }

    cvr = ldt_covariance( cvrtype, errtype == GB_ERR ? sd->gpserr : sd->gpterr );
    cvrused = cvr ? 1 : 0;

    /* If we are using a default covariance, we don't need to do anything else */

    if( cvrtype == CVR_DEFAULT ) return 1;

    /* Copy the information we have already read into the covariance matrix */

    if( cvrused && sd->vecerrlst && sd->nveccvr )
    {

        reset_list_pointer( sd->vecerrlst );

        for( i = sd->nvecobs, i3 = 0; i--; i3 += 3 )
        {
            vecerr_def *vecerr;
            double *vcvr;
            vecerr = (vecerr_def *) next_list_item( sd->vecerrlst );
            if( !vecerr->nvecobs ) continue;
            vcvr = vecerr->vecerr;
            i3 = (vecerr->nvecobs - 1)*3;
            if( cvrtype != CVR_FULL )
            {
                Lij(cvr,i3,i3) = vcvr[0];
                Lij(cvr,i3+1,i3+1) = vcvr[1];
                Lij(cvr,i3+2,i3+2) = vcvr[2];
                if( cvrtype == CVR_CORRELATION && !grouped )
                {
                    Lij(cvr,i3,i3+1) = vcvr[3];
                    Lij(cvr,i3,i3+2) = vcvr[4];
                    Lij(cvr,i3+1,i3+2) = vcvr[5];
                }
            }
            else if( !grouped )
            {
                for( i3 = 0; i3 < 6; i3++ ) cvr[i3] = vcvr[i3];
            }
        }
    }

    /* Finally read any remaining covariance or correlation values */

    ok = 1;
    setup_cvr_rows( sd );
    if( grouped && ( cvrtype == CVR_CORRELATION || cvrtype== CVR_FULL ))
    {
        const std::string matrix = cvrtype == CVR_CORRELATION ? "correlation" : "covariance";
        if( !data_available )
        {
            sd->error( MISSING_DATA, "The " + matrix + " matrix is missing" );
            ldt_cancel_inst();
            ok = 0;
        }
        else
        {
            const int *row = sd->cvrrow.data();
            int nvecrow = sd->nvecobs*3;

            for( i3 = 0; i3 < nvecrow; i3++ )
            {
                int ri = row[i3];
                int j3min, j3max;
                if( !cvrused ) ri = -1;
                if( sd->cvrupper )
                {
                    if( cvrtype == CVR_FULL ) { j3min = i3; j3max = nvecrow-1; }
                    else { j3min = i3+1; j3max = nvecrow-1; }
                }
                else
                {
                    if( cvrtype == CVR_FULL ) { j3min = 0; j3max = i3; }
                    else { j3min = 0; j3max = i3-1; }
                }


                for( j3 = j3min; j3 <= j3max; j3++ )
                {
                    int rj = row[j3];

                    while( sd->scanner().atEnd() )
                    {
                        if( sd->df->read_record() != OK ) break;
                    }
                    ok = read_double_field( sd->scanner(), val ) == FieldResult::Ok;
                    if( ri >= 0 && rj >= 0 ) Lij(cvr,ri,rj) = val;
                    if( !ok )
                    {
                        sd->error( INVALID_DATA, "The " + matrix + " matrix is not correctly specified" );
                        ldt_cancel_inst();
                        break;
                    }
                }
            }
        }
    }
    return ok;
}

static void init_read_vector_error( snapfile_def *sd )
{
    sd->nvecobs = 0;
    sd->nvecgood = 0;
    sd->currvecerr = NULL;
}


static void start_vector_error( snapfile_def *sd )
{

    sd->nvecobs++;

    if( !sd->vecerrlst )
    {
        sd->vecerrlst = create_list( sizeof( vecerr_def ) );
    }

    if( sd->nvecobs == 1 ) reset_list_pointer( sd->vecerrlst );

    sd->currvecerr = (vecerr_def *) next_list_item( sd->vecerrlst );
    if( !sd->currvecerr )
        sd->currvecerr = (vecerr_def *) add_to_list( sd->vecerrlst, NEW_ITEM );
    sd->currvecerr->nvecobs = 0;  /* Only set if observation successfully read */
}

static int read_vector_error( snapfile_def *sd )
{
    double *cvr = sd->currvecerr->vecerr;

    for( int i = sd->nveccvr; i--; )
    {
        if( read_double_field( sd->scanner(), *cvr++ ) != FieldResult::Ok )
        {
            sd->error( INVALID_DATA,
                       "The errors for the vector components are not correctly specified");
            ldt_cancel_data();
            return 0;
        }
    }
    if( sd->cvrtype == CVR_FULL && sd->nveccvr == 6 && sd->cvrupper )
    {
        cvr = sd->currvecerr->vecerr;
        std::swap( cvr[2], cvr[3] );
    }


    return 1;
}


/* Flag a vector error as being used in the final covariance matrix */

static void validate_vector_error( snapfile_def *sd )
{
    if( sd->currvecerr ) sd->currvecerr->nvecobs = ++sd->nvecgood;
}




static int read_data_error( snapfile_def *sd, data_field * )
{
    snap_data_type *st = sd->obstype;
    double error = 0.0;

    if( st->datatype->isvector )
    {
        read_vector_error( sd );
    }
    else
    {
        if( read_double_field( sd->scanner(), error ) != FieldResult::Ok )
        {
            sd->error( INVALID_DATA,
                       std::string( "Invalid or missing error for " ) + datatype[st->type].name );
            ldt_cancel_data();
            return 0;
        }
        if( st->datatype->isangle )
        {
            error *= (sd->dmsformat == AF_DEG) ? DTOR : STOR;
        }
        ldt_error( &error );
    }
    return 1;
}


/* Reads a hemisphere letter and negates value if it is the second of the two letters in sign */

static bool read_sign( FieldScanner &scanner, std::string_view sign, double &value )
{
    std::string field;

    if( read_string_field( scanner, field, 1 ) != FieldResult::Ok ) return false;
    boost::algorithm::to_upper( field );
    if( field[0] == sign[0] ) return true;
    if( field[0] == sign[1] ) { value = -value; return true; }
    return false;
}

static int read_data_data( snapfile_def *sd, data_field *fld )
{
    double value[3] = { 0.0, 0.0, 0.0 };
    FieldScanner &scanner = sd->scanner();

    /* Check whether the observation is to be rejected */

    const bool unused = scanner.skipIfNext( REJECT_CHAR ) || sd->rejobs;

    snap_data_type *st = sd->obstype;

    bool sts = true;
    if( st->datatype->isangle )
    {
        switch( sd->dmsformat )
        {
        case AF_HP:  sts = read_hp_angle_field( scanner, value[0] ) == FieldResult::Ok; break;
        case AF_DMS: sts = read_dms_angle_field( scanner, value[0] ) == FieldResult::Ok; break;
        case AF_DEG: sts = read_degree_angle_field( scanner, value[0] ) == FieldResult::Ok; break;
        default: handle_error(INTERNAL_ERROR,"Invalid angle format",__FILE__ " read_data_data" ); break;
        }
        if( sd->dmsformat != AF_DEG )
        {
            if( sts && st->type == LT ) sts = read_sign( scanner, "NS", value[0] );
            if( sts && st->type == LN ) sts = read_sign( scanner, "EW", value[0] );
        }
    }
    else if ( st->datatype->isvector )
    {
        sts = read_double_field( scanner, value[0] ) == FieldResult::Ok &&
              read_double_field( scanner, value[1] ) == FieldResult::Ok &&
              read_double_field( scanner, value[2] ) == FieldResult::Ok;
    }
    else
    {
        sts = read_double_field( scanner, value[0] ) == FieldResult::Ok;
    }

    if( !sts )
    {
        sd->error( MISSING_DATA, std::string( datatype[sd->obstype->type].name ) + " is missing" );
        ldt_cancel_data();
    }

    else
    {
        ldt_value( value );
    }

    if( unused ) ldt_unused();

    sd->value = value[0];  /* Only of interest for scalar data */

    /* Is the observation followed by an explicit error over-riding
       the default value. */

    if( sts && !sd->goterr )
    {
        const FieldScanner unread = scanner;
        std::string errstr;
        if( read_string_field( scanner, errstr, MAX_WORD_LEN ) == FieldResult::Ok )
        {
            if( boost::algorithm::iequals( errstr, ERROR1 ) || boost::algorithm::iequals( errstr, ERROR2 ) )
            {

                if( sd->obsclass == SD_VECDATA )
                {
                    sd->cvrtype = CVR_TOPOCENTRIC;  /* sd->dfltcvrtype; */
                    calc_nveccvr( sd );
                }

                sts = read_data_error( sd, fld );
                sd->goterr = 1;
            }
            else
            {
                scanner = unread;
            }
        }
    }

    return sts;
}


static void start_group( snapfile_def *sd )
{
    init_read_vector_error( sd );
    sd->ingroup = 1;
    sd->rejgroup = 0;
    if( sd->obsclass == SD_VECDATA )
    {
        sd->cvrtype = sd->dfltcvrtype;
        calc_nveccvr( sd );
    }
}

static void end_group( snapfile_def *sd, int at_endset )
{
    if( sd->obsclass == SD_VECDATA )
    {
        read_vector_covariance( sd, at_endset );
    }
    ldt_end_data();
    sd->ingroup = 0;
}

static int start_obs( snapfile_def *sd, data_field *fld )
{
    FieldScanner &scanner = sd->scanner();
    const FieldScanner unread = scanner;
    std::string skip;

    if( read_string_field( scanner, skip, 2 ) == FieldResult::Ok && skip.size() == 1 && skip[0] == SKIPOBS_CHAR )
    {
        sd->skipobs = 1;
        return OK;
    }
    scanner = unread;
    sd->skipobs = 0;

    ldt_nextdata( fld->id );

    sd->obstype = snap_data_type_from_id(fld->id);
    init_data_classifications( sd );
    init_data_syserrs( sd );

    int sts = OK;
    sd->goterr = fld->sec_id & FLG_DFLT_ERROR ? 0 : 1;

    /* Cannot override default errors for multistation GPS data */
    /* Set covariance type to CVR_DEFAULT if using predefined errors */
    /* Note: This is messy - the covariance type should really be defined
       in start_group, not start_obs */

    if( sd->obsclass == SD_VECDATA )
    {
        if( !sd->goterr ) { sd->cvrtype = CVR_DEFAULT; sd->nveccvr = 0; }
        if( sd->grouped ) sd->goterr = 1;
        start_vector_error( sd );
    }

    if( fld->sec_id & FLG_DFLT_DATA )
    {
        sts = read_data_data( sd, fld );
    }

    sd->inobs = 1;
    return sts;
}

static void end_obs( snapfile_def *sd )
{
    load_data_classifications( sd );
    load_data_syserrs( sd );
    if( !sd->goterr )
    {
        load_default_error( sd, sd->obstype, sd->value );
    }
    if( sd->obsclass == SD_VECDATA ) validate_vector_error( sd );
    sd->inobs = 0;
}


/* read_station returns OK, INVALID_DATA (missing or illformatted), or
   WARNING_ERROR (station not listed) */

static int read_station( snapfile_def *sd, int &stn_id, double &hgt )
{
    std::string name;
    const std::string station_role = sd->ingroup ? "target" : "instrument";

    /* Read the station code and the optional height */

    stn_id = 0;
    hgt = 0.0;

    if( read_string_field( sd->scanner(), name, MAX_NAME_LEN ) != FieldResult::Ok )
    {
        sd->error( INVALID_DATA, "Invalid or missing " + station_role + " station code" );
        return INVALID_DATA;
    }

    stn_id = numeric_cast<int>( ldt_get_id( ID_STATION, 0, name ) );
    if( stn_id == 0 )
    {
        sd->error( INVALID_DATA, "Invalid " + station_role + " station code \"" + name + "\"" );
    }

    if( sd->heights )
    {
        if( read_double_field( sd->scanner(), hgt ) != FieldResult::Ok )
        {
            sd->error( INVALID_DATA, "Invalid or missing " + station_role + " height" );
            if( !sd->ingroup ) sd->group_err = 1;
            return INVALID_DATA;
        }
    }

    return stn_id <= 0 ? WARNING_ERROR : OK;
}


static int read_data_line( snapfile_def *sd, bool rej )
{
    int stn_id;
    double ihgt;

    if( sd->fields.empty() )
    {
        sd->error( MISSING_DATA, "Definition of data format missing - use " + command_text( "data" ) );
        sd->definition_err = 1;
        return MISSING_DATA;
    }

    ldt_lineno( sd->df->line_number() );

    int sts = read_station( sd, stn_id, ihgt );

    /* For grouped data which does not require an end of set test whether
       this is the end of the line.  If it is then automatically start
       a new group. */

    bool startgrp = !sd->grouped ||
                    !sd->ingroup ||
                    (!sd->endset && sd->scanner().atEnd());

    if( startgrp )
    {
        if( sd->ingroup ) end_group(sd, 0);
        start_group( sd );

        sd->stn_id_inst = -1;
        if( sd->noinststn )
        {
            startgrp = false;
            ldt_inststn( 0, 0.0 );
        }
        else
        {
            ldt_inststn( stn_id, ihgt );
            sd->stn_id_inst = stn_id;
            sd->rejgroup = rej;
        }
        ldt_date( sd->date );
        load_projection( sd );

        if( sts != OK )
        {
            sd->group_err = 1;
            ldt_cancel_inst();
            if( sts != WARNING_ERROR ) return sts;
        }
    }

    sd->rejobs = sd->rejgroup || rej;

    /* If not grouped or point data then read the target station data,
       If there is a target station then load the data... */

    if( !sd->grouped && sd->obsclass != SD_PNTDATA )
    {
        if( ! sd->noinststn) sts = read_station( sd, stn_id, ihgt );
        ldt_tgtstn( stn_id, ihgt );
        sd->stn_id_trgt = stn_id;
    }
    else if( !startgrp )
    {
        ldt_tgtstn( stn_id, ihgt );
        sd->stn_id_trgt = stn_id;
    }

    if( sts != OK )
    {
        sd->group_err = 1;
        ldt_cancel_trgt();
        if( sts != WARNING_ERROR ) return sts;
    }

    /* If either within group (ie not just starting), or not in grouped
       data, then process the data fields

    */

    sd->skipobs = 0;
    sd->inobs = 0;

    if( !sd->grouped || !startgrp )
    {
        for( data_field &fld : sd->fields )
        {
            if( sd->skipobs && fld.type != DFT_START ) continue;
            switch( fld.type )
            {
            case DFT_START:  if( sd->inobs ) end_obs( sd );
                start_obs( sd, &fld );
                break;
            case DFT_DATA:   read_data_data( sd, &fld ); break;
            case DFT_ERROR:  read_data_error( sd, &fld ); break;
            case DFT_DATE:   read_data_date( sd, &fld ); break;
            case DFT_TIME:   read_data_time( sd, &fld ); break;
            case DFT_OBSID:  read_data_obs_id( sd ); break;
            case DFT_CLASS:  read_data_classification( sd, &fld ); break;
            case DFT_SYSERR: read_data_syserr( sd, &fld ); break;
            default: assert(0);
            }
        }
    }

    if( sd->inobs ) end_obs( sd );

    /* Check that there is no spurious data on the line */

    if( !sd->scanner().atEnd() )
    {
        if( startgrp )
        {
            sd->error( TOO_MUCH_DATA, "Extra data in grouped data instrument station line");
        }
        else
        {
            sd->error( TOO_MUCH_DATA, "Extra data in data file");
        }
    }
    return OK;
}

static void process_command( snapfile_def *sd )
{
    std::string cmdname;

    read_string_field( sd->scanner(), cmdname, MAX_COMMAND_LEN );

    const auto cmd = std::find_if( std::begin( commands ), std::end( commands ),
        [&cmdname]( const command &known ) { return boost::algorithm::iequals( known.command, cmdname ); } );

    if( cmd == std::end( commands ) )
    {
        sd->error( INVALID_DATA, "Invalid data definition command \"#" + cmdname.substr( 0, 30 ) + "\"" );
        return;
    }

    if( cmd->flags & CMD_ENDDATA && sd->ingroup )
    {
        end_group( sd, cmd->flags & CMD_ENDSET ? 1 : 0 );
    }

    if( cmd->action ) (*cmd->action)(sd, cmd->id, cmd->command );

}


int read_snap_data( DATAFILE *df, int (*check_progress)( DATAFILE *df ) )
{
    df->read_record();  /* Skip over the header line */

    snapfile_def sd( df );

    int sts = OK;
    while( df->read_record() == OK )
    {
        if( check_progress && !(*check_progress)(df) )
        {
            sts = OPERATION_ABORTED;
            break;
        }
        FieldScanner &scanner = df->input_string().scanner;
        if( scanner.atEnd() ) continue;
        if( scanner.skipIfNext( COMMAND_PREFIX ) )
        {
            process_command( &sd );
        }
        else if( !sd.definition_err )
        {
            const bool rej = scanner.skipIfNext( REJECT_CHAR );
            read_data_line( &sd, rej );
        }
    }
    if( sd.ingroup ) end_group( &sd, 0 );
    return sts;
}
