/*
   $Log: site2gps.c,v $
   Revision 1.1  1996/01/03 22:53:35  CHRIS
   Initial revision

*/

#define GETVERSION_SET_PROGRAM_DATE
#include "snapconfig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "snap/stnadj.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "snap/filenames.h"
#include "util/getversion.h"

void print_gps_baselines( FILE *out, int *stn, int nstns )
{
    int j;
    station *s1;
    vector3 dxyz;
    s1 = station_ptr( net, stn[0] );
    for( j = 1; j < nstns; j++ )
    {
        station *s2;
        s2 = station_ptr( net, stn[j] );
        calc_vec_dif( s1, 0.0, s2, 0.0, dxyz, NULL, NULL );
        fprintf(out,"%-5s %-5s %10.3lf %10.3lf %10.3lf\n",s1->Code,s2->Code,
                dxyz[0],dxyz[1],dxyz[2] );
    }
}

void print_multistation_gps( FILE *out, int *stn, int nstns )
{
    station *s1, *s2;
    int i;
    vector3 dxyz;
    s1 = station_ptr( net, stn[0] );
    fprintf(out,"%s\n",s1->Code);
    for( i = 1; i < nstns; i++ )
    {
        s2 = station_ptr( net, stn[i] );
        calc_vec_dif( s1, 0.0, s2, 0.0, dxyz, NULL, NULL );
        fprintf(out,"%-5s %10.3lf %10.3lf %10.3lf\n",s2->Code,
                dxyz[0],dxyz[1],dxyz[2] );
    }
}


int check_list( int *stn, int nstns, double maxlen, double minlen, double *dst, int nmax )
{
    int istn, i;
    station *s1, *s2;
    if( nstns < 2 ) return nstns;
    s1 = station_ptr( net, stn[0] );
    istn = 1;
    for( i = 1; i < nstns; i++ )
    {
        double d;
        s2 = station_ptr( net, stn[i] );
        d = calc_distance( s1, 0.0, s2, 0.0, NULL, NULL );
        if( d < minlen || d > maxlen ) continue;
        stn[istn++] = stn[i];
    }
    if( istn+1 <= nmax ) return istn;
    if( nmax < 1 ) return istn;
    if( !dst ) return istn;

    s2 = station_ptr( net, stn[1] );
    dst[1] = calc_distance( s1, 0.0, s2, 0.0, NULL, NULL );
    for( i = 2; i < istn; i++ )
    {
        int si = stn[i];
        int j;
        double d = calc_distance( s1, 0.0, station_ptr(net, si), 0.0, NULL, NULL );
        j = i-1;
        if( j > nmax ) j = nmax;

        while( j >= 1 && d < dst[j] )
        {
            dst[j+1] = dst[j];
            stn[j+1] = stn[j];
            j--;
        }
        j++;
        stn[j] = si;
        dst[j] = d;
    }

    return nmax+1;
}

int main( int argc, char *argv[] )
{
    FILE *in=NULL, *out;
    int *stn;
    int *stn2;
    double *dst;
    int nstns;
    int baselines = 0;
    int autolist = 0;
    double maxlen = 99999999999.0;
    double minlen = 0.0;
    int nmax = 30000;
    int syntax_error = 0;
    int nostns;
    int nextstn;
    int block_id;

    printf("\n%s %s: Creates a dummy GPS data file from a site file and list of lines\n\n",
           PROGRAM_NAME, PROGRAM_VERSION);

    while ( argc > 1 && argv[1][0] == '-' )
    {
        switch( argv[1][1] )
        {
        case 'b': case 'B': baselines = 1; break;
        case 'a': case 'A': autolist = 1; break;
        case 'f': case 'F': maxlen = atof( argv[1]+2 ) * 1000; break;
        case 'c': case 'C': minlen = atof( argv[1]+2 ) * 1000; break;
        case 'n': case 'N': nmax = atoi( argv[1]+2 ); break;
        default:
            printf("Invalid switch %s\n",argv[1]);
            syntax_error = 1;
        }
        argc--;
        argv++;
    }


    if( syntax_error  || argc != (autolist ? 3 : 4) )
    {
        printf("Syntax: site2gps [options] crd_file_name line_list_file output_file\n");
        printf("\nIn the line_list_file, each line is a list of stations to be connected\n\n");
        printf("Valid input files are:\n");
        printf("  -a    Automatically connect every station to every other station,\n");
        printf("        The line_list_file should be omitted\n");
        printf("  -b    The output file will consist of individual baselines rather than\n");
        printf("        multistation data\n");
        printf("  -c#   Lines less # km will be omitted\n");
        printf("  -f#   Lines greater then # km will be omitted\n");
        printf("  -n#   No more than n lines will be output\n");
        return 0;
    }

    install_default_crdsys_file();

    if( read_station_file( argv[1], NULL, STN_FORMAT_SNAP, 0, 0 ) != OK )
    {
        printf("Cannot open coordinate file %s\n",argv[1]);
        return 0;
    }

    nostns = number_of_stations( net );
    if( !nostns )
    {
        printf("No stations loaded from coordinate file\n");
        return 0;
    }
    stn = (int *) malloc( 2 * nostns * sizeof(int) );
    dst = (double *) malloc( nostns * sizeof(double) );
    if( !stn || !dst )
    {
        printf("Insufficient memory for program\n");
        return 0;
    }
    stn2 = stn + nostns;

    if( !autolist && (NULL == (in = fopen(argv[2],"r"))))
    {
        printf("Cannot open list file %s\n",argv[2]);
        return 0;
    }

    if( NULL == (out = fopen(argv[(autolist ? 2 :3)],"w")))
    {
        printf("Cannot open output data file %s\n",argv[(autolist ? 2 : 3)]);
        return 0;
    }

    fprintf(out,"Dummy gps data file\n");
    fprintf(out,"\n#date unknown\n");
    fprintf(out,"#gps_enu_error 10 10 10 mm 1 1 1 ppm\n");
    fprintf(out,"#data gb no_heights%s\n\n",baselines ? "" : " grouped");

    nextstn = 0;
    block_id = 0;

    while( 1 )
    {

        if( autolist )
        {
            int i;
            nextstn++;
            if( nextstn > nostns ) break;
            nstns = 1;
            stn[0] = nextstn;
            i = 1;
            for( ; i <= nostns; i++ )
            {
                if( i == nextstn ) continue;
                stn[nstns++] = i;
            }
            if( baselines ) nextstn = nostns+1;
        }
        else
        {
            char inrec[512];
            char *code;
            nstns = 0;

            if( !fgets(inrec,512,in) ) break;
            if( inrec[0] == '!' || inrec[0] == '#' ) { fputs(inrec,out); continue; }
            for( code = strtok(inrec," \t\r\n"); code; code = strtok(NULL," \t\r\n") )
            {
                int id;
                int i;
                id = find_station( net, code );
                if( !id )
                {
                    printf("Station %s is not defined in the coordinate file\n", code);
                    continue;
                }
                for( i = 0; i < nstns; i++ ) if( stn[i] == id ) break;
                if( i < nstns ) continue;
                stn[nstns++] = id;
            }
            fprintf(out,"\n#classify gb SESSION SESSION_%d\n",block_id);
            block_id++;
        }
        if( autolist || baselines )
        {
        }

        if( nstns < 2 ) continue;
        fprintf(out,"\n");
        if( baselines )
        {
            int i;
            char need_all = nmax < nstns;
            for( i = 0; i < nstns; i++ )
            {
                int j;
                int nstns2 = 1;
                stn2[0] = stn[i];
                for( j = need_all ? 0 : i+1; j < nstns; j++ )
                {
                    if( i == j ) continue;
                    stn2[nstns2++] = stn[j];
                }
                if( nstns2 > 1 )
                {
                    nstns2 = check_list( stn2, nstns2, maxlen, minlen, dst, nmax );
                    print_gps_baselines( out, stn2, nstns2 );
                }
            }
        }
        else
        {
            if( autolist ) nstns = check_list( stn, nstns, maxlen, minlen, dst, nmax );
            print_multistation_gps( out, stn, nstns );
        }
    }

    if(in) fclose(in);
    fclose(out);
    return 0;
}

