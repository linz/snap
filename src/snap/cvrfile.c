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
#include <math.h>

#include "cvrfile.h"
#include "output.h"
#include "snap/stnadj.h"
#include "snap/snapglob.h"
#include "snap/survfile.h"
#include "util/leastsqu.h"
#include "util/chkalloc.h"
#include "util/progress.h"
#include "util/dateutil.h"
#include "util/dms.h"
#include "util/xprintf.h"
#include "util/pi.h"
#include "util/getversion.h"

typedef double tmatrix[3][3];   /* Vector transformation matrix */

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
    check_free(rownos);
    if( latfmt ) delete_dms_format( latfmt );
    if( lonfmt ) delete_dms_format( lonfmt );
}

void print_coord_covariance_json( void )
{
    int nch;
    char *bfn;
    FILE *f;
    int maxstn, istn, ncrd;
    int *rownos;
    ellipsoid *elp;
    station *st;
    stn_adjustment *sa;
    long ncvr;
    int nline, ir, ic;
    bltmatrix *invnorm;
    double value;
    int projection_coords;
    int geocentric_coords;
    int ellipsoidal;

    nch = strlen( root_name ) + strlen(CVRFILE_EXT)+strlen( JSONFILE_EXT ) + 1;
    bfn = ( char * ) check_malloc( nch );
    strcpy( bfn, root_name );
    strcat( bfn, CVRFILE_EXT );
    strcat( bfn, JSONFILE_EXT );

    f = fopen( bfn, "w" );
    if( !f )
    {
        handle_error( FILE_OPEN_ERROR,"Unable to open JSON covariance_file", bfn );
    }
    else
    {
        xprintf("\nCreating the JSON coordinate covariance file %s\n",bfn);
    }
    check_free( bfn );
    if( !f ) return;

    maxstn = number_of_stations( net );
    ncrd = maxstn * 3;
    rownos = (int *) check_malloc( ncrd * sizeof(int));
    for( istn = 0; istn < ncrd; istn++ ) rownos[istn] = -1;

    projection_coords = is_projection( net->crdsys ) ? 1 : 0;
    geocentric_coords = is_geocentric( net->crdsys ) ? 1 : 0;
    ellipsoidal = net->options & NW_ELLIPSOIDAL_HEIGHTS;
    if( geocentric_coords ) ellipsoidal=1;

    elp = net->crdsys->rf->el;

    fprintf(f,"{\n");
    fprintf(f,"  \"coordsys\": \"%s\",\n",net->crdsys->code);
    fprintf(f,"  \"stations\": [");
    for( istn = 0, ir=0; istn++ < maxstn; ir+=3)
    {
        double height;
        st = stnptr( istn );
        sa = stnadj( st );
        if( istn > 1 ) fprintf(f,",");
        fprintf(f,"\n    {\n      \"code\": \"%s\",\n",st->Code);
        fprintf(f,"      \"coord\": [");
        height=st->OHgt;
        if( ellipsoidal ) height=st->OHgt+st->GUnd;
        if( projection_coords )
        {
            double northing, easting;
            geog_to_proj( net->crdsys->prj, st->ELon, st->ELat, &easting, &northing);
            fprintf(f,"%.*lf,%.*lf,%.*lf]\n",
                    (int) coord_precision,easting,
                    (int) coord_precision,northing,
                    (int) coord_precision,height);
        }
        else if( geocentric_coords )
        {
            double llh[3], xyz[3];
            llh[CRD_LON]=st->ELon;
            llh[CRD_LAT]=st->ELat;
            llh[CRD_HGT]=height;
            llh_to_xyz( elp, llh, xyz, 0, 0);
            fprintf(f,"%.*lf,%.*lf,%.*lf]\n",
                    (int) coord_precision,xyz[0],
                    (int) coord_precision,xyz[1],
                    (int) coord_precision,xyz[2]);
        }
        else
        {
            fprintf(f,"%.*lf,%.*lf,%.*lf]\n",
                    (int) coord_precision+6,st->ELon*RTOD,
                    (int) coord_precision+6,st->ELat*RTOD,
                    (int) coord_precision,height);
        }
        fprintf(f,"    }");

        if( sa->hrowno ) { rownos[ir] = sa->hrowno-1; rownos[ir+1] = sa->hrowno; }
        if( sa->vrowno ) { rownos[ir+2] = sa->vrowno-1; }
    }
    fprintf(f,"\n  ],\n");
    fprintf(f,"  \"seu\": %.6lf,\n",seu);
    fprintf(f,"  \"dof\": %d,\n",(int) dof);
    fprintf(f,"  \"enu_covariance\":\n  [");

    ncvr = ncrd*ncrd;
    init_progress_meter( ncvr );
    ncvr = 0;
    invnorm = lsq_normal_matrix();
    for( ir = 0; ir < ncrd; ir++ ) 
    {
        if( ir ) fprintf(f,",");
        fprintf(f,"\n    [");
        nline=0;
        for( ic = 0; ic < ncrd; ic++ )
        {
            ncvr++;
            if( ic ) fprintf(f,",");
            if( nline++ >= 6 )
            {
                fprintf(f,"\n    ");
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
        fprintf(f,"    ]");
    }
    fprintf(f,"\n  ]\n}\n");
    fclose(f);
    end_progress_meter();
    check_free(rownos);
}


static void print_sinex_date( FILE *f, double date )
{
    int year=0,dayno=0,secs=0;
    if( date != UNDEFINED_DATE )
    {
        date_as_yds( date, &year, &dayno, &secs );
    }
    year=year % 100;
    fprintf(f,"%02d:%03d:%05d",year,dayno,secs);
}

static void station_cvr( bltmatrix* invnorm, station *st1, station *st2, tmatrix cvr )
{
    int irow1[3], irow2[3];
    int i, j;

    irow1[0]=stnadj(st1)->hrowno;
    irow1[1]=irow1[0] ? irow1[0]+1 : 0;
    irow1[2]=stnadj(st1)->vrowno;

    irow2[0]=stnadj(st2)->hrowno;
    irow2[1]=irow2[0] ? irow2[0]+1 : 0;
    irow2[2]=stnadj(st2)->vrowno;


    for( j=0; j<3; j++) 
    {
        double icvr[3];
        for( i=0; i<3; i++ )
        {
            if( irow1[i]==0 || irow2[j]==0 )
            {
                icvr[i]=0.0;
            }
            else
            {
                icvr[i]=BLT(invnorm,irow1[i]-1,irow2[j]-1);
            }
        }
        unrotvec(icvr,&(st2->rTopo),icvr);
        for( i=0; i<3; i++ )
        {
            cvr[i][j]=icvr[i];
        }
    }
    for( i=0; i<3; i++ )
    {
        unrotvec(&(cvr[i][0]),&(st1->rTopo),&(cvr[i][0]));
    }
}
    

void print_coord_sinex( void )
{
    int nch;
    char *bfn;
    FILE *f;
    int maxstn, istn;
    ellipsoid *elp;
    station *st;
    stn_adjustment *sa;
    bltmatrix *invnorm;
    int nsnxprm=0;
    const char *dflt_mark="01";
    double mindate,maxdate;
    tmatrix cvr;
    int badcvr=0;

    nch = strlen( root_name ) + strlen(SINEX_EXT) + 1;
    bfn = ( char * ) check_malloc( nch );
    strcpy( bfn, root_name );
    strcat( bfn, SINEX_EXT );

    f = fopen( bfn, "w" );
    if( !f )
    {
        handle_error( FILE_OPEN_ERROR,"Unable to open SINEX output file", bfn );
    }
    else
    {
        xprintf("\nCreating the SINEX file %s\n",bfn);
    }
    check_free( bfn );
    if( !f ) return;

    elp = net->crdsys->rf->el;
    maxstn = number_of_stations( net );
    invnorm = lsq_normal_matrix();

    /* Header line */
    {
        int nnd;
        int maxcodlen=0;
        int constraint=2;

        survey_data_file_dates( &mindate, &maxdate, &nnd );

        /* Count parameters in this SINEX file */
        for( istn = 0; istn++ < maxstn; )
        {
            st = stnptr( istn );
            sa = stnadj( st );
            if( ! sa->obscount ) continue;
            if( strlen(st->Code) > maxcodlen ) maxcodlen=strlen(st->Code);
            if( sa->hrowno ||  sa->vrowno ) 
            {
                nsnxprm += 3;
                if( ! (sa->hrowno && sa->vrowno ) )
                {
                    badcvr=1;
                }
            }
            if( ! sa->flag.adj_h || ! sa->flag.adj_v ) constraint=0;
            else if( constraint==2 && (sa->flag.float_h || sa->flag.float_v)) constraint=1;
        }

        /* SNP = agency creating file */
        fprintf(f,"%%=SNX 2.01 SNP ");
        if( output_noruntime )
        {
            print_sinex_date(f,snap_date(2000,1,1));
        }
        else
        {
            print_sinex_date(f,snap_datetime_now());
        }
        /* SNP = agency providing data */
        fprintf(f," SNP ");
        print_sinex_date(f,mindate);
        fprintf(f," ");
        print_sinex_date(f,maxdate);
        fprintf(f," C %05d %1d S\n",nsnxprm,constraint);

        fprintf(f,"+FILE/REFERENCE\n");
        fprintf(f," %-18s %s\n","OUTPUT","Coordinate calculation results");
        fprintf(f," %-18s %s\n","SOFTWARE","SNAP survey network adjustment pacakge");
        fprintf(f," %-18s SNAP version %s date %s\n","SOFTWARE",
                PROGRAM_VERSION, PROGRAM_DATE );
        fprintf(f,"-FILE/REFERENCE\n");

        if( badcvr || maxcodlen > 4 )
        {
            fprintf(f,"+FILE/COMMENT\n");
            if( badcvr )
            {
                fprintf(f," Some stations adjusted horizontally or vertically only - covariance/constraints may be misleading\n");
                xprintf("  SINEX file may be incorrect as some stations adjusted horizontally or vertically only\n");
            }

            if( maxcodlen > 4 )
            {
                fprintf(f," SNAP station codes > 4 characters - may not be unique in SINEX\n");
                xprintf("  SNAP station codes > 4 characters - may not be unique in SINEX\n");
            }

            if( deformation )
            {
                fprintf(f,"\n The following deformation model was used in this solution\n");
                print_deformation_model( deformation, f, "   ");
            }

            fprintf(f,"-FILE/COMMENT\n");
        }
    }

    /* SITE/ID block */
    {
        void *latfmt = create_dms_format(3,1,DMSF_FMT_PREFIX_HEM,0,0,0,0,"-");
        void *lonfmt = create_dms_format(3,1,DMSF_FMT_PREFIX_HEM,0,0,0,0,"-");

        fprintf(f,"+SITE/ID\n");
        fprintf(f,"*CODE PT __DOMES__ T _STATION DESCRIPTION__ APPROX_LON_ APPROX_LAT_ _APP_H_\n");
        for( istn = 0; istn++ < maxstn; )
        {
            double lat;
            double lon;
            const char *mark;
            char latbuf[20];
            char lonbuf[20];

            st = stnptr( istn );
            sa = stnadj( st );
            if( ! sa->obscount ) continue;
            mark= strlen(st->Code) <= 4 ? dflt_mark : st->Code+4;
            lon=st->ELon*RTOD;
            if( lon < 0 ) lon += 360.0;
            lat=st->ELat*RTOD;
            dms_string(lat,latfmt,latbuf);
            dms_string(lon,lonfmt,lonbuf);
            
            fprintf(f," %-4.4s %-2.2s %-9.9s P %-22.22s %11.11s %11.11s %7.1lf\n",
                    st->Code,mark,st->Code,st->Name,lonbuf,latbuf,st->OHgt+st->GUnd
                   );
        }
        delete_dms_format(latfmt);
        delete_dms_format(lonfmt);
        fprintf(f,"-SITE/ID\n");
    }
    /*
    fprintf(f,"+SITE/ECCENTRICITY\n");
    fprintf(f,"-SITE/ECCENTRICITY\n");
    fprintf(f,"+SOLUTION/EPOCHS\n");
    fprintf(f,"-SOLUTION/EPOCHS\n");
    */
    {
        fprintf(f,"+SOLUTION/STATISTICS\n");
        if( badcvr || nsnxprm != nprm )
        {
            fprintf(f,"* This SINEX file does not include all parameters in the adjustment\n");
        }

        fprintf(f," NUMBER OF OBSERVATIONS                %15d\n",(int) (nobs+nschp));
        fprintf(f," NUMBER OF UNKNOWNS                    %15d\n",(int) nprm);
        fprintf(f," NUMBER OF DEGREES OF FREEDOM          %15d\n",(int) dof);
        fprintf(f," VARIANCE FACTOR                       %15.6lf\n",seu*seu);

        fprintf(f,"-SOLUTION/STATISTICS\n");
    }

    {
        const char *params[]={"STAX","STAY","STAZ"};
        double epoch=(maxdate+mindate)/2;
        int nprm = 0;

        fprintf(f,"+SOLUTION/ESTIMATE\n");
        fprintf(f,"*INDEX TYPE__ CODE PT SOLN _REF_EPOCH__ UNIT S __ESTIMATED VALUE____ _STD_DEV___\n");
        for( istn = 0 ; istn++ < maxstn;)
        {
            const char *mark;
            double llh[3], xyz[3];
            int stnconstraint;

            st = stnptr( istn );
            sa = stnadj( st );
            if( ! sa->obscount ) continue;
            if( ! ( sa->hrowno ||  sa->vrowno ) ) continue;

            mark= strlen(st->Code) <= 4 ? dflt_mark : st->Code+4;

            llh[CRD_LON]=st->ELon;
            llh[CRD_LAT]=st->ELat;
            llh[CRD_HGT]=st->OHgt+st->GUnd;
            llh_to_xyz( elp, llh, xyz, 0, 0);

            station_cvr(invnorm,st,st,cvr);
            stnconstraint=2;
            if( sa->flag.float_h || sa->flag.float_v) stnconstraint=1;

            for ( int i=0; i<3; i++ )
            {
                nprm++;
                fprintf(f," %5d %-6.6s %-4.4s %-2.2s 0001 ",nprm,params[i],st->Code,
                        mark);
                print_sinex_date(f,epoch);
                fprintf(f," m    %d %21.14lE %11.5lE\n",
                        stnconstraint,xyz[i],sqrt(cvr[i][i])*seu);
            }
        }
        fprintf(f,"-SOLUTION/ESTIMATE\n");
    }
    /*
    fprintf(f,"+SOLUTION/APRIORI\n");
    fprintf(f,"-SOLUTION/APRIORI\n");
    */
    {
        int nprmi,nprmj;
        fprintf(f,"+SOLUTION/MATRIX_ESTIMATE L COVA\n");
        fprintf(f,"*PARA1 PARA2 ____PARA2+0__________ ____PARA2+1__________ ____PARA2+2__________\n");

        nprmi = 1;
        for( istn = 0; istn++ < maxstn;)
        {
            int jstn;

            st = stnptr( istn );
            sa = stnadj( st );
            if( ! sa->obscount ) continue;
            if( ! ( sa->hrowno ||  sa->vrowno ) ) continue;

            nprmj=1;

            for( jstn = 1; jstn <= istn; jstn++ )
            {
                int row, col;
                station *stj =stnptr(jstn);
                stn_adjustment *saj=stnadj(stj);
                if( ! ( saj->hrowno ||  saj->vrowno ) ) continue;
                station_cvr(invnorm,st,stj,cvr);

                for( row=0; row < 3; row++ )
                {
                    int ncol = istn==jstn ? row : 2;
                    fprintf(f," %5d %5d",nprmi+row,nprmj);
                    for( col=0; col<=ncol; col++ )
                    {
                        fprintf(f," %21.14lE",cvr[row][col]*seu*seu);
                    }
                    fprintf(f,"\n");
                }
                nprmj += 3;
            }
            nprmi += 3;
        }
        fprintf(f,"-SOLUTION/MATRIX_ESTIMATE L COVA\n");
    }

    fprintf(f,"%%ENDSNX\n");
    fclose(f);
}
