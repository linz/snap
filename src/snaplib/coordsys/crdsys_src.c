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
#include <ctype.h>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_src.h"
#include "util/chkalloc.h"
#include "util/dateutil.h"
#include "util/errdef.h"


static char rcsid[]="$Id: crdsysd0.c,v 1.1 1995/12/22 16:28:07 CHRIS Exp $";

static crdsys_source_def *sources = NULL;
static int update_id = 0;

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

    return el;
}

static int parse_epoch( const char *epochstr, double *epoch )
{
    if( _stricmp(epochstr,"now") == 0 )
    {
        *epoch = date_as_year(snap_datetime_now());
        return 1;
    }
    if( epochstr[0] != '1' && epochstr[0] != '2' ) return 0;
    if( strlen(epochstr) == 8 && epochstr[4] != '.' )
    {
        int i, y,m,d;
        for( i = 1; i < 8; i++ )
        {
            if( ! isdigit(epochstr[i])) return 0;
        }
        sscanf(epochstr,"%4d%2d%2d",&y,&m,&d);
        *epoch = date_as_year( snap_date(y,m,d));
        return 1;
    }
    if( strlen(epochstr) >= 4 )
    {
        int i;
        for( i = 1; i < strlen(epochstr); i++ )
        {
            char c = epochstr[i];
            if( i == 4 && c == '.' ) continue;
            if( i != 4 && isdigit(c) ) continue;
            return 0;
        }
        if ( sscanf(epochstr,"%lf",epoch) == 1 ) return 1;
    }

    return 0;
}

/* Coordinate systems defined by code, optionally followed by @epoch, where
   epoch is either a decimal year number (eg 2007.5) or "now" */

coordsys * load_coordsys( const char *code )
{
    crdsys_source_def *csd;
    char cscode[CRDSYS_CODE_LEN+1];
    int nch;
    double epoch = 0;
    int sts;
    coordsys *cs= NULL;

    /* Look for an @ character, defining an epoch */
    for( nch = 0; code[nch] != 0 && code[nch] != '@'; nch++ ) {}
    if( code[nch] && ! parse_epoch( code+nch+1, &epoch ) )
    {
        return NULL;
    }
    if( nch > CRDSYS_CODE_LEN ) return NULL;
    strncpy(cscode,code,nch);
    cscode[nch] = 0;

    for( sts = MISSING_DATA, csd = sources;
            sts == MISSING_DATA && csd;
            csd = csd->next ) if( csd->getcs )
        {

            sts = (*csd->getcs)( csd->data, CS_ID_UNAVAILABLE, cscode, &cs );
        }

    if( cs ) define_coordsys_epoch(cs,epoch);
    return cs;
}
