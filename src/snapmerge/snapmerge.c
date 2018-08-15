#include "snapconfig.h"
/*
   $Log: $
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "util/errdef.h"
#include "util/strarray.h"
#include "network/network.h"
#include "util/fileutil.h"
#include "util/dstring.h"
#include "util/getversion.h"
#include "snap/filenames.h"


strarray codes;
network *base = 0;
char listonly = 0;

int select_station( station *st )
{
    if( listonly && strarray_find( &codes, st->Code ) == STRARRAY_NOT_FOUND ) return 0;
    return 1;
}

int main( int argc, char *argv[] )
{
    char *stnlistfile = NULL;
    char *basefile = NULL;
    char *datafile = NULL;
    char *newfile = NULL;
    network *data;
    int quiet = 0;
    int overwrite = 0;
    int syntaxerror = 0;
    int mergeopt = 0;
    int updatecls = 0;
    int updatecrd = 0;
    int addstn = 1;
    int addclass = 1;
    int clearbaseorders = 0;
    int cleardataorders = 0;

    CONFIGURE_RUNTIME();

    while( ! syntaxerror && argc > 1 && argv[1][0] == '-' )
    {
        switch ( argv[1][1] )
        {
        case 'q':
        case 'Q': quiet = 1; break;

        case 'o':
        case 'O': 
            switch (argv[1][2])
            {
            case 0:
                overwrite=1; 
                break;
            case 'c':
            case 'C': 
                updatecrd= 1;
                break;
            case 'a':
            case 'A': 
                updatecls= 1;
                break;
            default:
                syntaxerror=1;
                break;
            };
            break;

        case 'u':
        case 'U': 
            switch (argv[1][2])
            {
            case 0:
                addstn = 0; 
                break;
            case 'a':
            case 'A': 
                addclass = 0;
                break;
            default:
                syntaxerror=1;
                break;
            };
            break;

        case 'c':
        case 'C':
            switch (argv[1][2])
            {
            case 0:
            case 'b':
            case 'B': clearbaseorders= 1;
                break;
            case 'd':
            case 'D': cleardataorders= 1;
                break;
            default:
                syntaxerror=1;
                break;
            };
            break;

        case 'l':
        case 'L':
            if( argc > 2 )
            {
                stnlistfile = argv[2];
                argc--;
                argv++;
            }
            else
            {
                syntaxerror = 1;
            }
            break;

        default:
            syntaxerror = 1;
            break;
        }
        argc--;
        argv++;
    }

    if( argc != 3 && argc != 4 ) syntaxerror = 1;

    if( ! quiet )
    {
        printf("\n%s %s: Merges two SNAP coordinate files\n\n",
               PROGRAM_NAME, PROGRAM_VERSION );
    }

    if( syntaxerror )
    {
        printf("Syntax: snapmerge [options] basefile datafile [newfile]\n\n");
        printf("where\n");
        printf("  basefile    is the input coordinate file\n");
        printf("  datafile    is the coordinate file to merge\n");
        printf("  newfile     is the name of the output coordinate file\n");
        printf("              (defaults to the same as basefile)\n");
        printf("and options are:\n");
        printf("  -o          replace existing stations with new versions\n");
        printf("  -oc         overwrite coordinates of stations\n");
        printf("  -oa         overwrite classification attributes of stations\n");
        printf("  -u          update only .. don't add any new stations\n");
        printf("  -ua         update classes only .. don't add new classes\n");
        printf("  -c or -cb   Remove coordinate orders from basefile\n");
        printf("  -cd         Remove coordinate orders from datafile\n");
        printf("  -l listfile defines a file with a list of station codes to include\n");
        printf("              from data file - only these codes will be added\n");
        printf("  -q          quiet mode - minimal output\n");

        return 2;
    }

    basefile = argv[1];
    datafile = argv[2];
    newfile = argc > 3 ? argv[3] : argv[1];

    install_default_crdsys_file();

    base = new_network();
    if( read_network( base, basefile, 0 ) != OK )
    {
        printf("Cannot open base coordinate file %s\n",basefile);
        return 2;
    }
    if( clearbaseorders && base->orderclsid > 0)
    {
        int nstn= number_of_stations( base );
        int clsid = base->orderclsid;
        for( int istn = 1; istn <= nstn; istn++ )
        {
            station *st = station_ptr( base, istn );
            set_station_class( st, clsid, 0 );
        }

    }

    data = new_network();
    if( read_network( data, datafile, 0 ) != OK )
    {
        printf("Cannot open data coordinate file %s\n",datafile);
        return 2;
    }
    if( cleardataorders && data->orderclsid > 0)
    {
        int nstn= number_of_stations( data );
        int clsid = data->orderclsid;
        for( int istn = 1; istn <= nstn; istn++ )
        {
            station *st = station_ptr( data, istn );
            set_station_class( st, clsid, 0 );
        }
    }

    strarray_init( &codes );
    if( stnlistfile )
    {
        char code[81];
        FILE *list;
        list = fopen( stnlistfile,"r");
        if( ! list )
        {
            printf("Cannot open station list file %s\n",stnlistfile);
            return 2;
        }
        skip_utf8_bom(list);
        while( fscanf( list,"%80s",code) == 1 )
        {
            strarray_add( &codes, code );
        }
        fclose(list);
        listonly = 1;
    }

    if( ! addstn && ! (updatecrd | updatecls) ) overwrite=1;
    mergeopt = 0;
    if( addstn) mergeopt |= NW_MERGEOPT_ADDNEW;
    if( addclass ) mergeopt |= NW_MERGEOPT_ADDCLASSES;
    if( overwrite ) mergeopt |= NW_MERGEOPT_OVERWRITE;
    if( updatecrd ) mergeopt |= NW_MERGEOPT_COORDS;
    if( updatecls ) mergeopt |= NW_MERGEOPT_CLASSES;
    merge_network( base, data, mergeopt, &select_station );
    strarray_delete( &codes );

    if( write_network( base, newfile, 0, 0, 0 ) != OK )
    {
        return 2;
    }

    return 0;
}
