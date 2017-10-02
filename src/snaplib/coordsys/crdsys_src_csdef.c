#include "snapconfig.h"
/* crdsysfl.c:  Reading of coordinate system definitions from file of
   definitions.

NOTE 1: This should probably be modified.  At present it keep the data file
open indefinitely.  It would make more sense to close the file after each
item has been read.  However to be rigorous it would then need to check
that the file had not been modified in the mean time - tricky .

*/



/*
   $Log: crdsysfl.c,v $
   Revision 1.1  1995/12/22 16:36:03  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_src.h"
#include "util/chkalloc.h"
#include "util/errdef.h"
#include "util/datafile.h"
#include "util/fileutil.h"
#include "util/dstring.h"
#include "util/linklist.h"

#define MAXRECLEN 512

/* static char *crdsys_fname = NULL; */
/* static DATAFILE *csf=NULL;        */

#define REFFRAME_TAG  "[reference_frames]"
#define ELLIPSOID_TAG "[ellipsoids]"
#define COORDSYS_TAG  "[coordinate_systems]"
#define REFFRAME_NOTE_TAG  "[reference_frame_notes]"
#define COORDSYS_NOTE_TAG  "[coordinate_system_notes]"
#define VDATUM_TAG  "[vertical_datums]"
#define VDATUM_TAG2  "[vdatumerence_surfaces]"

#define END_NOTE_MARKER "end_note"

/* Structure used to record reference frames and ellipsoids already
   passed when loading a coordinate system */

typedef struct code_loc_s
{
    struct code_loc_s *next;
    char *code;
    datafile_loc loc;
    char hidden;
} code_loc;

/* Structure defining a coordinate system definition file */

typedef struct
{
    DATAFILE *df;
    code_loc *codes[CS_COORDSYS_COUNT];
} crdsys_file_source;

/* Add a new code */

static code_loc *add_code( crdsys_file_source *csf, int type, const char *code, int clen, char hidden, datafile_loc *loc )
{
    code_loc **next;
    code_loc *newloc;
    char *newcode;
    if( type < 0  || type >= CS_COORDSYS_COUNT )
    {
        return 0;
    }
    next = &(csf->codes[type]);
    while( *next ) next=&((*next)->next);
    newloc = (code_loc *) check_malloc( sizeof(code_loc) + clen + 1 );
    newcode = ((char *) newloc)+sizeof(code_loc);
    strncpy(newcode,code,clen);
    newcode[clen] = 0;
    newloc->next=0;
    newloc->code=newcode;
    newloc->hidden=hidden;
    memcpy(&(newloc->loc),loc,sizeof(datafile_loc));
    (*next)=newloc;
    return newloc;
}

static code_loc *add_codes( crdsys_file_source *csf, int type, const char *code, datafile_loc *loc )
{
    const char *cptr, *eptr;
    int clen;
    char hidden;
    code_loc *newloc=0;

    eptr=code;
    while( *eptr )
    {
        cptr=eptr;
        /* Allow for codes with aliases as NZGD2000=NZGD2000_2010601 */
        /* Allow for hidden codes with (20170601) */
        while( *eptr && *eptr != '=' ) eptr++;
        if( eptr > cptr )
        {
            clen=eptr-cptr;
            hidden=0;
            if( *cptr == '(' && clen >= 3 && cptr[clen-1] == ')' )
            {
                cptr++;
                clen -= 2;
                hidden=1;
            }
            if( clen > 0 && clen <= CRDSYS_CODE_LEN )
            {
                newloc=add_code( csf, type, cptr, clen, hidden, loc );
            }
        }
        if( *eptr ) eptr++;
    }
    return newloc;
}

static code_loc *find_code_loc( crdsys_file_source *csf, int type, const char *code )
{
    code_loc *loc;
    if( type < 0  || type >= CS_COORDSYS_COUNT )
    {
        return 0;
    }
    loc=csf->codes[type];
    while( loc && _stricmp(loc->code, code) != 0 ) loc=loc->next;
    return loc;
}

static void delete_code_locs( code_loc **codes )
{
    code_loc *code;
    code_loc *next;
    code = (*codes);
    (*codes)=0;
    while( code )
    {
        next = code->next;
        check_free( code );
        code = next;
    }
}

static void scan_coordsys_defs( crdsys_file_source *cfs )
{
    char type = CS_INVALID;
    if( !cfs->df ) return;
    while( df_read_data_file( cfs->df ) == OK )
    {
        input_string_def *is;
        datafile_loc loc;
        char code[255];
        df_save_data_file_loc( cfs->df, &loc );
        is =  df_input_string( cfs->df );
        if( next_string_field( is ,code, 255 ) != OK ) continue;
        if( code[0] == '[' )
        {
            if( _stricmp(code,ELLIPSOID_TAG) == 0 ) type = CS_ELLIPSOID;
            else if( _stricmp(code,REFFRAME_TAG) == 0 ) type = CS_REF_FRAME;
            else if( _stricmp(code,COORDSYS_TAG ) == 0 ) type = CS_COORDSYS;
            else if( _stricmp(code,COORDSYS_NOTE_TAG ) == 0 ) type = CS_COORDSYS_NOTE;
            else if( _stricmp(code,REFFRAME_NOTE_TAG ) == 0 ) type = CS_REF_FRAME_NOTE;
            else if( _stricmp(code,VDATUM_TAG ) == 0 ) type = CS_VDATUM;
            else if( _stricmp(code,VDATUM_TAG2 ) == 0 ) type = CS_VDATUM;
            else type = CS_INVALID;
        }
        else if( type != CS_INVALID )
        {
            add_codes( cfs, type, code, &loc );
            if( type == CS_COORDSYS_NOTE || type == CS_REF_FRAME_NOTE )
            {
                /* Notes can refer to multiple codes - get a complete list */
                while( next_string_field( is ,code, CRDSYS_CODE_LEN+1 ) == OK )
                {
                    add_codes( cfs, type, code, &loc );
                }
                /* Notes continue to a line ending end_note ... */
                while( df_read_data_file( cfs->df ) == OK )
                {
                    is =  df_input_string( cfs->df );
                    if( test_next_string_field( is, END_NOTE_MARKER )) break;
                }
            }
        }
    }
}


/* Load all codes defined in the coordinate system file */

static int get_codes( void *pcfs,
                      void (*addfunc)( int type, long id, const char *code, const char *desc ))
{
    crdsys_file_source *cfs = (crdsys_file_source *) pcfs;
    char name[CRDSYS_NAME_LEN];
    long id;
    int type;
    code_loc *cl;
    input_string_def *instr;

    if( !cfs ) return OK;

    for( type=0; type < CS_COORDSYS_COUNT; type++ )
    {
        id = 0;
        for( cl=cfs->codes[type]; cl; cl=cl->next )
        {
            if( ! cl->hidden )
            {
                df_reset_data_file_loc( cfs->df, &cl->loc );
                instr = df_input_string( cfs->df );
                skip_string_field( instr );
                if( next_string_field( instr, name, CRDSYS_NAME_LEN ) != OK)
                {
                    strcpy(name,"(unnamed)");
                }
                (*addfunc)((int) type, id, cl->code, name );
            }
            id++;
        }
    }
    return OK;
}

static code_loc *get_code_loc( crdsys_file_source *cfs, int type, long id )
{
    code_loc *cl;
    if( !cfs ) return 0;
    if( type < 0 || type >= CS_COORDSYS_COUNT ) return 0;
    if( id < 0 ) return 0;
    cl=cfs->codes[type];
    while( cl && id--) cl=cl->next;
    return cl;
}

static input_string_def *cfs_code_def( crdsys_file_source *cfs, long id, int type, const char *code )
{
    code_loc *cl;
    input_string_def *instr;
    if( id == CS_ID_UNAVAILABLE )
    {
        cl = find_code_loc( cfs, type, code );
    }
    else
    {
        cl = get_code_loc( cfs, type, id );
    }
    if( !cl ) return NULL;
    df_reset_data_file_loc( cfs->df, &cl->loc );
    instr = df_input_string( cfs->df );
    replace_next_field(instr,cl->code);
    return instr;
}


static int get_ellipsoid( void *pcfs, long id, const char *code, ellipsoid**el )
{
    crdsys_file_source *cfs = (crdsys_file_source *) pcfs;
    input_string_def *instr;
    *el = NULL;
    instr = cfs_code_def( cfs, id, CS_ELLIPSOID, code );
    if( !instr ) return MISSING_DATA;
    *el = parse_ellipsoid_def( instr, 0 );
    return *el ? OK : INVALID_DATA;
}

static crdsys_file_source *input_cfs;
static ellipsoid *ellipsoid_from_code( const char *code)
{
    ellipsoid *el;
    int sts;
    sts = get_ellipsoid( input_cfs, CS_ID_UNAVAILABLE, code, &el );
    if( sts != OK ) el = NULL;
    return el;
}

static ref_frame *ref_frame_from_code( const char *code, int loadref );

static int get_ref_frame( void *pcfs, long id, const char *code, ref_frame **rf, int loadref)
{
    crdsys_file_source *cfs = (crdsys_file_source *) pcfs;
    input_string_def *instr;
    *rf = NULL;
    instr = cfs_code_def( cfs, id, CS_REF_FRAME, code );
    if( !instr ) return MISSING_DATA;
    input_cfs = cfs;
    *rf = parse_ref_frame_def( instr, ellipsoid_from_code, ref_frame_from_code, 0, loadref );
    return *rf ? OK : INVALID_DATA;
}

static ref_frame *ref_frame_from_code( const char *code, int loadref )
{
    ref_frame *rf;
    int sts;
    sts = get_ref_frame( input_cfs, CS_ID_UNAVAILABLE, code, &rf, loadref );
    if( sts != OK ) rf = NULL;
    return rf;
}

static int get_ref_frame_cs( void *pcfs, long id, const char *code, ref_frame **rf )
{
    return get_ref_frame( pcfs, id, code, rf, 1 );
}

static int get_coordsys( void *pcfs, long id, const char *code, coordsys **cs )
{
    crdsys_file_source *cfs = (crdsys_file_source *) pcfs;
    input_string_def *instr;
    *cs = NULL;
    instr = cfs_code_def( cfs, id, CS_COORDSYS, code );
    if( !instr ) return MISSING_DATA;
    input_cfs = cfs;
    *cs = parse_coordsys_def( instr, ref_frame_from_code );
    if( *cs )
    {
        char *fn = df_file_name( cfs->df );
        char *source = (char *) check_malloc(strlen(fn)+6);
        strcpy(source,"file:");
        strcat(source,fn);
        (*cs)->source = source;
    }
    return *cs ? OK : INVALID_DATA;
}

static int get_vdatum( void *pcfs, long id, const char *code, vdatum **hrs );

static vdatum *vdatum_from_code( const char *code, int loadref )
{
    vdatum *hrf;
    int sts;
    sts = get_vdatum( input_cfs, CS_ID_UNAVAILABLE, code, &hrf );
    if( sts != OK ) hrf = NULL;
    return hrf;
}

static int get_vdatum( void *pcfs, long id, const char *code, vdatum **hrs )
{
    crdsys_file_source *cfs = (crdsys_file_source *) pcfs;
    input_string_def *instr;
    *hrs = NULL;
    instr = cfs_code_def( cfs, id, CS_VDATUM, code );
    if( !instr ) return MISSING_DATA;
    input_cfs = cfs;
    *hrs = parse_vdatum_def( instr, ref_frame_from_code, vdatum_from_code );
    if( *hrs )
    {
        char *fn = df_file_name( cfs->df );
        char *source = (char *) check_malloc(strlen(fn)+6);
        strcpy(source,"file:");
        strcat(source,fn);
        (*hrs)->source = source;
    }
    return *hrs ? OK : INVALID_DATA;
}

static int get_csdef_notes( void *pcfs, int type, const char *code, void *sptr, int (*puttext)(const char *note, void *sptr ))
{
    crdsys_file_source *cfs = (crdsys_file_source *) pcfs;
    code_loc *cl;
    input_string_def *instr;

    if( type != CS_COORDSYS_NOTE && type != CS_REF_FRAME_NOTE ) return INVALID_DATA;

    cl = find_code_loc( cfs, type, code );
    if( ! cl ) return INVALID_DATA;

    df_reset_data_file_loc( cfs->df, &cl->loc );
    /* df_read_data_file( cfs->df ); */

    while( df_read_data_file( cfs->df ) == OK )
    {
        const char *text;
        instr = df_input_string( cfs->df );
        if( test_next_string_field( instr, END_NOTE_MARKER )) break;
        text = unread_string( instr );
        (*puttext)( text, sptr );
        (*puttext)( "\n", sptr );
    }
    return OK;
}

static int delete_crdsys_file_source( void *pcfs )
{
    int type;
    crdsys_file_source *cfs = (crdsys_file_source *) pcfs;
    df_close_data_file( cfs->df );
    for( type=0; type<CS_COORDSYS_COUNT; type++ ) delete_code_locs(&(cfs->codes[type]));
    check_free( cfs );
    return 0;
}

static int create_crdsys_file_source( const char *filename )
{
    DATAFILE *df;
    crdsys_file_source *cfs;
    crdsys_source_def csd;
    int dfreclen;
    int type;

    dfreclen = df_data_file_default_reclen( MAXRECLEN );
    df = df_open_data_file( filename, "coordinate systems definition" );
    df_data_file_default_reclen( dfreclen );
    if( !df ) return FILE_OPEN_ERROR;
    cfs = (crdsys_file_source *) check_malloc( sizeof( crdsys_file_source ));
    cfs->df = df;
    for( type=0; type<CS_COORDSYS_COUNT; type++ ) cfs->codes[type]=0;
    scan_coordsys_defs( cfs );
    csd.data = cfs;
    csd.getel = get_ellipsoid;
    csd.getrf = get_ref_frame_cs;
    csd.getcs = get_coordsys;
    csd.gethrs = get_vdatum;
    csd.getnotes = get_csdef_notes;
    csd.getcodes = get_codes;
    csd.delsource = delete_crdsys_file_source;
    register_crdsys_source( &csd );
    return OK;
}


int install_crdsys_file( const char *filename )
{
    return create_crdsys_file_source( filename );
}

