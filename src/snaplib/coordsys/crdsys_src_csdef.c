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

static char rcsid[]="$Id: crdsysfl.c,v 1.1 1995/12/22 16:36:03 CHRIS Exp $";

/* Structure used to record reference frames and ellipsoids already
   passed when loading a coordinate system */

#define CODE_BLOCK_SIZE 20

typedef struct
{
    char type;
    char code[CRDSYS_CODE_LEN+1];
    datafile_loc loc;
} code_loc;

typedef struct code_loc_block_s
{
    struct code_loc_block_s *next;
    int ncode;
    code_loc codes[CODE_BLOCK_SIZE];
} code_loc_block;


/* Structure defining a coordinate system definition file */

typedef struct
{
    DATAFILE *df;
    code_loc_block *codes;
} crdsys_file_source;

/* Management of an expandable array of codes */

static code_loc *get_code_loc( code_loc_block *codes, int ncode )
{
    if( !codes ) return NULL;
    while( ncode >= codes->ncode )
    {
        if( !codes->next ) return NULL;
        ncode -= codes->ncode;
        codes = codes->next;
    }
    if( ncode < 0 ) return NULL;
    return codes->codes + ncode;
}

static code_loc *find_code_loc( code_loc_block *codes, const char type, const char *code )
{
    for( ; codes; codes = codes->next )
    {
        code_loc *cl;
        int nc;
        for( cl = codes->codes, nc=codes->ncode; nc--; cl++)
        {
            if( cl->type == type && _stricmp(cl->code,code) == 0 ) return cl;
        }
    }
    return NULL;
}

static code_loc *add_code_loc( code_loc_block **codes )
{
    /* Find the last block */
    while( (*codes) )
    {
        if( (*codes)->ncode < CODE_BLOCK_SIZE ) break;
        codes = &((*codes)->next);
    }
    if( !(*codes) )
    {
        (*codes) = (code_loc_block *) check_malloc( sizeof(code_loc_block) );
        (*codes)->next = NULL;
        (*codes)->ncode = 0;
    }
    return (*codes)->codes + ((*codes)->ncode++);
}

static void delete_code_locs( code_loc_block *codes )
{
    code_loc_block *next;
    while( codes )
    {
        next = codes->next;
        check_free( codes );
        codes = next;
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
        char code[CRDSYS_CODE_LEN+1];
        df_save_data_file_loc( cfs->df, &loc );
        is =  df_input_string( cfs->df );
        if( next_string_field( is ,code, CRDSYS_CODE_LEN+1 ) != OK ) continue;
        if( code[0] == '[' )
        {
            if( _stricmp(code,ELLIPSOID_TAG) == 0 ) type = CS_ELLIPSOID;
            else if( _stricmp(code,REFFRAME_TAG) == 0 ) type = CS_REF_FRAME;
            else if( _stricmp(code,COORDSYS_TAG ) == 0 ) type = CS_COORDSYS;
            else type = CS_INVALID;
        }
        else if( type != CS_INVALID )
        {
            code_loc *cl;
            cl = add_code_loc( &cfs->codes );
            cl->type = type;
            strcpy( cl->code, code );
            memcpy( &cl->loc, &loc, sizeof( datafile_loc ) );
        }
    }
}


static input_string_def *cfs_code_def( crdsys_file_source *cfs, long id, const char type, const char *code )
{
    code_loc *cl;
    input_string_def *instr;
    if( id == CS_ID_UNAVAILABLE )
    {
        cl = find_code_loc( cfs->codes, type, code );
    }
    else
    {
        cl = get_code_loc( cfs->codes, (int) id );
    }
    if( !cl ) return NULL;
    df_reset_data_file_loc( cfs->df, &cl->loc );
    instr = df_input_string( cfs->df );
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


/* Load all codes defined in the coordinate system file */

static int get_codes( void *pcfs,
                      void (*addfunc)( int type, long id, const char *code, const char *desc ))
{
    crdsys_file_source *cfs = (crdsys_file_source *) pcfs;
    char name[CRDSYS_NAME_LEN];
    long id;
    code_loc_block *cb;
    code_loc *cl;
    input_string_def *instr;

    if( !cfs ) return OK;

    id = 0;
    for( cb = cfs->codes; cb; cb=cb->next )
    {
        int nc;
        for( nc = cb->ncode, cl = cb->codes; nc--; cl++, id++ )
        {
            df_reset_data_file_loc( cfs->df, &cl->loc );
            instr = df_input_string( cfs->df );
            skip_string_field( instr );
            if( next_string_field( instr, name, CRDSYS_NAME_LEN ) != OK)
            {
                strcpy(name,"(unnamed)");
            }
            (*addfunc)((int) cl->type, id, cl->code, name );
        }
    }
    return OK;
}

static int delete_crdsys_file_source( void *pcfs )
{
    crdsys_file_source *cfs = (crdsys_file_source *) pcfs;
    df_close_data_file( cfs->df );
    delete_code_locs( cfs->codes );
    check_free( cfs );
    return 0;
}

static int create_crdsys_file_source( const char *filename )
{
    DATAFILE *df;
    crdsys_file_source *cfs;
    crdsys_source_def csd;
    int dfreclen;

    dfreclen = df_data_file_default_reclen( MAXRECLEN );
    df = df_open_data_file( filename, "coordinate systems definition" );
    df_data_file_default_reclen( dfreclen );
    if( !df ) return FILE_OPEN_ERROR;
    cfs = (crdsys_file_source *) check_malloc( sizeof( crdsys_file_source ));
    cfs->df = df;
    cfs->codes = NULL;
    scan_coordsys_defs( cfs );
    csd.data = cfs;
    csd.getel = get_ellipsoid;
    csd.getrf = get_ref_frame_cs;
    csd.getcs = get_coordsys;
    csd.getcodes = get_codes;
    csd.delsource = delete_crdsys_file_source;
    register_crdsys_source( &csd );
    return OK;
}


int install_crdsys_file( const char *filename )
{
    return create_crdsys_file_source( filename );
}

