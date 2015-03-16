#include "snapconfig.h"
/* Code to support printing of covariance information to files for
   subsequent use by other programs */

/*
   $Log: cvrfile.c,v $
   Revision 1.3  2004/04/22 02:35:43  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  1996/01/10 19:46:00  CHRIS
   Fixed error calculating projection coordinates of stations - eastings
   and northings and lats/longs were reversed in call geog_to_proj.

   Revision 1.1  1996/01/03 21:58:28  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "cvrfile.h"
#include "snapmain.h"
#include "output.h"
#include "snap/stnadj.h"
#include "util/leastsqu.h"
#include "util/chkalloc.h"
#include "util/progress.h"
#include "util/dms.h"
#include "util/xprintf.h"
#include "util/pi.h"


void print_coord_covariance( void )
{
    int nch;
    char *bfn;
    FILE *f;
    int maxstn, istn, ncrd;
    int *rownos;
    station *st;
    stn_adjustment *sa;
    long ncvr;
    int nline, ir, ic;
    bltmatrix *invnorm;
    double value;
    char projection_coords;
    void *latfmt = 0;
    void *lonfmt = 0;

    nch = strlen( root_name ) + strlen( CVRFILE_EXT ) + 1;
    bfn = ( char * ) check_malloc( nch );
    strcpy( bfn, root_name );
    strcat( bfn, CVRFILE_EXT );

    f = fopen( bfn, "w" );
    if( !f )
    {
        handle_error( FILE_OPEN_ERROR,"Unable to open covariance_file", bfn );
    }
    else
    {
        xprintf("\nCreating the coordinate covariance file %s\n",bfn);
    }
    check_free( bfn );
    if( !f ) return;

    maxstn = number_of_stations( net );
    ncrd = maxstn * 3;
    rownos = (int *) check_malloc( ncrd * sizeof(int));
    for( istn = 0; istn < ncrd; istn++ ) rownos[istn] = -1;

    projection_coords = is_projection( net->crdsys ) ? 1 : 0;
    if( !projection_coords )
    {
        latfmt = create_dms_format(3,6,0,NULL,NULL,NULL," N"," S");
        lonfmt = create_dms_format(3,6,0,NULL,NULL,NULL," E"," W");
    }

    fprintf(f,"! Lower triangle of coordinate covariance matrix\n");
    fprintf(f,"! Number of stations, number of coords per station\n");
    fprintf(f,"%d\n%d\n",(int) maxstn,3);
    fprintf(f,"! List of stations\n");
    for( istn = 0, ir=0; istn++ < maxstn; ir+=3)
    {
        st = stnptr( istn );
        sa = stnadj( st );
        fprintf(f,"%*s ",stn_name_width,st->Code );
        if( projection_coords )
        {
            double northing, easting;
            geog_to_proj( net->crdsys->prj, st->ELon, st->ELat, &easting, &northing);
            fprintf(f,"%13.*lf ",(int) coord_precision,easting);
            fprintf(f,"%13.*lf ",(int) coord_precision,northing);
        }
        else
        {
            fprintf(f,"%s ",dms_string(st->ELat*RTOD,latfmt,NULL));
            fprintf(f,"%s ",dms_string(st->ELon*RTOD,lonfmt,NULL));
        }
        fprintf(f,"%13.*lf\n",(int) coord_precision,st->OHgt);
        if( sa->hrowno ) { rownos[ir] = sa->hrowno-1; rownos[ir+1] = sa->hrowno; }
        if( sa->vrowno ) { rownos[ir+2] = sa->vrowno-1; }
    }

    if( !projection_coords )
    {
        check_free( latfmt );
        check_free( lonfmt );
    }

    fprintf(f,"! Lower triangle of coordinate covariance matrix\n");

    ncvr = ncrd;
    ncvr = (ncvr * (ncvr-1))/2;
    init_progress_meter( ncvr );
    nline = 0;
    ncvr = 0;
    invnorm = lsq_normal_matrix();
    for( ir = 0; ir < ncrd; ir++ ) for( ic = 0; ic <= ir; ic++ )
        {
            ncvr++;
            if( nline++ >= 5 )
            {
                fprintf(f,"\n");
                update_progress_meter( ncvr );
                nline = 1;
            }
            value = 0.0;
            if( rownos[ir] >= 0 && rownos[ic] >= 0 )
            {
                value = BLT( invnorm, rownos[ir], rownos[ic] );
            }
            fprintf(f," %15.9lE",value);
        }
    end_progress_meter();
    fclose(f);
}
