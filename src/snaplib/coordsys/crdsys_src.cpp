#include "snapconfig.h"
/* crdsysd0.c:  Routines to manage coordinate system definitions. */
/* The definitions are held in a data structure which includes a function
   pointers.  This is done to avoid linking in, say the coordinate system
	file handler, when we only want to use fixed definitions.  It also
	facilitates adding new sources of coordinate system definitions. */


/*
   $Log: crdsysd0.c,v $
   Revision 1.1  1995/12/22 16:28:07  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <optional>
#include <string>
#include <string_view>
#include "util/snapctype.h"

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_src.h"
#include "util/chkalloc.h"
#include "util/dateutil.h"
#include "util/errdef.h"


static crdsys_source_def *sources = NULL;
static int update_id = 0;
static char csfullcode[CRDCNV_CODE_LEN+1];

crdsys_source_def *crdsys_sources()
{
    return sources;
}

int crdsys_source_update()
{
    return update_id;
}


void register_crdsys_source( crdsys_source_def *csd )
{
    csd->next = sources;
    sources = (crdsys_source_def *) check_malloc( sizeof( crdsys_source_def ) );
    memcpy( sources, csd, sizeof( crdsys_source_def ) );
    update_id++;
}

void uninstall_crdsys_lists( void )
{
    crdsys_source_def *csd;
    update_id++;
    while( sources )
    {
        csd = sources;
        sources = sources->next;
        if( csd->delsource ) (*csd->delsource)( csd->data );
        check_free( csd );
    }
}


ref_frame * load_ref_frame( const char *code )
{
    crdsys_source_def *csd;
    int sts;
    ref_frame *rf = NULL;

    for( sts = MISSING_DATA, csd = sources;
            sts == MISSING_DATA && csd;
            csd = csd->next ) if( csd->getrf )
        {

            sts = (*csd->getrf)( csd->data, CS_ID_UNAVAILABLE, code, &rf );
        }

    if( sts == MISSING_DATA )
    {
        char errmsg[80];
        sprintf(errmsg,"Reference frame %.20s is not defined",code);
        handle_error(INVALID_DATA,errmsg,nullptr);
    }
    return rf;
}


ellipsoid * load_ellipsoid( const char *code )
{
    crdsys_source_def *csd;
    int sts;
    ellipsoid *el= NULL;

    for( sts = MISSING_DATA, csd = sources;
            sts == MISSING_DATA && csd;
            csd = csd->next ) if( csd->getel )
        {

            sts = (*csd->getel)( csd->data, CS_ID_UNAVAILABLE, code, &el );
        }
    if( sts == MISSING_DATA )
    {
        char errmsg[80];
        sprintf(errmsg,"Ellipsoid %.20s is not defined",code);
        handle_error(INVALID_DATA,errmsg,nullptr);
    }

    return el;
}

int parse_crdsys_epoch( const char *epochstr, double *epoch )
{
    *epoch=snap_datetime_parse(epochstr,0);
    if( ! *epoch ) return 0;
    *epoch = date_as_year(*epoch);
    return 1;
}

/* Coordinate systems defined by code, optionally followed by @epoch, where
   epoch is either a decimal year number (eg 2007.5) or "now" */

coordsys * load_coordsys( std::string_view code )
{
    crdsys_source_def *csd;
    double epoch = 0;
    int sts;

    vdatum *hrs=nullptr;
    coordsys *cs= nullptr;
    ref_frame *rf=nullptr;

    /* Code format is  CSCODE(DATUMCODE)/HRSCODE@epoch */

    const std::string_view::size_type endcode = code.find_first_of( "(/@" );
    const std::string_view cscode = code.substr( 0, endcode );
    std::string_view rest =
        endcode == std::string_view::npos ? std::string_view() : code.substr( endcode );
    std::optional<std::string_view> dtmcode;
    std::optional<std::string_view> hrscode;
    std::optional<std::string_view> epochstr;

    // An unclosed "(" is left in rest, which is reported as invalid below
    if( ! rest.empty() && rest.front() == '(' )
    {
        const std::string_view::size_type close = rest.find( ')' );
        if( close != std::string_view::npos )
        {
            dtmcode = rest.substr( 1, close-1 );
            rest.remove_prefix( close+1 );
        }
    }
    if( ! rest.empty() && rest.front() == '/' )
    {
        rest.remove_prefix( 1 );
        const std::string_view::size_type at = rest.find( '@' );
        hrscode = rest.substr( 0, at );
        rest = at == std::string_view::npos ? std::string_view() : rest.substr( at );
    }
    if( ! rest.empty() && rest.front() == '@' )
    {
        epochstr = rest.substr( 1 );
        rest = std::string_view();
    }

    /* Invalid coordinate system definition */
    if( ! rest.empty() )
    {
        const std::string errmsg = "Invalid coordinate definition (" +
            std::string( rest.substr( 0, 40 ) ) + ") in " + std::string( code.substr( 0, 40 ) );
        handle_error(INVALID_DATA,errmsg.c_str(),nullptr);
        return nullptr;
    }

    /* Check epoch */
    if( epochstr && ! parse_crdsys_epoch( std::string( *epochstr ).c_str(), &epoch ) )
    {
        const std::string errmsg = "Invalid coordinate system epoch in " + std::string( code.substr( 0, 40 ) );
        handle_error(INVALID_DATA,errmsg.c_str(),nullptr);
        return nullptr;
    }

    /* Check length of coordinate system code */
    if( cscode.empty() || cscode.size() > CRDSYS_CODE_LEN )
    {
        const std::string errmsg = "Invalid coordinate system code in " + std::string( code.substr( 0, 40 ) );
        handle_error(INVALID_DATA,errmsg.c_str(),nullptr);
        return nullptr;
    }

    /* Check alternative reference frame */
    if( dtmcode )
    {
        if( dtmcode->size() > CRDSYS_CODE_LEN )
        {
            const std::string errmsg = "Invalid alternative ref frame code in " + std::string( code.substr( 0, 40 ) );
            handle_error(INVALID_DATA,errmsg.c_str(),nullptr);
            return nullptr;
        }
        else if( ! dtmcode->empty() )
        {
            rf=load_ref_frame( std::string( *dtmcode ).c_str() );
            if( ! rf ) return nullptr;
        }
    }

    /* Check vertical datum */
    if( hrscode )
    {
        if( hrscode->size() > CRDSYS_CODE_LEN )
        {
            const std::string errmsg = "Invalid height system code in " + std::string( code.substr( 0, 40 ) );
            handle_error(INVALID_DATA,errmsg.c_str(),nullptr);
            return nullptr;
        }
        else if( ! hrscode->empty() )
        {
            hrs=load_vdatum( std::string( *hrscode ).c_str() );
            if( ! hrs )
            {
                delete rf;
                return nullptr;
            }
        }
    }

    for( sts = MISSING_DATA, csd = sources;
            sts == MISSING_DATA && csd;
            csd = csd->next ) if( csd->getcs )
        {

            sts = (*csd->getcs)( csd->data, CS_ID_UNAVAILABLE, cscode, &cs );
        }

    if( sts == MISSING_DATA )
    {
        const std::string errmsg = "Coordinate system " + std::string( cscode ) + " is not defined";
        handle_error(INVALID_DATA,errmsg.c_str(),nullptr);
    }


    if( cs ) 
    {
        if( rf )
        {
            set_coordsys_ref_frame( cs, rf );
            rf=0;
        }

        if( hrs )
        {
            if( ! coordsys_vdatum_compatible( cs, hrs ) )
            {
                char errmsg[100];
                sprintf(errmsg,"Vertical datum %.20s not compatible with coordinate system %.20s",
                        hrs->code.c_str(), cs->code.c_str() );
                handle_error( INVALID_DATA, errmsg, nullptr );
                delete cs;
                delete hrs;
                return nullptr;
            }
            set_coordsys_vdatum( cs, hrs );
        }
        define_deformation_model_epoch(cs,epoch);
    }
    else
    {
        delete hrs;
        delete rf;
    }
    return cs;
}

const char *coordsys_load_code( coordsys *cs )
{
    strncpy( csfullcode, cs->code.c_str(), CRDCNV_CODE_LEN );
    if( cs->setrf && cs->rf &&
            cs->rf->code.size()+strlen(csfullcode)+2 < CRDCNV_CODE_LEN )
    {
        strcat(csfullcode,"(");
        strcat(csfullcode,cs->rf->code.c_str());
        strcat(csfullcode,")");
        
    }
    if( cs->hrs
            && cs->hrs->code.size()+strlen(csfullcode)+1 < CRDCNV_CODE_LEN
      )
    {
        strcat(csfullcode,"/");
        strcat(csfullcode,cs->hrs->code.c_str());
    }
    return csfullcode;
}

vdatum * load_vdatum( const char *code )
{
    crdsys_source_def *csd;
    int sts;
    vdatum *hrs= NULL;

    for( sts = MISSING_DATA, csd = sources;
            sts == MISSING_DATA && csd;
            csd = csd->next ) if( csd->gethrs )
        {

            sts = (*csd->gethrs)( csd->data, CS_ID_UNAVAILABLE, code, &hrs );
        }

    if( sts == MISSING_DATA )
    {
        char errmsg[80];
        sprintf(errmsg,"Vertical datum %.20s is not defined",code);
        handle_error(INVALID_DATA,errmsg,nullptr);
    }
    return hrs;
}


int get_notes( int type, const char *code, output_string_def *os )
{
    crdsys_source_def *csd;
    char cscode[CRDCNV_CODE_LEN+1];
    int nch;
    double epoch = 0;
    int sts = MISSING_DATA;

    /* Look for an @ character, defining an deformation model reference epoch */
    for( nch = 0; code[nch] != 0 && code[nch] != '@'; nch++ ) {}
    if( code[nch] && ! parse_crdsys_epoch( code+nch+1, &epoch ) )
    {
        return INVALID_DATA;
    }
    if( nch > CRDCNV_CODE_LEN ) return INVALID_DATA;
    strncpy(cscode,code,nch);
    cscode[nch] = 0;

    for( csd = sources; csd; csd = csd->next )
        if( csd->getnotes )
        {

            sts = (*csd->getnotes)( csd->data, type, cscode, os->sink, os->write );
            if( sts == OK ) break;
        }
    return sts;
}


int get_crdsys_notes( coordsys *cs, output_string_def *os  )
{
    int sts;

    sts = get_notes( CS_COORDSYS_NOTE, cs->code.c_str(), os );
    if( cs->rf )
    {
        if( get_notes( CS_REF_FRAME_NOTE, cs->rf->code.c_str(), os  ) == OK ) sts=OK;
    }
    return sts;
}

std::optional<std::string> get_crdsys_file( const std::string &filename, const std::string &extension )
{
    for( crdsys_source_def *csd = sources; csd; csd = csd->next )
        if( csd->getcsfile )
        {
            auto found = (*csd->getcsfile)( csd->data, filename, extension );
            if( found ) return found;
        }
    return std::nullopt;
}

static int gcc_notes( int type, const char *code1, const char *code2, output_string_def *os )
{
    char convcode[CRDCNV_CODE_LEN+1];
    if( ! code1 || ! code2 ) return INVALID_DATA;
    if( strlen(code1) > CRDSYS_CODE_LEN || strlen(code2) > CRDSYS_CODE_LEN ) return INVALID_DATA;
    strcpy(convcode,code1);
    strcat(convcode,":");
    strcat(convcode,code2);
    return get_notes(type,convcode,os);
}

int get_conv_code_notes( int type, const char *code1, const char *code2, output_string_def *os )
{
    if( gcc_notes(type,code1,code2,os)==OK || gcc_notes(type,code2,code1,os)==OK ) return OK;
    return MISSING_DATA;
}

int get_conv_notes( coord_conversion *conv, output_string_def *os )
{
    int sts = MISSING_DATA;
    int icrf;
    const char *code1, *code2;
    if( get_conv_code_notes( CS_COORDSYS_NOTE,conv->from->code.c_str(), conv->to->code.c_str(), os ) == OK )
    {
        sts = OK;
    }
    code2=conv->from->rf->code.c_str();
    for( icrf=0; icrf < conv->ncrf; icrf++ )
    {
        coord_conversion_rf *crf = &(conv->crf[icrf]);
        if( crf->def_only ) continue;
        if( ! crf->rf ) continue;
        code1=code2;
        code2=crf->rf->code.c_str();
        if( get_conv_code_notes( CS_REF_FRAME_NOTE,code1,code2, os ) == OK )
        {
            sts = OK;
        }
    }
    return sts;
}
