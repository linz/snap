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
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include "util/snapctype.h"

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_src.h"
#include "util/chkalloc.h"
#include "util/dateutil.h"
#include "util/errdef.h"


static std::forward_list<crdsys_source_def> sources;
static int update_id = 0;

const std::forward_list<crdsys_source_def> &crdsys_sources()
{
    return sources;
}

int crdsys_source_update()
{
    return update_id;
}


void register_crdsys_source( const crdsys_source_def &csd )
{
    sources.push_front( csd );
    update_id++;
}

void uninstall_crdsys_lists( void )
{
    update_id++;
    for( const crdsys_source_def &csd : sources )
    {
        if( csd.delsource ) (*csd.delsource)( csd.data );
    }
    sources.clear();
}


ref_frame * load_ref_frame( std::string_view code )
{
    int sts = MISSING_DATA;
    ref_frame *rf = nullptr;

    for( const crdsys_source_def &csd : sources )
    {
        if( ! csd.getrf ) continue;
        sts = (*csd.getrf)( csd.data, CS_ID_UNAVAILABLE, code, &rf );
        if( sts != MISSING_DATA ) break;
    }

    if( sts == MISSING_DATA )
    {
        const std::string errmsg = "Reference frame " + std::string( code.substr( 0, 20 ) ) + " is not defined";
        handle_error(INVALID_DATA,errmsg.c_str(),nullptr);
    }
    return rf;
}


ellipsoid * load_ellipsoid( std::string_view code )
{
    int sts = MISSING_DATA;
    ellipsoid *el= nullptr;

    for( const crdsys_source_def &csd : sources )
    {
        if( ! csd.getel ) continue;
        sts = (*csd.getel)( csd.data, CS_ID_UNAVAILABLE, code, &el );
        if( sts != MISSING_DATA ) break;
    }
    if( sts == MISSING_DATA )
    {
        const std::string errmsg = "Ellipsoid " + std::string( code.substr( 0, 20 ) ) + " is not defined";
        handle_error(INVALID_DATA,errmsg.c_str(),nullptr);
    }

    return el;
}

bool parse_crdsys_epoch( std::string_view epochstr, double &epoch )
{
    epoch=snap_datetime_parse(epochstr);
    if( ! epoch ) return false;
    epoch = date_as_year(epoch);
    return true;
}

/* Coordinate systems defined by code, optionally followed by @epoch, where
   epoch is either a decimal year number (eg 2007.5) or "now" */

coordsys * load_coordsys( std::string_view code )
{
    double epoch = 0;
    int sts = MISSING_DATA;

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
    if( epochstr && ! parse_crdsys_epoch( *epochstr, epoch ) )
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
            rf=load_ref_frame( *dtmcode );
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
            hrs=load_vdatum( *hrscode );
            if( ! hrs )
            {
                delete rf;
                return nullptr;
            }
        }
    }

    for( const crdsys_source_def &csd : sources )
    {
        if( ! csd.getcs ) continue;
        sts = (*csd.getcs)( csd.data, CS_ID_UNAVAILABLE, cscode, &cs );
        if( sts != MISSING_DATA ) break;
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

std::string coordsys_load_code( coordsys *cs )
{
    std::string fullcode = cs->code.substr( 0, CRDCNV_CODE_LEN );
    if( cs->setrf && cs->rf &&
            cs->rf->code.size()+fullcode.size()+2 < CRDCNV_CODE_LEN )
    {
        fullcode += "(" + cs->rf->code + ")";
    }
    if( cs->hrs
            && cs->hrs->code.size()+fullcode.size()+1 < CRDCNV_CODE_LEN
      )
    {
        fullcode += "/" + cs->hrs->code;
    }
    return fullcode;
}

vdatum * load_vdatum( std::string_view code )
{
    int sts = MISSING_DATA;
    vdatum *hrs= nullptr;

    for( const crdsys_source_def &csd : sources )
    {
        if( ! csd.gethrs ) continue;
        sts = (*csd.gethrs)( csd.data, CS_ID_UNAVAILABLE, code, &hrs );
        if( sts != MISSING_DATA ) break;
    }

    if( sts == MISSING_DATA )
    {
        const std::string errmsg = "Vertical datum " + std::string( code.substr( 0, 20 ) ) + " is not defined";
        handle_error(INVALID_DATA,errmsg.c_str(),nullptr);
    }
    return hrs;
}


int get_notes( int type, std::string_view code, output_string_def *os )
{
    double epoch = 0;
    int sts = MISSING_DATA;

    /* Look for an @ character, defining an deformation model reference epoch */
    const size_t nch = std::min( code.find( '@' ), code.size() );
    if( nch < code.size() && ! parse_crdsys_epoch( code.substr( nch + 1 ), epoch ) )
    {
        return INVALID_DATA;
    }
    if( nch > CRDCNV_CODE_LEN ) return INVALID_DATA;
    const std::string_view cscode = code.substr( 0, nch );

    for( const crdsys_source_def &csd : sources )
    {
        if( ! csd.getnotes ) continue;
        sts = (*csd.getnotes)( csd.data, type, cscode, os->sink, os->write );
        if( sts == OK ) break;
    }
    return sts;
}


int get_crdsys_notes( coordsys *cs, output_string_def *os  )
{
    int sts;

    sts = get_notes( CS_COORDSYS_NOTE, cs->code, os );
    if( cs->rf )
    {
        if( get_notes( CS_REF_FRAME_NOTE, cs->rf->code, os  ) == OK ) sts=OK;
    }
    return sts;
}

std::optional<std::string> get_crdsys_file( const std::string &filename, const std::string &extension )
{
    for( const crdsys_source_def &csd : sources )
    {
        if( ! csd.getcsfile ) continue;
        auto found = (*csd.getcsfile)( csd.data, filename, extension );
        if( found ) return found;
    }
    return std::nullopt;
}

static int gcc_notes( int type, std::string_view code1, std::string_view code2, output_string_def *os )
{
    if( code1.size() > CRDSYS_CODE_LEN || code2.size() > CRDSYS_CODE_LEN ) return INVALID_DATA;
    std::string convcode( code1 );
    convcode += ':';
    convcode += code2;
    return get_notes(type,convcode,os);
}

int get_conv_code_notes( int type, std::string_view code1, std::string_view code2, output_string_def *os )
{
    if( gcc_notes(type,code1,code2,os)==OK || gcc_notes(type,code2,code1,os)==OK ) return OK;
    return MISSING_DATA;
}

int get_conv_notes( coord_conversion *conv, output_string_def *os )
{
    int sts = MISSING_DATA;
    int icrf;
    std::string_view code1, code2;
    if( get_conv_code_notes( CS_COORDSYS_NOTE,conv->from->code, conv->to->code, os ) == OK )
    {
        sts = OK;
    }
    code2=conv->from->rf->code;
    for( icrf=0; icrf < conv->ncrf; icrf++ )
    {
        coord_conversion_rf *crf = &(conv->crf[icrf]);
        if( crf->def_only ) continue;
        if( ! crf->rf ) continue;
        code1=code2;
        code2=crf->rf->code;
        if( get_conv_code_notes( CS_REF_FRAME_NOTE,code1,code2, os ) == OK )
        {
            sts = OK;
        }
    }
    return sts;
}
