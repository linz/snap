#include "snapconfig.h"
/*
   $Log: netstns1.c,v $
   Revision 1.1  1995/12/22 17:31:54  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/network.h"
#include "network/stnoffset.h"
#include "util/dstring.h"
#include "util/chkalloc.h"

/* Procedure to dump the station coordinates to a binary file */
/* Flags are dumped separately to improve binary file compatibility
   between different compilers */


static void dump_station_offset( station *st, FILE *f )
{
    stn_offset *sto=(stn_offset *)(st->ts);
    int ncomp=0;
    if( sto )
    {
        for( stn_offset_comp *comp=sto->components; comp; comp=comp->next ) ncomp++;
    }
    fwrite( &ncomp, sizeof(int), 1, f );
    if( ! ncomp ) return;
    fwrite( &(sto->isdeformation), sizeof(int), 1, f );
    for( stn_offset_comp *comp=sto->components; comp; comp=comp->next )
    {
        fwrite(&(comp->mode),sizeof(int),1,f);
        fwrite(&(comp->isxyz),sizeof(int),1,f);
        fwrite(&(comp->ntspoints),sizeof(int),1,f);
        fwrite(&(comp->basepoint),sizeof(stn_tspoint),1,f);
        if( comp->ntspoints )
        {
            fwrite(comp->tspoints,sizeof(stn_tspoint),comp->ntspoints,f);
        }
    }
}

static void reload_station_offset( station *st, FILE *f )
{
    int ncomp;
    int isdef;
    fread( &ncomp, sizeof(int), 1, f );
    if( ncomp == 0 ) return;
    fread( &isdef, sizeof(int), 1, f );
    while( ncomp-- )
    {
        int mode, isxyz, ntspoints;
        stn_offset_comp *sto;
        fread(&mode,sizeof(int),1,f);
        fread(&isxyz,sizeof(int),1,f);
        fread(&ntspoints,sizeof(int),1,f);
        sto=create_stn_offset_comp(mode,isxyz,ntspoints);
        fread(&(sto->basepoint),sizeof(stn_tspoint),1,f);
        if( ntspoints > 0 )
        {
            fread(sto->tspoints,sizeof(stn_tspoint),ntspoints,f);
        }
        add_stn_offset_comp_to_station( st, sto, isdef );
    }
}

void dump_station( station *st, FILE *f )
{
    fwrite(st,sizeof(station)-sizeof(st->classval)-sizeof(st->Name)-sizeof(st->ts)-sizeof(st->hook),1,f);
    if( st->nclass > 0 ) fwrite( st->classval, sizeof(int), st->nclass, f );
    dump_station_offset( st, f );
    dump_string( st->Name, f );
}

station *reload_station( FILE *f )
{
    station *st;
    int nclass;
    st = new_station();
    fread(st,sizeof(station)-sizeof(st->classval)-sizeof(st->Name)-sizeof(st->ts)-sizeof(st->hook),1,f);
    nclass = st->nclass;
    if( nclass > 0 )
    {
        st->nclass = 0;
        st->classval = 0;
        init_station_classes( st, nclass );
        fread( st->classval, sizeof(int), nclass,f );
    }
    reload_station_offset( st, f );
    st->Name = reload_string( f );
    return st;
}

