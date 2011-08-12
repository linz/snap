#include "snapconfig.h"
/* loaddata.h: Routines to load observations into the structures defined
   in loaddata.h.

   Handling of errors needs a lot of rationallizing ...

   */

/*
   $Log: loaddata.c,v $
   Revision 1.2  2004/04/22 02:35:15  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:46:36  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "util/chkalloc.h"
#include "util/symmatrx.h"
#include "snapdata/datatype.h"
#include "snapdata/loaddata.h"
#include "snapdata/survdata.h"
#include "util/errdef.h"
#include "util/dstring.h"

#define INC_SIZE 5   /* Increment for data blocks */

#ifdef DEBUG_LOADDATA
#define DEBUG_PRINT(x) { putchar('\n'); printf x ; putchar( '\n' ); }
#else
#define DEBUG_PRINT(x)
#endif

static char rcsid[]="$Id: loaddata.c,v 1.2 2004/04/22 02:35:15 ccrook Exp $";

#define ID_UNDEFINED -1

static survdata data;
static trgtdata *tgt = NULL;
static int tgt_id;
static double tgt_hgt;
static long file_lineno;
static long noteloc;
static int cancelled;
static int data_cancelled;
static int trgt_cancelled;
static int trgt_init_nobs;

static obsdata *od;
static vecdata *vd;
static pntdata *pd;
static datatypedef *obsdatatype;

static double *aval;
static int  nval;
static double *aerr;
static int  nerr;
static int  iscovar;
static int  gotval;
static int  goterr;

static int projid=ID_UNDEFINED;
static int projclassid=ID_UNDEFINED;
static int projidcache=ID_UNDEFINED;     // Caching of projection code value
static int projcodeid=ID_UNDEFINED;

static int default_refcoef = ID_UNDEFINED;
static int default_reffrm = ID_UNDEFINED;
static int reffrm_classified = 0;
static int reffrm_classification = ID_UNDEFINED;

static unsigned char *datablock = NULL;
static classdata *classblock = NULL;
static syserrdata *syserrblock = NULL;
static int datasize = 0;
static int classblocksize = 0;
static int syserrblocksize = 0;

static ltmat cvr = NULL;
static ltmat calccvr = NULL;
static ltmat rescvr = NULL;
static int cvrdim = 0;

/* The function to call when data has been read */

void (*usedata_func)( survdata *sd );

/* The functions for converting codes to and from numeric id's */

long (*id_func)( int type, int group_id, const char *code );
const char * (*code_func)( int type, int group_id, long id );

/* The function for computing gps covariances is handled with a function
   pointer to avoid linking in the associated code when we are not
   interested in gps covariances */

static void (*gpscvr_func)( survdata *vd, int cvrtype,
                            double *mmerr ) = 0;

static int cvrtype;
static double cvr_mmerr[9];
// static double cvr_ppmerr[3];

/* Initiallization and clean-up functions */

static coef_class_info coef_classes[] =
{
    {"distance_scale_code", &datatypedef::distsf },
    {"bearing_error_code", &datatypedef::brngref },
    {"refraction_coef_code", &datatypedef::refcoef },
    {"ref_frame_code", &datatypedef::reffrm }
};

static struct
{
    char *classname;
    int idtype;
    int idclass;
    int idname;
    int idcoef;
} coef_class_id[N_COEF_CLASSES];

coef_class_info *coef_class( int coeftype )
{
    return &(coef_classes[coeftype]);
}

void set_coef_class( int coeftype, const char *name )
{
    if( coef_class_id[coeftype].classname )
    {
        check_free( coef_class_id[coeftype].classname );
        coef_class_id[coeftype].classname = 0;
    }
    if( name )
    {
        coef_class_id[coeftype].classname = copy_string( name );
    }
}

static void init_coef_class_id()
{
    for( int i = 0; i < N_COEF_CLASSES; i++ )
    {
        coef_class_id[i].idtype = i;
        if( ! coef_class_id[i].classname )
        {
            coef_class_id[i].classname = copy_string( coef_classes[i].default_classname );
        }
        coef_class_id[i].idclass = ldt_get_id( ID_CLASSTYPE,0, coef_class_id[i].classname);
        coef_class_id[i].idname = ID_UNDEFINED;
        coef_class_id[i].idcoef = ID_UNDEFINED;
    }
}

static int get_coef_id( int coeftype, int idname )
{
    if( idname==coef_class_id[coeftype].idname) return coef_class_id[coeftype].idcoef;
    const char * name = ldt_get_code( ID_CLASSNAME, coef_class_id[coeftype].idclass, idname );
    int idcoef = ldt_get_id( ID_COEF, coeftype, name );
    coef_class_id[coeftype].idcoef = idcoef;
    coef_class_id[coeftype].idname = idname;
    return idcoef;
}

static void report_error( char *location )
{
    char msg[100];

    sprintf(msg,"Internal programming error in call to %s",location);
    handle_error( INTERNAL_ERROR, msg, "In module loaddata.c");
    return;
}

void init_load_data( void (*usedata)( survdata *sd ),
                     long (*idfunc)( int type, int group_id, const char *code ),
                     const char * (*codefunc)( int type, int group_id, long id ))
{
    DEBUG_PRINT(("LDT: init_load_data"));
    usedata_func = usedata;
    id_func = idfunc;
    code_func = codefunc;

    data.nobs = 0;
    noteloc = 0;
    tgt = NULL;
    tgt_id = 0;
    od = NULL;
    vd = NULL;
    pd = NULL;
    init_coef_class_id();
}

void term_load_data( void )
{
    DEBUG_PRINT(("LDT: term_load_data"));
    if( cvr ) free( cvr );
    if( calccvr ) free( calccvr );
    if( rescvr ) free( rescvr );
    if( datablock ) check_free( datablock );
    if( classblock ) check_free( classblock );
    if( syserrblock ) check_free( syserrblock );

    cvrdim = 0;
    cvr = NULL;
    calccvr = NULL;
    rescvr = NULL;
    datablock = NULL;
    classblock = NULL;
    syserrblock = NULL;
    datasize = 0;
    classblocksize = 0;
    syserrblocksize = 0;
}

void set_gpscvr_func( void (*func)( survdata *vd, int cvrtype,
                                    double *mmerr ) )
{
    DEBUG_PRINT(("LDT: set_gpscvr_fund"));
    gpscvr_func = func;
}


/* Memory allocation routines for the components that grow... */

static void setup_data_format( int format )
{

    data.format = format;
    switch( format )
    {
    case SD_OBSDATA:
        data.obssize = sizeof( obsdata );
        break;

    case SD_VECDATA:
        data.obssize = sizeof( vecdata );
        break;

    case SD_PNTDATA:
        data.obssize = sizeof( pntdata );
        break;
    }

}

static void check_data( void )
{
    if( cancelled || data_cancelled ) return;

    if( !gotval || !goterr)
    {
        handle_error( INVALID_DATA, "Observation value or error not defined",NO_MESSAGE);
    }
    switch( data.format )
    {
    case SD_OBSDATA:
        if( obsdatatype->projctn )
        {
            if( projid == ID_UNDEFINED)
            {
                projid = ldt_get_id( ID_PROJCTN, 0, DEFAULT_PROJECTION );
            }
            data.reffrm = projid;
            if( projclassid == ID_UNDEFINED )
            {
                projclassid = ldt_get_id( ID_CLASSTYPE, 0, "projection" );
            }
            if( projclassid )
            {
                if( projid != projidcache )
                {
                    const char *code = ldt_get_code( ID_PROJCTN, 0, projid );
                    projidcache = projid;
                    projcodeid = ldt_get_id( ID_CLASSNAME, projclassid, code );
                }
                ldt_classification( projclassid, projcodeid );
            }
        }
        if( obsdatatype->refcoef && od->refcoef == ID_UNDEFINED )
        {
            int idclass = coef_class_id[COEF_CLASS_REFCOEF].idclass;
            if( idclass )
            {
                // If refraction coefficient not yet defined then define it (via classification)
                if( default_refcoef == ID_UNDEFINED)
                {
                    default_refcoef = ldt_get_id( ID_CLASSNAME, idclass, DEFAULT_REFCOEF_NAME);
                }
                ldt_classification( idclass, default_refcoef );
            }
            // No classification so coefficient directly
            else
            {
                if( default_refcoef == ID_UNDEFINED )
                {
                    default_refcoef = ldt_get_id( ID_COEF, COEF_CLASS_REFCOEF, DEFAULT_REFCOEF_NAME );
                }
                od->refcoef = default_refcoef;
            }
        }
        if( od->refcoef == ID_UNDEFINED ) od->refcoef = 0;
        break;

    case SD_VECDATA:
        if( obsdatatype->reffrm )
        {
            int idclass = coef_class_id[COEF_CLASS_REFFRM].idclass;
            if( idclass )
            {
                // If reference frame not yet defined then define it (via classification)
                if( data.reffrm == ID_UNDEFINED )
                {
                    int id = ldt_get_id( ID_CLASSNAME, idclass, DEFAULT_REFFRM_NAME);
                    ldt_classification( idclass, id );
                }
                // Else if subsequent vector and not classified then use previous classification
                else if( ! reffrm_classified )
                {
                    ldt_classification( idclass, reffrm_classification );
                }
            }
            // Cannot define via class, defined direclty
            else if( data.reffrm == ID_UNDEFINED )
            {
                if( default_reffrm == ID_UNDEFINED )
                {
                    default_reffrm = ldt_get_id( ID_COEF, COEF_CLASS_REFFRM, DEFAULT_REFFRM_NAME );
                }
                data.reffrm = default_reffrm;
            }
        }
        break;

    case SD_PNTDATA:
        break;
    }

}


static void setup_target( int datatype )
{
    tgt->to = tgt_id;
    tgt->tohgt = tgt_hgt;
    tgt->id = 0;
    tgt->type = datatype;
    tgt->lineno = file_lineno;
    tgt->nclass = 0;
    tgt->nsyserr = 0;
    tgt->unused = 0;
    tgt->noteloc = noteloc;
    noteloc = 0;
}

static void *next_data( int type )
{
    int required;
    int format;
    void *obs;

    DEBUG_PRINT(("LDT: next_data"));

    if( data.nobs ) check_data();
    obsdatatype = datatypedef_from_id( type );


    if( obsdatatype->isvector )
    {
        format = SD_VECDATA;
    }
    else if( obsdatatype->ispoint )
    {
        format = SD_PNTDATA;
    }
    else
    {
        format = SD_OBSDATA;
    }

    if( data.nobs == 0 )
    {
        setup_data_format( format );
    }
    else if ( format != data.format )
    {
        report_error("next_data");
    }

    required = (data.nobs+1)*data.obssize;
    if( required > datasize )
    {
        datasize = required + INC_SIZE * data.obssize;
        datablock = (unsigned char *) check_realloc( datablock, datasize );
    }
    data_cancelled = 0;
    gotval = 0;
    goterr = 0;
    reffrm_classified = 0;
    obs =  (void *) (datablock + data.nobs++ *data.obssize);

    iscovar = 0;

    switch( data.format )
    {
    case SD_OBSDATA: od = (obsdata *) obs;
        tgt = &od->tgt;
        aval = &od->value;
        nval = 1;
        aerr = &od->error;
        nerr = 1;
        iscovar = 0;
        od->refcoef = ID_UNDEFINED;
        od->prm_id = 0;
        break;

    case SD_VECDATA: vd = (vecdata *) obs;
        tgt = &vd->tgt;
        aval = vd->vector;
        nval = 3;
        aerr = NULL;
        nerr = 0;
        goterr = 1;
        iscovar = 1;
        break;

    case SD_PNTDATA: pd = (pntdata *) obs;
        tgt = &pd->tgt;
        aval = &pd->value;
        nval = 1;
        aerr = &pd->error;
        nerr = 1;
        iscovar = 0;
        break;

    default: report_error("nextdata");
    }
    setup_target( type );

    return obs;
}

static void clear_cvr( int ndim, ltmat cvr )
{
    int i, j;
    for( i = 0; i < ndim; i++ ) for( j = 0; j <= i; j++ )
        {
            *cvr = 0.0;
            cvr++;
        }
}

static ltmat alloc_cvr( void )
{
    long cvrsize;
    int dimreq;
    dimreq = data.nobs * 3;
    if( dimreq > cvrdim )
    {
        if( cvr ) free( cvr );
        if( calccvr ) free( calccvr );
        if( rescvr ) free( rescvr );
        cvr = calccvr = rescvr = NULL;
        cvrdim = dimreq + 6;
        cvrsize = ( (long) cvrdim * (long) (cvrdim+1) ) / 2;
        cvr = (ltmat) malloc( cvrsize * sizeof( double ) );
        calccvr = (ltmat) malloc( cvrsize * sizeof( double ) );
        rescvr = (ltmat) malloc( cvrsize * sizeof( double ) );
        if( !cvr || !calccvr || !rescvr )
        {
            handle_error( MEM_ALLOC_ERROR, NO_MESSAGE, NO_MESSAGE );
        }
    }

    clear_cvr( dimreq, cvr );
    clear_cvr( dimreq, calccvr );
    clear_cvr( dimreq, rescvr );

    return cvr;
}

static syserrdata *getsyserrdata( void )
{
    int idx;
    idx = data.nsyserr;
    if( idx >= syserrblocksize )
    {
        int reqsize = idx+1+INC_SIZE;
        syserrblocksize = reqsize;
        reqsize *= sizeof( syserrdata );
        syserrblock = (syserrdata *) check_realloc( syserrblock, reqsize );
    }
    return syserrblock + idx;
}

static classdata *getclassdata( void )
{
    int idx;
    idx = data.nclass;
    if( idx >= classblocksize )
    {
        int reqsize = idx+1+INC_SIZE;
        classblocksize = reqsize;
        reqsize *= sizeof( classdata );
        classblock = (classdata *) check_realloc( classblock, reqsize );
    }
    return classblock + idx;
}


long ldt_get_id( int type, int group_id, const char *code )
{
    return (*id_func)( type, group_id, code );
}

const char *ldt_get_code( int type, int group_id, long id )
{
    return (*code_func)( type, group_id, id );
}

void ldt_file( int fileno  )
{
    DEBUG_PRINT(("LDT: ldt_file %d", (int) fileno ));
    data.file = fileno;
}

void ldt_lineno( long lineno )
{
    DEBUG_PRINT(("LDT: ldt_lineno %ld", lineno ));
    file_lineno = lineno;
}

void ldt_date( double obsdate )
{
    DEBUG_PRINT(("LDT: ldt_date %.3lf", obsdate));
    data.date = obsdate;
}

void ldt_time( double time )
{
    DEBUG_PRINT(("LDT: ldt_time %.5lf",time));
    if( data.date != UNKNOWN_DATE ) data.date = floor(data.date)+time;
}

void ldt_projection( int proj_id )
{
    DEBUG_PRINT(("LDT: ldt_projection %d", (int) reffrm_id));
    projid = proj_id;
}


void ldt_inststn( int stn_id, double ihgt )
{

    DEBUG_PRINT(("LDT: ldt_inststn %d %.3lf", (int) stn_id, ihgt ));

    /* Save existing data if any */

    if( data.nobs ) ldt_end_data();

    /* Initiallize data structure.

       Note: data.file persists, everything else is reset */

    data.from = stn_id;
    data.fromhgt = ihgt;
    data.date = UNKNOWN_DATE;
    data.reffrm = ID_UNDEFINED;
    data.format = 0;
    data.nobs = 0;
    data.ncvr = 0;
    data.nclass = 0;
    data.nsyserr = 0;

    tgt = NULL;
    tgt_id = 0;
    trgt_init_nobs = 0;
    od = NULL;
    vd = NULL;
    pd = NULL;
    cancelled = 0;
}


void ldt_tgtstn( int stn_id, double ihgt  )
{
    DEBUG_PRINT(("LDT: ldt_tgtstn %d %.3lf", (int) stn_id, ihgt ));
    tgt_id = stn_id;
    tgt_hgt = ihgt;
    trgt_cancelled = 0;
    trgt_init_nobs = data.nobs;
}



void ldt_nextdata( int type )
{
    void *next;

    DEBUG_PRINT(("LDT: ldt_nextdata %d", (int) type ));

    if( data.format == SD_PNTDATA && data.nobs ) ldt_end_data();

    next = next_data(type);
    if( trgt_cancelled ) ldt_cancel_data();
}


void ldt_value( double *value )
{
    int i;

#ifdef DEBUG
    if( nval == 1 )
    {
        DEBUG_PRINT(("LDT: ldt_value  %.5lf",value[0]));
    }
    else
    {
        DEBUG_PRINT(("LDT: ldt_value %.3lf %.3lf %.3lf",value[0],value[1],value[2]));
    }
#endif

    for( i = 0; i<nval; i++ ) aval[i] = value[i];
    gotval = 1;
}

void ldt_error( double *error )
{
    int i;

    DEBUG_PRINT(("LDT: ldt_error %.5lf",error[0]));
    if( !nerr )
    {
        report_error("ldt_error");
    }

    for( i = 0; i<nerr; i++ ) aerr[i] = error[i];
    if( iscovar )
    {
        for( i = 0; i<nerr; i++ ) aerr[i] *= aerr[i];
    }
    goterr = 1;
}

/* Note: this should be called only when all the vectors have been read,
   (ie nobs is correct) */

ltmat ldt_covariance( int type, double* mmerr )
{
    DEBUG_PRINT(("LDT: ldt_covariance %d", (int) type ));

    if( data.format != SD_VECDATA )
    {
        report_error( "ldt_covariance" );
        return NULL;
    }

    if( !gpscvr_func || cancelled )
    {
        data.cvr = NULL;
        data.calccvr = NULL;
        data.rescvr = NULL;
    }
    else
    {
        int i;
        data.cvr = alloc_cvr();
        data.calccvr = calccvr;
        data.rescvr = rescvr;
        cvrtype = type;
        for( i = 0; i < 9; i++ )
        {
            cvr_mmerr[i] = mmerr[i];
        }

        data.ncvr = data.nobs * 3;
    }
    return data.cvr;
}


void ldt_unused()
{
    DEBUG_PRINT(("LDT: ldt_unused"));
    if( !tgt )
    {
        report_error( "ldt_unused" );
        return;
    }
    tgt->unused = REJECT_OBS_BIT;
}


void ldt_obs_id( int id )
{
    if( !tgt )
    {
        report_error( "ldt_classification" );
        return;
    }
    tgt->id = id;
}

void ldt_classification( int class_id, int name_id )
{
    classdata *cd;

    DEBUG_PRINT(("LDT: ldt_classification %d %d", (int) class_id, (int) name_id ));

    if( ! class_id ) return;

    if( !tgt )
    {
        report_error( "ldt_classification" );
        return;
    }

    cd = (classdata *) getclassdata();
    cd->class_id = class_id;
    cd->name_id = name_id;

    if( !tgt->nclass ) tgt->iclass = data.nclass;
    tgt->nclass++;
    data.nclass++;

    // Classifications that relate to other parameters...
    if( obsdatatype->distsf && class_id == coef_class_id[COEF_CLASS_DISTSF].idclass && od )
    {
        od->prm_id = get_coef_id( COEF_CLASS_DISTSF, name_id );
    }
    if( obsdatatype->brngref && class_id == coef_class_id[COEF_CLASS_BRNGREF].idclass && od )
    {
        od->prm_id = get_coef_id( COEF_CLASS_BRNGREF, name_id );
    }
    if( obsdatatype->refcoef && class_id == coef_class_id[COEF_CLASS_REFCOEF].idclass )
    {
        od->refcoef = get_coef_id( COEF_CLASS_REFCOEF, name_id );
    }
    if( obsdatatype->reffrm && class_id == coef_class_id[COEF_CLASS_REFFRM].idclass )
    {
        data.reffrm = get_coef_id( COEF_CLASS_REFFRM, name_id );
        reffrm_classification = name_id;
    }
}


void ldt_syserr( int syserr_id, double influence )
{
    syserrdata *sd;

    DEBUG_PRINT(("LDT: ldt_syserr %d  %.5lf", (int) syserr_id, influence));

    if( !tgt )
    {
        report_error( "ldt_syserr" );
        return;
    }

    sd = (syserrdata *) getsyserrdata();
    sd->prm_id = syserr_id;
    sd->influence = influence;

    if( !tgt->nsyserr ) tgt->isyserr = data.nsyserr;
    tgt->nsyserr++;
    data.nsyserr++;
}


void ldt_vecsyserr( int syserr_id, double influence[] )
{
    DEBUG_PRINT(("LDT: ldt_vecsyserr %d %.5lf %.5lf %.5lf", (int) syserr_id,
                 influence[0], influence[1], influence[2]));

    if( !tgt )
    {
        report_error( "ldt_vecsyserr" );
        return;
    }
    ldt_syserr( syserr_id, influence[0] );
    ldt_syserr( syserr_id, influence[1] );
    ldt_syserr( syserr_id, influence[2] );
}


void ldt_prefix_note( const char *note )
{
    long newnote;
    DEBUG_PRINT(("LDT: ldt_prefix_note %s",note));
    newnote = ldt_get_id( ID_NOTE, noteloc ? 1 : 0, note );
    if( !noteloc ) noteloc = newnote;
}

void ldt_note( const char *note )
{
    long newnote;
    DEBUG_PRINT(("LDT: ldt_note %s",note));

    if( !tgt )
    {
        report_error( "ldt_postfix_note" );
        return;
    }
    newnote = ldt_get_id( ID_NOTE, tgt->noteloc ? 1 : 0, note );
    if( !tgt->noteloc ) tgt->noteloc = newnote;
}

void ldt_cancel_data( void )
{
    DEBUG_PRINT(("LDT: ldt_cancel_data"));
    if( !data_cancelled )
    {
        data.nobs--;
        data_cancelled = 1;
    }
}

void ldt_cancel_inst( void )
{
    DEBUG_PRINT(("LDT: ldt_cancel_inst"));
    cancelled = 1;
}


void ldt_cancel_trgt( void )
{
    DEBUG_PRINT(("LDT: ldt_cancel_trgt"));
    data.nobs = trgt_init_nobs;
    trgt_cancelled = 1;
    data_cancelled = 1;
}

int ldt_data_cancelled( void )
{
    return cancelled || data_cancelled;
}

void ldt_end_data( void )
{
    DEBUG_PRINT(("LDT: ldt_end_data"));
    if( data.nobs > 0 && !cancelled )
    {
        check_data();

        /* Ensure pointers are correct after potential reallocations */

        switch( data.format )
        {

        case SD_OBSDATA:
            data.obs.odata = (obsdata *) datablock;
            break;

        case SD_VECDATA:
            data.obs.vdata = (vecdata *) datablock;
            if( data.cvr )
            {
                (*gpscvr_func)( &data, cvrtype, cvr_mmerr );
                clear_cvr( data.nobs * 3, data.calccvr );
                clear_cvr( data.nobs * 3, data.rescvr );
            }
            break;

        case SD_PNTDATA:
            data.obs.pdata = (pntdata *) datablock;
            break;

        default: report_error("ldt_end_data"); return;
        }

        data.clsf = classblock;
        data.syserr = syserrblock;

        if( data.reffrm == ID_UNDEFINED )
        {
            data.reffrm = 0;
        }
        if( usedata_func ) (*usedata_func)( &data );
    }
    data.nobs = 0;
}

