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
#include <string>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_src.h"
#include "util/errdef.h"
#include "util/datafile.h"
#include "util/fileutil.h"
#include "util/dstring.h"
#include "util/fieldscanner.hpp"

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

struct code_loc
{
    code_loc *next;
    std::string code;
    datafile_loc loc;
    bool hidden;

    /// Scratch storage for cfs_code_def()'s reconstructed line, owned by
    /// this code_loc (not a buffer shared across other codes) since it must
    /// outlive cfs_code_def()'s own return - see cfs_code_def(). A
    /// std::vector<char>, not a std::string, since cfs_code_def()'s callers
    /// write through it (parse_number()'s null-terminate-then-restore
    /// trick), which is undefined behavior on a std::string's own storage.
    std::vector<char> replacedLine;
};

/* Structure defining a coordinate system definition file */

struct crdsys_file_source
{
    std::unique_ptr<DATAFILE> df;
    code_loc *codes[CS_COORDSYS_COUNT] = {};
};

/* Add a new code */

static code_loc *add_code( crdsys_file_source *csf, int type, const std::string &code, bool hidden, const datafile_loc &loc )
{
    if( type < 0  || type >= CS_COORDSYS_COUNT )
    {
        return nullptr;
    }
    // Walk forward while the current slot holds a node, so `next` ends up
    // pointing at whichever pointer needs to become non-null to add one here
    // - the list head itself, or the last node's own next field.
    code_loc **next = &(csf->codes[type]);
    while( *next ) next=&((*next)->next);
    code_loc *newloc = new code_loc{ nullptr, code, loc, hidden, {} };
    (*next)=newloc;
    return newloc;
}

/// One '='-delimited segment of a coordinate system code's alias list, e.g.
/// "NZGD2000", "NZGD2000_20180701" and "(20180701)" in
/// "NZGD2000=NZGD2000_20180701=(20180701)".
struct CodeAlias
{
    std::string code;  ///< the alias code, with any hiding parentheses stripped
    bool hidden;        ///< true if the segment was wrapped in parentheses
};

/// Parses one alias segment, recognizing the "(code)" hidden-alias form. A
/// hidden alias is still fully valid and lookupable directly by name - it's
/// just suppressed from get_codes()'s general listing (e.g. a "pick a
/// coordinate system" dropdown), typically used for a legacy/internal
/// shorthand kept functional for backward compatibility.
static CodeAlias parse_code_alias( const std::string &segment )
{
    if( segment.size() >= 3 && segment.front() == '(' && segment.back() == ')' )
    {
        return CodeAlias{ segment.substr(1,segment.size()-2), true };
    }
    return CodeAlias{ segment, false };
}

static code_loc *add_codes( crdsys_file_source *csf, int type, std::string_view code, const datafile_loc &loc )
{
    code_loc *newloc=nullptr;
    std::vector<std::string> segments;
    /* Allow for codes with aliases as NZGD2000=NZGD2000_2010601 */
    boost::algorithm::split( segments, code, boost::algorithm::is_any_of("=") );
    for( const std::string &segment : segments )
    {
        CodeAlias alias = parse_code_alias( segment );
        if( alias.code.size() > 0 && alias.code.size() <= CRDSYS_CODE_LEN )
        {
            newloc=add_code( csf, type, alias.code, alias.hidden, loc );
        }
    }
    return newloc;
}

static code_loc *find_code_loc( crdsys_file_source *csf, int type, std::string_view code )
{
    code_loc *loc;
    if( type < 0  || type >= CS_COORDSYS_COUNT )
    {
        return 0;
    }
    loc=csf->codes[type];
    while( loc && ! boost::algorithm::iequals(loc->code,code) ) loc=loc->next;
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
        delete code;
        code = next;
    }
}

static void scan_coordsys_defs( crdsys_file_source *cfs )
{
    char type = CS_INVALID;
    if( !cfs->df ) return;
    while( cfs->df->read_record() == OK )
    {
        datafile_loc loc;
        cfs->df->save_loc( loc );
        input_string_def &is = cfs->df->input_string();
        auto field = is.scanner.checkAndRecoverQuotedValue( true, std::nullopt );
        if( ! field ) continue;
        const std::string_view code = *field;
        if( ! code.empty() && code.front() == '[' )
        {
            if( boost::algorithm::iequals(code,ELLIPSOID_TAG) ) type = CS_ELLIPSOID;
            else if( boost::algorithm::iequals(code,REFFRAME_TAG) ) type = CS_REF_FRAME;
            else if( boost::algorithm::iequals(code,COORDSYS_TAG ) ) type = CS_COORDSYS;
            else if( boost::algorithm::iequals(code,COORDSYS_NOTE_TAG ) ) type = CS_COORDSYS_NOTE;
            else if( boost::algorithm::iequals(code,REFFRAME_NOTE_TAG ) ) type = CS_REF_FRAME_NOTE;
            else if( boost::algorithm::iequals(code,VDATUM_TAG ) ) type = CS_VDATUM;
            else if( boost::algorithm::iequals(code,VDATUM_TAG2 ) ) type = CS_VDATUM;
            else type = CS_INVALID;
        }
        else if( type != CS_INVALID )
        {
            add_codes( cfs, type, code, loc );
            if( type == CS_COORDSYS_NOTE || type == CS_REF_FRAME_NOTE )
            {
                /* Notes can refer to multiple codes - get a complete list */
                while( (field = is.scanner.checkAndRecoverQuotedValue( true, std::nullopt )) )
                {
                    add_codes( cfs, type, *field, loc );
                }
                /* Notes continue to a line ending end_note ... */
                while( cfs->df->read_record() == OK )
                {
                    auto marker = cfs->df->input_string().scanner.checkAndRecoverQuotedValue( true, std::nullopt );
                    if( marker && boost::algorithm::iequals(*marker,END_NOTE_MARKER) ) break;
                }
            }
        }
    }
}


/* Load all codes defined in the coordinate system file */

static int get_codes( void *pcfs,
                      void (*addfunc)( int type, long id, std::string_view code, std::string_view desc ))
{
    crdsys_file_source *cfs = static_cast<crdsys_file_source *>( pcfs );
    long id;
    int type;
    code_loc *cl;

    if( !cfs ) return OK;

    for( type=0; type < CS_COORDSYS_COUNT; type++ )
    {
        id = 0;
        for( cl=cfs->codes[type]; cl; cl=cl->next )
        {
            if( ! cl->hidden )
            {
                cfs->df->reset_loc( cl->loc );
                input_string_def &instr = cfs->df->input_string();
                instr.scanner.checkAndRecoverQuotedValue( true, std::nullopt ); // skip the code field
                auto field = instr.scanner.checkAndRecoverQuotedValue( true, std::nullopt );
                (*addfunc)( type, id, cl->code, field ? *field : std::string_view( "(unnamed)" ) );
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

static std::optional<std::reference_wrapper<input_string_def>> cfs_code_def( crdsys_file_source *cfs, long id, int type, std::string_view code )
{
    code_loc *cl;
    if( id == CS_ID_UNAVAILABLE )
    {
        cl = find_code_loc( cfs, type, code );
    }
    else
    {
        cl = get_code_loc( cfs, type, id );
    }
    if( !cl ) return std::nullopt;
    cfs->df->reset_loc( cl->loc );
    input_string_def &instr = cfs->df->input_string();

    // The line's raw first field may be a combined alias list, e.g.
    // "NZGD2000=NZGD2000_20180701=(20180701)" - cl->code is already the one
    // resolved alias (set once, at file-scan time by add_codes()). Replace
    // the raw combined token with the resolved code before handing the line
    // to whatever re-parses the definition, by building a fresh line owned
    // by cl itself (stable for as long as cl is, unlike a buffer shared
    // across other codes) rather than mutating the DATAFILE's own record
    // buffer in place - nothing downstream depends on the replacement
    // landing at the original token's byte offset.
    instr.scanner.next();
    std::string newLine = cl->code + std::string(instr.scanner.remainder());
    cl->replacedLine.assign( newLine.begin(), newLine.end() );
    cl->replacedLine.push_back( '\0' );
    instr.scanner = FieldScanner( std::string_view(cl->replacedLine.data(), cl->replacedLine.size()-1) );

    return std::ref(instr);
}


static int read_ellipsoid_def( crdsys_file_source *cfs, long id, std::string_view code, ellipsoid**el )
{
    *el = nullptr;
    auto instr = cfs_code_def( cfs, id, CS_ELLIPSOID, code );
    if( !instr ) return MISSING_DATA;
    *el = parse_ellipsoid_def( instr->get(), 0 );
    return *el ? OK : INVALID_DATA;
}

static int get_ellipsoid( void *pcfs, long id, std::string_view code, ellipsoid**el )
{
    return read_ellipsoid_def( static_cast<crdsys_file_source *>( pcfs ), id, code, el );
}

static crdsys_file_source *input_cfs;
static ellipsoid *ellipsoid_from_code( std::string_view code)
{
    ellipsoid *el;
    int sts;
    sts = read_ellipsoid_def( input_cfs, CS_ID_UNAVAILABLE, code, &el );
    if( sts != OK ) el = nullptr;
    return el;
}

static ref_frame *ref_frame_from_code( std::string_view code, int loadref );

static int read_ref_frame_def( crdsys_file_source *cfs, long id, std::string_view code, ref_frame **rf, int loadref)
{
    *rf = nullptr;
    auto instr = cfs_code_def( cfs, id, CS_REF_FRAME, code );
    if( !instr ) return MISSING_DATA;
    input_cfs = cfs;
    *rf = parse_ref_frame_def( instr->get(), ellipsoid_from_code, ref_frame_from_code, 0, loadref );
    return *rf ? OK : INVALID_DATA;
}

static ref_frame *ref_frame_from_code( std::string_view code, int loadref )
{
    ref_frame *rf;
    int sts;
    sts = read_ref_frame_def( input_cfs, CS_ID_UNAVAILABLE, code, &rf, loadref );
    if( sts != OK ) rf = nullptr;
    return rf;
}

static int get_ref_frame_cs( void *pcfs, long id, std::string_view code, ref_frame **rf )
{
    return read_ref_frame_def( static_cast<crdsys_file_source *>( pcfs ), id, code, rf, 1 );
}

static int get_coordsys( void *pcfs, long id, std::string_view code, coordsys **cs )
{
    crdsys_file_source *cfs = static_cast<crdsys_file_source *>( pcfs );
    *cs = nullptr;
    auto instr = cfs_code_def( cfs, id, CS_COORDSYS, code );
    if( !instr ) return MISSING_DATA;
    input_cfs = cfs;
    *cs = parse_coordsys_def( instr->get(), ref_frame_from_code );
    return *cs ? OK : INVALID_DATA;
}

static int read_vdatum_def( crdsys_file_source *cfs, long id, std::string_view code, vdatum **hrs );

static vdatum *vdatum_from_code( std::string_view code, int )
{
    vdatum *hrf;
    int sts;
    sts = read_vdatum_def( input_cfs, CS_ID_UNAVAILABLE, code, &hrf );
    if( sts != OK ) hrf = nullptr;
    return hrf;
}

static int read_vdatum_def( crdsys_file_source *cfs, long id, std::string_view code, vdatum **hrs )
{
    *hrs = nullptr;
    auto instr = cfs_code_def( cfs, id, CS_VDATUM, code );
    if( !instr ) return MISSING_DATA;
    input_cfs = cfs;
    *hrs = parse_vdatum_def( instr->get(), ref_frame_from_code, vdatum_from_code );
    return *hrs ? OK : INVALID_DATA;
}

static int get_vdatum( void *pcfs, long id, std::string_view code, vdatum **hrs )
{
    return read_vdatum_def( static_cast<crdsys_file_source *>( pcfs ), id, code, hrs );
}

static int get_csdef_notes( void *pcfs, int type, std::string_view code, void *sptr, output_string_func puttext )
{
    crdsys_file_source *cfs = static_cast<crdsys_file_source *>( pcfs );

    if( type != CS_COORDSYS_NOTE && type != CS_REF_FRAME_NOTE ) return INVALID_DATA;

    code_loc *cl = find_code_loc( cfs, type, code );
    if( ! cl ) return INVALID_DATA;

    cfs->df->reset_loc( cl->loc );

    while( cfs->df->read_record() == OK )
    {
        input_string_def &instr = cfs->df->input_string();
        if( test_next_string_field( instr.scanner, END_NOTE_MARKER ) ) break;
        (*puttext)( instr.scanner.remainder(), sptr );
        (*puttext)( "\n", sptr );
    }
    return OK;
}

static std::optional<std::string> get_csfile( void *pcfs, const std::string &filename, const std::string &extension )
{
    crdsys_file_source *cfs = static_cast<crdsys_file_source *>( pcfs );
    return find_relative_file( cfs->df->file_name(), filename, extension );
}

static int delete_crdsys_file_source( void *pcfs )
{
    crdsys_file_source *cfs = static_cast<crdsys_file_source *>( pcfs );
    for( int type=0; type<CS_COORDSYS_COUNT; type++ ) delete_code_locs(&(cfs->codes[type]));
    delete cfs;
    return 0;
}

static int create_crdsys_file_source( std::string_view filename )
{
    crdsys_source_def csd;

    const int dfreclen = DATAFILE::default_reclen( MAXRECLEN );
    std::unique_ptr<DATAFILE> df = DATAFILE::open( filename, "coordinate systems definition" );
    DATAFILE::default_reclen( dfreclen );
    if( !df ) return FILE_OPEN_ERROR;
    crdsys_file_source *cfs = new crdsys_file_source;
    cfs->df = std::move( df );
    scan_coordsys_defs( cfs );
    csd.data = cfs;
    csd.getcsfile = get_csfile;
    csd.getel = get_ellipsoid;
    csd.getrf = get_ref_frame_cs;
    csd.getcs = get_coordsys;
    csd.gethrs = get_vdatum;
    csd.getnotes = get_csdef_notes;
    csd.getcodes = get_codes;
    csd.delsource = delete_crdsys_file_source;
    register_crdsys_source( csd );
    return OK;
}


int install_crdsys_file( std::string_view filename )
{
    return create_crdsys_file_source( filename );
}

