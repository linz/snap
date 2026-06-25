#include "snapconfig.h"
/*
   $Log: rftrndmp.c,v $
   Revision 1.1  1995/12/22 17:47:24  CHRIS
   Initial revision

*/

#include <stdio.h>

#include "util/errdef.h"
#include "util/binfile.h"
#include "snap/rftrndmp.h"
#include "snap/rftrans.h"
#include "util/dstring.h"


void dump_rftransformations( BINARY_FILE *b )
{
    int nrf;
    rfTransformation *rf;
    int irf;

    create_section(b,"RFTRANSFORMATIONS");
    nrf = rftrans_count();

    fwrite( &nrf, sizeof(nrf), 1, b->f );

    /* Dump the reference frame definitions */

    for( irf = 0; irf++ < nrf; )
    {
        rf = rftrans_from_id( irf );
        fwrite( rf, sizeof(*rf), 1, b->f );
        dump_string( rf->name, b->f );
    }

    end_section( b );
}



int reload_rftransformations( BINARY_FILE *b )
{
    int nrf;
    int irf;
    rfTransformation *rf;

    if(find_section(b,"RFTRANSFORMATIONS") != OK ) return MISSING_DATA;

    /* Dump the station definitions */

    if( fread(&nrf, sizeof(nrf), 1, b->f ) != 1 || nrf < 0 ) return INVALID_DATA;

    clear_rftrans_list();
    for( irf = 0; irf++ < nrf;  )
    {
        rf = new_rftrans();
        if( fread(rf, sizeof(*rf), 1, b->f ) != 1 ) return INVALID_DATA;
        rf->name = reload_string( b->f );
        if( !rf->name ) return INVALID_DATA;
    }
    return check_end_section( b );
}
