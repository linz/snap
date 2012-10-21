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
#include <ctype.h>

#ifndef DEBUG
#define NDEBUG
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
#define COMLEN 80
#define UNITLEN 10
#define NOTELEN 80
#define NAMELEN 20


#define COMMAND_PREFIX '#'
#define REJECT_CHAR    '*'
#define SKIPOBS_CHAR   '-'

#define ERROR1         "error"
#define ERROR2         "+/-"

#define ISEQ(s1,s2) (_stricmp(s1,s2)==0)

static char rcsid[]="$Id: snapdata.c,v 1.4 2004/04/22 02:35:15 ccrook Exp $";

/* Definition of a data field in the file */

typedef struct
{
    int type;   /* Data type */
    int id;     /* Data type id */
    int sec_id; /* Secondary id - eg for classified systematic errors */
} data_field;

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

typedef struct
{
    char *name;
    int class_id;
    int id[NOBSTYPE];
    int data_id;        /* Id for the current data field group */
} data_class;

typedef struct
{
    char *name;
    int syserr_id;
    int class_id;
    int class_name_id;  /* Cache the last value */
    int name_len;
    char defined;         /* Defined for current data field group */
    char vector;          /* True if the current systematic error is a vector */
    double influence[3];  /* Influence for current data field */
} data_syserr;

/* Structure for holding a vector error specification */

typedef struct
{
    int nvecobs;
    double vecerr[MAXVECERR];
} vecerr_def;

/* Structure holding the current state of the file */

typedef struct
{
    DATAFILE *df;

    int nfield;         /* Definitions of data fields */
    int maxfield;
    data_field *fields;

    int nclass;         /* Definitions of classifications */
    int maxclass;
    data_class *clsf;

    int nsyserr;        /* Definitions of systematic errors */
    int maxsyserr;
    data_syserr *syserr;

    int coef_class_id[N_COEF_CLASSES];

    double dserr, dsppmerr;    /* Definitions of errors */
    double haerr, hammerr;
    double azerr, azmmerr;
    double zderr, zdmmherr, zdmmverr;
    double lverr;
    double lnerr, lterr;
    double oherr;
    double eherr;
    double gpserr[9], gpterr[9];

    int gotdflterr[NERRTYPE];

    /* Definitions of coefficients */
    int dmsformat;
    int refcoef;      /* Default values - 0 = none */
    int distsf;
    int brngref;

    int refframe;     /* Definition of the reference frame */
    int projctn;      /* Projection id */
    int usereffrm;    /* True if current data uses reffrm/projection */
    int useprojctn;

    int obs_refcoef;  /* As applying to the current observation */
    int obs_distsf;
    int obs_brngref;


    double date;        /* Miscellaneous data */
    double value;       /* Used for default error of scalar data */
    snap_data_type *obstype;      /* Current observation type */
    int stn_id_inst;
    int stn_id_trgt;
    int obsclass;
    int rejobs;
    int skipobs;
    int inobs;
    int noinststn;
    int grouped;
    int ingroup;
    int rejgroup;
    int goterr;
    int endset;       /* Requires definition of end of group */
    int heights;
    int nveccvr;
    int cvrupper;
    int nvecobs;
    int nvecgood;
    void  *vecerrlst;   /* Link list used to hold vector errors as they are read */
    int *cvrrow;
    int maxcvrrow;
    vecerr_def *currvecerr;
    int dfltcvrtype;
    int cvrtype;

    int definition_err;    /* Error status */
    int group_err;

} snapfile_def;


/* Structure used to define valid specification commands within the data file */

static int read_error_command( snapfile_def *sd, int id, char *cmd );
static int read_angle_type_command( snapfile_def *sd, int id, char *cmd );
static int read_date_command( snapfile_def *sd, int id, char *cmd );
static int read_time_command( snapfile_def *sd, int id, char *cmd );
static int read_proj_command( snapfile_def *sd, int id, char *cmd );
static int read_coef_command( snapfile_def *sd, int id, char *cmd );
static int read_gps_errtype_command( snapfile_def *sd, int id, char *cmd );
static int read_syserr_command( snapfile_def *sd, int id, char *cmd );
static int read_classification( snapfile_def *sd, int id, char *cmd );
static int read_classify_command( snapfile_def *sd, int id, char *cmd );
static int read_endset_command( snapfile_def *sd, int id, char *cmd );
static int read_note_command( snapfile_def *sd, int id, char *cmd );
static int read_data_command( snapfile_def *sd, int id, char *cmd );
static int read_endset_command( snapfile_def *sd, int id, char *cmd );

typedef struct
{
    char *command;
    int id;
    int (*action)( snapfile_def *sd, int id, char *cmd );
    int flags;
} command;

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
    {"data",0,read_data_command,CMD_ENDDATA},
    {NULL, 0, NULL, 0}
};

/* Initiallize the snapfile_def structure */

static void init_snapfile_def( snapfile_def *sd, DATAFILE *df )
{
    int i;
    sd->df = df;
    sd->nfield = 0;
    sd->maxfield = 0;
    sd->fields = NULL;
    sd->nclass = 0;
    sd->maxclass = 0;
    sd->clsf = NULL;
    sd->nsyserr = 0;
    sd->maxsyserr = 0;
    sd->syserr = NULL;
    sd->dserr = -1;
    sd->dsppmerr = -1;
    sd->haerr = -1;
    sd->hammerr = -1;
    sd->azerr = -1;
    sd->azmmerr = -1;
    sd->zderr = -1;
    sd->zdmmherr = -1;
    sd->zdmmverr = -1;
    sd->lverr = -1;
    sd->lnerr = -1;
    sd->lterr = -1;
    sd->oherr = -1;
    sd->eherr = -1;
    for( i = 0; i < 9; i++ )
    {
        sd->gpserr[i] = 0.0;
        sd->gpterr[i] = 0.0;
    }
    sd->gpserr[0] = -1;
    sd->gpterr[0] = -1;
    for( i = 0; i < NERRTYPE; i++ )
    {
        sd->gotdflterr[i] = 0;
    }
    for( i = 0; i < N_COEF_CLASSES; i++ )
    {
        sd->coef_class_id[i] = -1;
    }
    sd->nveccvr = 0;
    sd->cvrupper = 0;
    sd->dfltcvrtype = CVR_FULL;
    sd->cvrtype = 0;
    sd->date = UNKNOWN_DATE;
    sd->noinststn = 0;
    sd->grouped = 0;
    sd->ingroup = 0;
    sd->heights = 0;
    sd->dmsformat = AF_DMS;
    sd->refcoef = 0;
    sd->distsf = 0;
    sd->brngref = 0;
    sd->refframe = 0;
    sd->usereffrm = 0;
    sd->projctn = 0;
    sd->useprojctn = 0;
    sd->definition_err = 0;
    sd->group_err = 0;
    sd->vecerrlst = NULL;
    sd->currvecerr = NULL;
    sd->maxcvrrow = 0;
    sd->cvrrow = NULL;
};

static void term_snapfile_def( snapfile_def *sd )
{
    int i;
    check_free( sd->fields );
    for( i = 0; i < sd->nclass; i++ ) check_free( sd->clsf[i].name );
    if( sd->clsf ) check_free( sd->clsf );
    for( i = 0; i < sd->nsyserr; i++ ) check_free( sd->syserr[i].name );
    if( sd->syserr ) check_free( sd->syserr );
    if( sd->vecerrlst ) free_list( sd->vecerrlst, NO_ACTION );
    if( sd->cvrrow ) check_free( sd->cvrrow );
    init_snapfile_def( sd, sd->df );
}

/*===============================================================*/
/* Get observation snap type from its code                       */

static snap_data_type *obstype_from_code( char *code )
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

static int get_classification( snapfile_def *sd, const char *cclass )
{
    int i;
    for( i = 0; i<sd->nclass; i++ )
    {
        if( _stricmp( cclass, sd->clsf[i].name ) == 0 ) return i;
    }
    return -1;
}

static int create_classification( snapfile_def *sd, const char *cclass )
{
    int ic, ot;
    data_class *cd;
    ic = get_classification( sd, cclass );
    if( ic >= 0 ) return ic;

    if( sd->nclass >= sd->maxclass )
    {
        sd->maxclass = sd->nclass + 5;
        sd->clsf = (data_class *)
                   check_realloc( sd->clsf, sd->maxclass*sizeof(data_class));
    }
    ic = sd->nclass;
    sd->nclass ++;
    cd = sd->clsf + ic;

    cd->name = copy_string( cclass );
    cd->class_id = (int) ldt_get_id( ID_CLASSTYPE, 0, cclass );
    for( ot = 0; ot < NOBSTYPE; ot++ ) cd->id[ot] = 0;

    return ic;
}

static void set_obstype_classification( snapfile_def *sd, int type,
                                        int clsf_id, char *name )
{

    data_class *cd;
    int name_id = 0;

    cd = sd->clsf + clsf_id;
    if( _stricmp(name,"none") != 0 )
    {
        name_id = ldt_get_id( ID_CLASSNAME, cd->class_id, name );
    }
    if( type >= 0 )
    {
        cd->id[type] = name_id;
    }
    else
    {
        int i;
        for( i=0; i < NOBSTYPE; i++ )
        {
            cd->id[i] = name_id;
        }
    }
}


/* Loading classifications for an observation.  First call
   init_data_classifications for the type, then read_data_classification
   for each DFT_CLASS field for the observation,
   then load_data_classifications when done */

static void init_data_classifications( snapfile_def *sd )
{
    int i;
    for( i = 0; i < sd->nclass; i++ )
    {
        sd->clsf[i].data_id = sd->clsf[i].id[sd->obstype->type];
    }
}

static int read_data_obs_id( snapfile_def *sd )
{
    int id;
    if( df_read_int( sd->df, &id ) )
    {
        ldt_obs_id( id );
    }
    else
    {
        df_data_file_error( sd->df, INVALID_DATA,
                            "Observation id is missing or invalid - must be an integer number");
    }

    return OK;
}

static int read_data_classification( snapfile_def *sd, data_field *fld )
{
    char name[NAMELEN];
    int name_id = 0;
    data_class *cd;

    if( !df_read_field( sd->df, name, NAMELEN ) )
    {
        char errmsg[80];
        sprintf(errmsg,"Classification %s is missing",
                ldt_get_code( ID_CLASSTYPE, 0, sd->clsf[fld->id].class_id ) );
        df_data_file_error( sd->df, MISSING_DATA, errmsg );
        ldt_cancel_data();
        return 0;
    }

    assert( fld->type == DFT_CLASS );
    assert( fld->id >= 0 && fld->id <= sd->nclass );

    cd = sd->clsf + fld->id;
    if( _stricmp(name,"none") != 0 )
    {
        name_id = (int) ldt_get_id( ID_CLASSNAME, cd->class_id, name );
    }
    cd->data_id = name_id;
    return 1;
}

static void load_data_classifications( snapfile_def *sd )
{
    int ic;
    data_class *cd;

    cd = sd->clsf;

    for( ic = sd->nclass; ic--; cd++ )
    {
        if( cd->data_id > 0 ) ldt_classification( cd->class_id, cd->data_id );
    }
}

/*============================================================*/
/* Management of systematic errors                            */


static int get_syserr( snapfile_def *sd, char *sename )
{
    int i;
    for( i = 0; i<sd->nsyserr; i++ )
    {
        if( _stricmp( sename, sd->syserr[i].name ) == 0 ) return i;
    }
    return -1;
}

static int create_syserr( snapfile_def *sd, char *sename, int class_id )
{
    int ic;
    data_syserr *ds;
    ic = get_syserr( sd, sename );
    if( ic >= 0 )
    {
        if( sd->syserr[ic].class_id != class_id )
        {
            char errmess[120];
            sprintf( errmess, "Cannot redefine systematic error %s",sename);
            df_data_file_error( sd->df, INVALID_DATA, errmess );
        }
        return ic;
    }

    if( sd->nsyserr >= sd->maxsyserr )
    {
        sd->maxsyserr = sd->nsyserr + 5;
        sd->syserr= (data_syserr *)
                    check_realloc( sd->syserr, sd->maxsyserr*sizeof(data_syserr));
    }
    ic = sd->nsyserr;
    sd->nsyserr ++;
    ds = sd->syserr + ic;

    ds->name = copy_string( sename );
    ds->name_len = strlen( sename );
    ds->syserr_id = class_id >= 0 ? 0 : (int) ldt_get_id( ID_SYSERR, 0, sename );
    ds->class_id = class_id;
    ds->class_name_id = -1;

    return ic;
}


/* Procedure for loading systematic errors is analogous to that for
   classfications, ie init_data_syserrs for each observations, then
   read_data_syserr for each DFT_SYSERR field, then load_data_syserrs */

static void init_data_syserrs( snapfile_def *sd )
{
    int i;
    for( i = 0; i < sd->nsyserr; i++ )
    {
        sd->syserr[i].defined = 0;
    }
}

static int read_data_syserr( snapfile_def *sd, data_field *fld )
{
    data_syserr *ds;
    int i;
    double *inf;
    int isangle;

    assert( fld->type == DFT_SYSERR );
    assert( fld->id >= 0 && fld->id <= sd->nsyserr);

    ds = sd->syserr + fld->id;
    inf = ds->influence;
    ds->defined = 1;
    ds->vector = sd->obstype->datatype->isvector;
    isangle = sd->obstype->datatype->isangle;

    for( i = ds->vector ? 3 : 1; i--; inf++)
    {
        ds->defined = ds->defined && df_read_double( sd->df, inf );
        if( isangle ) *inf *= STOR;
    }

    if( !ds->defined )
    {
        char errmsg[100];
        sprintf(errmsg,"Influence of %s is missing or invalid",
                ldt_get_code( ID_SYSERR, 0, ds->syserr_id ) );
        df_data_file_error( sd->df, MISSING_DATA, errmsg );
        ldt_cancel_data();
        return 0;
    }

    return ds->defined ? 1 : 0;
}


static void load_data_syserrs( snapfile_def *sd )
{
    int ic;
    data_syserr *ds;

    ds = sd->syserr;

    for( ic = sd->nsyserr; ic--; ds++ )
    {
        if( !ds->defined ) continue;

        /* If the systematic error is classification dependent, get the
        value for the class */

        if( ds->class_id >= 0 )
        {
            int class_id;
            int class_name_id;
            class_id = ds->class_id;
            class_name_id = sd->clsf[class_id].data_id;
            if( class_name_id != ds->class_name_id )
            {
                const char *clsf_code = "default";
                char syserrname[80];
                ds->class_name_id = class_name_id;
                if( class_name_id )
                {
                    clsf_code = ldt_get_code( ID_CLASSNAME,
                                              sd->clsf[class_id].class_id, class_name_id );
                }
                strcpy( syserrname, ds->name );
                strcpy( syserrname + ds->name_len, "/");
                strncpy( syserrname + ds->name_len + 1, clsf_code, 79 - ds->name_len );
                ds->syserr_id = ldt_get_id( ID_SYSERR, 0, syserrname );
            }
        }
        if( ds->vector )
        {
            ldt_vecsyserr( ds->syserr_id, ds->influence );
        }
        else
        {
            ldt_syserr( ds->syserr_id, ds->influence[0] );
        }
    }
}

/*=====================================================================*/
/* Handling of refraction coefficients, distance scale errors, and     */
/* bearing orientation errors                                          */

#pragma warning(disable: 4100)

static int read_proj_command( snapfile_def *sd, int id, char *cmd )
{
    char name[NAMELEN];

    if( ! df_read_field( sd->df, name, NAMELEN ) )
    {
        char errmsg[100];
        sprintf(errmsg,"Name missing - use syntax %c%s name",
                COMMAND_PREFIX, cmd );
        df_data_file_error( sd->df, MISSING_DATA, errmsg );
        return OK; // Already reported
    }   
    sd->projctn = ldt_get_id( ID_PROJCTN, 0, name );
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

static int read_coef_command( snapfile_def *sd, int id, char *cmd )
{
    char name[NAMELEN];
    int classid;

    coef_class_info *cinfo = coef_class( id );
    classid = get_coef_class_id( sd, id );

    if( ! df_read_field( sd->df, name, NAMELEN ) )
    {
        char errmsg[100];
        sprintf(errmsg,"Name missing - use syntax %c%s name",
                COMMAND_PREFIX, cmd );
        df_data_file_error( sd->df, MISSING_DATA, errmsg );
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
            df_data_file_error( sd->df, MISSING_DATA,
                                "Missing or invalid projection - check #projection in data file");
            ldt_cancel_inst();
            sd->definition_err = 1;
        }
        ldt_projection( sd->projctn );
    }
}


/*=====================================================================*/

#pragma warning(disable: 4100)

static int read_error_command( snapfile_def *sd, int errtype, char *cmd )
{
    double value[3];

    char *ppm = "ppm";
    char *mm  = "mm";
    char *mmh = "mmh";
    char *mmv = "mmv";
    char *mmr = "mmr";
    char *ppmr = "ppmr";
    char *secs = "sec";
    int nerrvals;
    int goterr = 0;
    int status;

    struct
    {
        char *code;
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

    for(;;)
    {
        char name[10];
        status = OK;
        if( !df_read_double( sd->df, &value[0] )) break;
        for( iv = 1; iv < nerrvals; iv++ )
        {
            status = MISSING_DATA;
            if( !df_read_double( sd->df, &value[iv] )) break;
            status = OK;
        }
        if( status != OK ) break;
        if( !df_read_field( sd->df, name, 10 ) ) {status = MISSING_DATA; break; }
        status = INVALID_DATA;
        for( ic = 0; ic < nerrcodes; ic++ )
        {
            if( !errcodes[ic].found && _stricmp( errcodes[ic].code, name ) == 0 )
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
        char errmsg[100];
        char *s;
        sprintf(errmsg,"%s error definition - use syntax %c%s",
                status == MISSING_DATA ? "Missing" : "Invalid",
                COMMAND_PREFIX,cmd);
        s = errmsg + strlen(errmsg);
        for( ic = 0; ic < nerrcodes; ic++ )
        {
            strcpy( s, " " ); s++;
            for( iv = 0; iv < nerrvals; iv++ )
            {
                strcpy(s,"#.# ");
                s+=4;
            }
            strcpy( s, errcodes[ic].code );
            s += strlen( s );
        }

        df_data_file_error( sd->df, status, errmsg );
    }

    else
    {
        sd->gotdflterr[errtype] = 1;
    }

    return OK; /* As any errors have already been reported */
}


static void report_missing_default_error( snapfile_def *sd, snap_data_type *obstype )
{
    char errmsg[100];
    char *errcmd;
    int ic;

    errcmd = 0;
    for( ic = 0; commands[ic].command && !errcmd; ic++ )
    {
        if( commands[ic].action == read_error_command &&
                commands[ic].id == obstype->errortype )
            errcmd = commands[ic].command;
    }
    sprintf(errmsg,"Error of %s not defined%s%c%s",
            datatype[obstype->type].name,
            errcmd ? " - use " : "",
            errcmd ? COMMAND_PREFIX : '\0',
            errcmd ? errcmd : "");
    df_data_file_error( sd->df, MISSING_DATA, errmsg );

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
            double mmherr, mmverr, dist, angle;
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

#pragma warning(disable: 4100)

static int read_angle_type_command( snapfile_def *sd, int id, char *cmd )
{
    sd->dmsformat = id;
    return OK;
}

#pragma warning(disable: 4100)

static int read_date_command( snapfile_def *sd, int id, char *cmd )
{
    char mon[4];
    char unknown[10];
    int dy, mn, yr;
    int ok;
    DATAFILE *d;
    char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL",
                       "AUG","SEP","OCT","NOV","DEC"
                     };

    /* Is the date unknown */

    d = sd->df;

    ok = df_read_field( d, unknown, 10 ) &&
         _stricmp(unknown, "unknown") == 0;
    if( ok ) { sd->date = UNKNOWN_DATE; return 1; }

    /* Otherwise read the date */

    df_reread_field( d );
    dy = mn = yr = 0;
    ok = df_read_int( d, &dy ) &&
         df_read_field( d, mon, 4 ) &&
         df_read_int( d, &yr ) &&
         ( dy >= 1 || dy <= 31 );
    if( ok )
    {
        if( yr < 100 ) yr += 1900;
        for( mn = 0; mn < 12; mn++ )
            if ( _stricmp(mon,months[mn]) == 0 ) break;
        ok = mn < 12;
        mn++;
    }
    if( ok )
    {
        sd->date = snap_date( yr, mn, dy ) + 0.5;
    }
    else
    {
        char errmsg[100];
        sprintf(errmsg,"Invalid date definition, use \"%c%s unknown\" or eg %c%s 5 MAY 1993",
                COMMAND_PREFIX,cmd,COMMAND_PREFIX,cmd );
        df_data_file_error( d, INVALID_DATA, errmsg );
    }

    return OK; /* As errors are handled */
}


static int read_time( DATAFILE *d, double *obstime )
{
    char time[10];
    int hr, min;

    if( !df_read_field( d, time, 10 ) ) return 0;

    if( (sscanf(time,"%d:%d", &hr, &min ) < 2 &&
            sscanf(time,"%d.%d", &hr, &min ) < 2 ) ||
            hr < 0 || hr > 24 || min < 0 || min > 59 ) return 0;

    *obstime = (hr + min/60.0)/24.0;
    return 1;
}


#pragma warning(disable: 4100)

static int read_time_command( snapfile_def *sd, int id, char *cmd )
{
    double obstime;
    if( read_time( sd->df, &obstime ) )
    {
        if( sd->date != UNKNOWN_DATE ) sd->date = floor( sd->date) + obstime;
        ldt_date( sd->date );
    }
    else
    {
        char errmsg[80];
        sprintf(errmsg,"Invalid time - use syntax eg \"%c%s 15:20\"",
                COMMAND_PREFIX, cmd );
        df_data_file_error( sd->df, INVALID_DATA, errmsg );
    }
    return OK; /* Since errors are already handled */
}

#pragma warning(disable: 4100)

static int read_data_time( snapfile_def *sd, data_field *fld )
{
    double obstime;
    if( read_time( sd->df, &obstime ) )
    {
        ldt_time( obstime );
    }
    else
    {
        df_data_file_error( sd->df, INVALID_DATA,
                            "Invalid time - use syntax eg \"15:20\"");
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

#pragma warning(disable: 4100)

static int read_gps_errtype_command( snapfile_def *sd, int id, char *cmd )
{
    char option[20];
    int ok;

    ok = 0;
    if( df_read_field( sd->df, option, 20 ))
    {
        ok = 1;
        if( ISEQ(option,"diagonal") )  sd->dfltcvrtype = CVR_DIAGONAL;
        else if( ISEQ(option,"full") ) sd->dfltcvrtype = CVR_FULL;
        else if( ISEQ(option,"correlation") )
            sd->dfltcvrtype = CVR_CORRELATION;
        else if( ISEQ(option,"enu") )  sd->dfltcvrtype = CVR_TOPOCENTRIC;
        else ok = 0;
    }
    sd->cvrupper = 0;
    if( df_read_field( sd->df, option, 20 ))
    {
        if( ISEQ(option,"upper") ) sd->cvrupper = 1; else ok = 0;
    }

    if( !ok )
    {
        df_data_file_error( sd->df, INVALID_DATA,
                            "Invalid gps error type - must be \"diagonal\", \"full\", \"correlation\", or \"enu\"");
    }

    return OK; /* As error is already handled */
}

#pragma warning(disable: 4100)

static int read_syserr_command( snapfile_def *sd, int id, char *cmd )
{
    char name[NAMELEN];
    char classname[NAMELEN];
    int class_id;

    if( !df_read_field( sd->df, name, NAMELEN ) )
    {
        char errmsg[100];
        sprintf(errmsg,"Name missing - use %c%s name [classification_name]",
                COMMAND_PREFIX, cmd );
        df_data_file_error( sd->df,MISSING_DATA,errmsg );
        return OK;
    }

    if( df_read_field( sd->df, classname, NAMELEN ))
    {
        class_id = create_classification( sd, classname );
    }
    else
    {
        class_id = -1;
    }

    create_syserr( sd, name, class_id );
    return OK;
}


#pragma warning(disable: 4100)

static int read_classification( snapfile_def *sd, int id, char *cmd )
{
    char name[NAMELEN];
    if( !df_read_field( sd->df, name, NAMELEN ) )
    {
        char errmsg[100];
        sprintf(errmsg,"Name missing - use %c%s name",COMMAND_PREFIX,cmd );
        df_data_file_error( sd->df, MISSING_DATA, errmsg );
    }
    else
    {
        create_classification( sd, name );
    }
    return OK;
}

#pragma warning(disable: 4100)

static int read_classify_command( snapfile_def *sd, int id, char *cmd )
{
    char fields[3][NAMELEN];
    char *types, *clsf, *value;
    int class_id;
    int i;

    for( i=0; i<3; i++ )
    {
        if( ! df_read_field( sd->df, fields[i], NAMELEN ) ) break;
    }

    if( i < 2 )
    {
        char errmsg[100];
        sprintf(errmsg,"Missing information - use %c%s [type/type...] class value",
                COMMAND_PREFIX, cmd);
        df_data_file_error( sd->df, MISSING_DATA, errmsg );
    }

    if( i == 2 )
    {
        types = NULL;
        i = 0;
    }
    else
    {
        types = fields[0];
        i = 1;
    }
    clsf = fields[i++];
    value = fields[i++];

    class_id = create_classification( sd, clsf );

    if( types )
    {
        char *te;
        int end;
        snap_data_type *obstype;
        te = types;
        end = 0;
        while( !end )
        {
            te = types;
            while( *te && *te != '/' ) te++;
            end = (*te == 0);
            *te = 0;
            obstype = obstype_from_code( types );
            if( ! obstype )
            {
                char errmsg[100];
                sprintf(errmsg,"Invalid observation type %s",types);
                df_data_file_error( sd->df, INVALID_DATA, errmsg );
            }
            else
            {
                set_obstype_classification( sd, obstype->type, class_id, value );
            }
            types = te+1;
        }
    }

    else
    {
        set_obstype_classification( sd, -1, class_id, value );
    }
    return OK; /* All errors are already reported */
}

#pragma warning(disable: 4100)

static int read_endset_command( snapfile_def *sd, int id, char *cmd )
{
    return OK; /* Nothing to do - just marks end of the group */
}

#pragma warning(disable: 4100)

static int read_note_command( snapfile_def *sd, int id, char *cmd )
{
    char note[NOTELEN];
    if( sd->definition_err ) return OK;
    note[0] = 0;
    df_read_rest( sd->df, note, NOTELEN );
    ldt_prefix_note( note );
    return OK;
}

/**********************************************************************/
/* Reading a data command -                                           */

static data_field *next_data_field( snapfile_def *sd, int dftype, int id, int sec_id )
{
    data_field *df;
    if( sd->nfield >= sd->maxfield )
    {
        sd->maxfield = sd->nfield+10;
        sd->fields = (data_field *)
                     check_realloc( sd->fields, sd->maxfield*sizeof(data_field) );
    }
    df = sd->fields + sd->nfield;
    sd->nfield++;
    df->type = dftype;
    df->id = id;
    df->sec_id = sec_id;
    return df;
}


#pragma warning(disable: 4100)

static int read_data_command( snapfile_def *sd, int id, char *cmd )
{
    int nobs = 0;
    int oneonly = 0;
    int startno = 0;
    snap_data_type *obstype = 0;
    char name[COMLEN];

    /* Set up the defaults */

    sd->noinststn = 0;
    sd->grouped = 0;
    sd->heights = 1;
    sd->definition_err = 0;
    sd->group_err = 0;
    sd->nfield = 0;
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

    while( df_read_field( sd->df, name, COMLEN ) )
    {

        /* Commands which can precede an observation type */

        if( _stricmp( name, "grouped" ) == 0 ) { sd->grouped = 1; continue; }
        if( _stricmp( name, "no_heights") == 0 ) { sd->heights = 0; continue; }
        if( _stricmp( name, "time" ) == 0 ) { next_data_field( sd, DFT_TIME, 0, 0 ); continue; }

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
                char errmsg[100];
                sprintf(errmsg,"%s obs not compatible with %s",
                        datatype[sd->fields[0].id].name, dt->name );
                df_data_file_error( sd->df, INCONSISTENT_DATA, errmsg );
                sd->definition_err = 1;
                break;
            }

            sd->obsclass = st->obsclass;
            /* Point vectors are defined using a target station and no inst station,
               so they can be grouped!? */
            sd->noinststn = dt->isvector && dt->ispoint;
            sd->obstype = obstype;
            startno = sd->nfield;
            next_data_field( sd, DFT_START, obstype->type,
                             FLG_DFLT_DATA | FLG_DFLT_ERROR );
            nobs++;
            if( st->datatype->needsgroup ) sd->grouped = 1;
            if( st->oneonly ) oneonly = obstype->type+1;
            continue;
        }

        if( _stricmp( name, "value" ) == 0 )
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

        if ( _stricmp( name, "error" ) == 0 )
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

        if( _stricmp( name, "distance_scale_error" ) == 0 )
        {
            next_data_field( sd, DFT_CLASS, get_coef_class_id(sd,COEF_CLASS_DISTSF), 0 );
            continue;
        }

        if( _stricmp( name, "refraction_coefficient" ) == 0 )
        {
            next_data_field( sd, DFT_CLASS, get_coef_class_id(sd,COEF_CLASS_REFCOEF), 0 );
            continue;
        }

        if( _stricmp( name, "bearing_orientation_error" ) == 0 )
        {
            next_data_field( sd, DFT_CLASS, get_coef_class_id(sd,COEF_CLASS_BRNGREF), 0 );
            continue;
        }

        if( _stricmp( name, "id" ) == 0 )
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
        char errmsg[100];
        sprintf( errmsg,"Field \"%s\" invalid or out of place in %c%s",name,
                 COMMAND_PREFIX,cmd );
        df_data_file_error( sd->df, INVALID_DATA, errmsg );
    }

    else if( sd->usereffrm && sd->useprojctn )
    {
        df_data_file_error(sd->df, INVALID_DATA,
                           "Cannot mix data using reference frames and projections");
        sd->definition_err = 1;
    }

    else if( nobs < 1 )
    {
        char errmsg[80];
        sprintf( errmsg,"No data is specified in %c%s",COMMAND_PREFIX,cmd);
        df_data_file_error( sd->df, MISSING_DATA, errmsg );
        sd->definition_err = 1;
    }

    else if( nobs > 1 && oneonly )
    {
        char errmsg[100];
        sprintf(errmsg,"Cannot combine %s with other observations",
                datatype[oneonly-1].name );
        df_data_file_error( sd->df, INCONSISTENT_DATA, errmsg );
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
    int nc = sd->nvecobs*3;
    int *row;
    int i, i3;
    if( nc > sd->maxcvrrow )
    {
        sd->maxcvrrow = nc+30;
        if( sd->cvrrow ) check_free( sd->cvrrow );
        sd->cvrrow = (int *) check_malloc( sd->maxcvrrow * sizeof(int) );
    }
    reset_list_pointer( sd->vecerrlst );
    row = sd->cvrrow;
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
    double val;
    int i, i3, j3, ok, cvrtype;
    int grouped;
    char errmess[80];
    DATAFILE *df;
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
        if( !data_available )
        {
            sprintf(errmess,"The %s matrix is missing",
                    cvrtype == CVR_CORRELATION ? "correlation" : "covariance");
            df_data_file_error( sd->df, MISSING_DATA, errmess );
            ldt_cancel_inst();
            ok = 0;
        }
        else
        {
            int *row = sd->cvrrow;
            int nvecrow = sd->nvecobs*3;
            df = sd->df;

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

                    while( df_end_of_line( df ) ) df_read_data_file( df );
                    ok = df_read_double( df, &val );
                    if( ri >= 0 && rj >= 0 ) Lij(cvr,ri,rj) = val;
                    if( !ok )
                    {
                        sprintf(errmess,"The %s matrix is not correctly specified",
                                cvrtype == CVR_CORRELATION ? "correlation" : "covariance" );
                        df_data_file_error( df, INVALID_DATA, errmess);
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
    double *cvr;
    int i;

    cvr = sd->currvecerr->vecerr;

    for ( i = sd->nveccvr; i--; ) if( !df_read_double( sd->df, cvr++ ) )
        {
            df_data_file_error( sd->df, INVALID_DATA,
                                "The errors for the vector components are not correctly specified");
            ldt_cancel_data();
            return 0;
        }
    if( sd->cvrtype == CVR_FULL && sd->nveccvr == 6 && sd->cvrupper )
    {
        double tmp;
        cvr = sd->currvecerr->vecerr;
        tmp = cvr[2]; cvr[2] = cvr[3]; cvr[3] = tmp;
    }


    return 1;
}


/* Flag a vector error as being used in the final covariance matrix */

static void validate_vector_error( snapfile_def *sd )
{
    if( !ldt_data_cancelled() && sd->currvecerr )
        sd->currvecerr->nvecobs = ++sd->nvecgood;
}




static int read_data_error( snapfile_def *sd, data_field *fld )
{
    snap_data_type *st;
    double error;

    st = sd->obstype;

    if( st->datatype->isvector )
    {
        read_vector_error( sd );
    }
    else
    {
        if( !df_read_double( sd->df, &error ))
        {
            char errmsg[80];
            sprintf(errmsg,"Invalid or missing error for %s",
                    datatype[st->type].name);
            df_data_file_error( sd->df, INVALID_DATA, errmsg );
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


static int read_sign( DATAFILE *df, char *sign, double *value )
{
    char field[2];

    if( !df_read_field( df, field, 2 )) return 0;
    _strupr( field );
    if( field[0] == sign[0] ) return 1;
    if( field[0] == sign[1] ) { *value = - *value; return 1; }
    return 0;
}

static int read_data_data( snapfile_def *sd, data_field *fld )
{
    double value[3];
    snap_data_type *st;
    int sts;
    char reject[3];
    char errstr[10];
    int unused;

    /* Check whether the observation is to be rejected */
    /* Read the field, then reread skip the first character if the obs
       is to be rejected */

    df_read_field( sd->df, reject, 3 );
    df_reread_field( sd->df );

    if( reject[0] == REJECT_CHAR )
    {
        df_skip_character( sd->df );
        unused = 1;
    }
    else
    {
        unused = sd->rejobs;
    }

    st = sd->obstype;

    sts = 1;
    if( st->datatype->isangle )
    {
        switch( sd->dmsformat )
        {
        case AF_HP:  sts = df_read_hpangle( sd->df, value ); break;
        case AF_DMS: sts = df_read_dmsangle( sd->df, value ); break;
        case AF_DEG: sts = df_read_degangle( sd->df, value ); break;
        default: handle_error(INTERNAL_ERROR,"Invalid angle format",__FILE__ " read_data_data" ); break;
        }
        if( sd->dmsformat != AF_DEG )
        {
            if( sts && st->type == LT ) sts = read_sign( sd->df, "NS", value );
            if( sts && st->type == LN ) sts = read_sign( sd->df, "EW", value );
        }
    }
    else if ( st->datatype->isvector )
    {
        sts = df_read_double( sd->df, value )   &&
              df_read_double( sd->df, value+1 ) &&
              df_read_double( sd->df, value+2 );
    }
    else
    {
        sts = df_read_double( sd->df, value );
    }

    if( !sts )
    {
        char errmsg[80];
        sprintf(errmsg,"%s is missing",
                datatype[sd->obstype->type].name);
        df_data_file_error( sd->df, MISSING_DATA, errmsg );
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

    if( sts && !sd->goterr && df_read_field( sd->df, errstr, 10 ) )
    {
        if( _stricmp(errstr,ERROR1)==0 || _stricmp(errstr,ERROR2)==0 )
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
            df_reread_field( sd->df );
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
    char skip[3];
    int sts;

    sts = df_read_field( sd->df, skip, 3 );
    if( sts && skip[0] == SKIPOBS_CHAR && skip[1] == 0 )
    {
        sd->skipobs = 1;
        return OK;
    }
    else
    {
        df_reread_field( sd->df );
        sd->skipobs = 0;
    }

    ldt_nextdata( fld->id );

    sd->obstype = snap_data_type_from_id(fld->id);
    init_data_classifications( sd );
    init_data_syserrs( sd );

    sts = OK;
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

static int read_station( snapfile_def *sd, int *stn_id, double *hgt )
{
    char name[NAMELEN];
    int sts;

    /* Read the station code and the optional height */

    *stn_id = 0;
    *hgt = 0.0;

    sts = df_read_field( sd->df, name, NAMELEN );
    if( !sts )
    {
        char errmsg[100];
        sprintf(errmsg, "Invalid or missing %s station code",
                sd->ingroup ? "target" : "instrument" );
        df_data_file_error( sd->df, INVALID_DATA, errmsg );
        return INVALID_DATA;
    }

    *stn_id = ldt_get_id( ID_STATION, 0, name );
    if( *stn_id == 0 )
    {
        char errmsg[100];
        sprintf( errmsg, "Invalid %s station code \"%s\"",
                 sd->ingroup ? "target" : "instrument", name);
        df_data_file_error( sd->df, INVALID_DATA, errmsg );
    }

    if( sd->heights )
    {
        sts = df_read_double( sd->df, hgt );
        if( !sts )
        {
            char errmsg[80];
            sprintf( errmsg, "Invalid or missing %s height",
                     sd->ingroup ? "target" : "instrument" );
            df_data_file_error( sd->df, INVALID_DATA, errmsg );
            if( !sd->ingroup ) sd->group_err = 1;
            return INVALID_DATA;
        }
    }

    return *stn_id <= 0 ? WARNING_ERROR : OK;
}


static int read_data_line( snapfile_def *sd, int rej )
{
    int stn_id;
    double ihgt;
    int startgrp;
    int sts;
    int i;

    if( sd->nfield <= 0 )
    {
        char errmsg[80];
        sprintf(errmsg, "Definition of data format missing - use %cdata",COMMAND_PREFIX);
        df_data_file_error( sd->df, MISSING_DATA, errmsg );
        sd->definition_err = 1;
        return MISSING_DATA;
    }

    ldt_lineno( df_line_number( sd->df ) );

    sts = read_station( sd, &stn_id, &ihgt );

    /* For grouped data which does not require an end of set test whether
       this is the end of the line.  If it is then automatically start
       a new group. */

    startgrp = !sd->grouped ||
               !sd->ingroup ||
               (!sd->endset && df_end_of_line( sd->df ));

    if( startgrp )
    {
        if( sd->ingroup ) end_group(sd, 0);
        start_group( sd );

        sd->stn_id_inst = -1;
        if( sd->noinststn )
        {
            startgrp = 0;
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
        if( ! sd->noinststn) sts = read_station( sd, &stn_id, &ihgt );
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
        data_field *fld;
        for( i = sd->nfield, fld = sd->fields; i--; fld++ )
        {
            if( sd->skipobs && fld->type != DFT_START ) continue;
            switch( fld->type )
            {
            case DFT_START:  if( sd->inobs ) end_obs( sd );
                start_obs( sd, fld );
                break;
            case DFT_DATA:   read_data_data( sd, fld ); break;
            case DFT_ERROR:  read_data_error( sd, fld ); break;
            case DFT_TIME:   read_data_time( sd, fld ); break;
            case DFT_OBSID:  read_data_obs_id( sd ); break;
            case DFT_CLASS:  read_data_classification( sd, fld ); break;
            case DFT_SYSERR: read_data_syserr( sd, fld ); break;
            default: assert(0);
            }
        }
    }

    if( sd->inobs ) end_obs( sd );

    /* Check that there is no spurious data on the line */

    if( !df_end_of_line( sd->df ) )
    {
        df_data_file_error( sd->df, TOO_MUCH_DATA, "Extra data in data file");
    }
    return OK;
}

static void process_command( snapfile_def *sd )
{
    command *cmd;
    char  cmdname[COMLEN];

    df_read_field( sd->df, cmdname, COMLEN );

    for( cmd = commands; cmd->command; cmd++ )
    {
        if( _stricmp( cmd->command, cmdname ) == 0 ) break;
    }

    if( !cmd->command )
    {
        char errmsg[80];
        sprintf(errmsg,"Invalid data definition command \"#%.30s\"",cmdname);
        df_data_file_error( sd->df, INVALID_DATA, errmsg );
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
    snapfile_def sd;
    int sts;

    df_read_data_file( df );  /* Skip over the header line */

    init_snapfile_def( &sd, df );

    sts = OK;
    while( df_read_data_file( df ) == OK )
    {
        char prefix[2];
        int rej;
        if( check_progress && !(*check_progress)(df) )
        {
            sts = OPERATION_ABORTED;
            break;
        }
        if( !df_read_field( df, prefix, 2 )) continue;
        df_reread_field( df );
        if( prefix[0] == COMMAND_PREFIX )
        {
            df_skip_character( df );
            process_command( &sd );
        }
        else if( !sd.definition_err )
        {
            rej = (prefix[0] == REJECT_CHAR);
            if( rej ) df_skip_character( df );
            read_data_line( &sd, rej );
        }
    }
    if( sd.ingroup ) end_group( &sd, 0 );
    term_snapfile_def( &sd );
    return sts;
}
