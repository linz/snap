#include "snapconfig.h"
/*
   $Log: stnobseq.c,v $
   Revision 1.6  2005/08/02 00:37:39  ccrook
   Removed minor error ... brng can be used without being initiallized.

   Revision 1.5  2003/11/23 23:05:19  ccrook
   Updated iteration output to display number of stations exceeding adjustment
   tolerance

   Revision 1.4  1998/06/15 02:24:59  ccrook
   Modified to handle long integer number of changes

   Revision 1.3  1998/05/21 04:01:53  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.2  1996/10/25 21:50:05  CHRIS
   Fixed bug identifying the station corresponding to a row number
   in the adjustment.

   Revision 1.1  1996/01/03 22:11:45  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "stnobseq.h"
#include "adjparam.h"
#include "autofix.h"
#include "output.h"
#include "reorder.h"
#include "residual.h"
#include "snap/deform.h"
#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "snapdata/stnrecode.h"
#include "snapdata/survdata.h" /* For UNDEFINED_DATE */
#include "util/binfile.h"
#include "util/dateutil.h"
#include "util/dms.h"
#include "util/errdef.h"
#include "util/leastsqu.h"
#include "util/lsobseq.h"
#include "util/pi.h"
#include "util/probfunc.h"

int scale_error = 0;
char  floating_stations=0;
char  relative_floating=0;

static void *latfmt = NULL;
static void *lonfmt = NULL;

static void setup_latlon_format( void )
{
    latfmt = create_dms_format(3,6,0,NULL,NULL,NULL," N"," S");
    lonfmt = create_dms_format(3,6,0,NULL,NULL,NULL," E"," W");
}


void count_stn_obs( int type, int stn, char unused )
{
    stn_adjustment *sa;
    if( unused ) return;
    sa = stnadj( stnptr(stn) );
    sa->flag.observed = 1;
    if( datatype[type].isvector ) sa->obscount += 2;
    sa->obscount++;
}

static int add_colocation_constraint( station *stcol, station *st0, double herror, double verror )
{

    stn_adjustment *sa=stnadj(stcol);
    if( sa->idcol )
    {
        char errmsg[80+STNCODELEN*2];
        sprintf(errmsg,"Cannot colocate %.*s with %.*s - already colocated with %.*s",
                STNCODELEN,stcol->Code,STNCODELEN,st0->Code,
                STNCODELEN,stnptr(sa->idcol)->Code );
        handle_error(INCONSISTENT_DATA,errmsg,NO_MESSAGE);
        return INCONSISTENT_DATA;
    }
    sa->idcol=st0->id;
    sa->herror=herror;
    sa->verror=verror;
    sa->flag.float_h=1;
    sa->flag.float_v=1;
    add_connection( stcol->id, st0->id );
    return OK;
}

int add_station_colocation_constraints()
{
    int sts=OK;
    if( ! stnrecode ) return sts;
    for( int i = 0; i++ < number_of_stations(net); )
    {
        station *st = stnptr(i);
        stn_recode *recode0 = get_station_recodes( stnrecode,  st->Code );
        if( ! recode0 ) continue;
        /* Check for duplicate recodings */
        int sts0=OK;
        for( stn_recode *recode=recode0; recode->next ; recode=recode->next )
        {
            if( recode->datefrom != UNDEFINED_DATE && recode->dateto != UNDEFINED_DATE ) continue;
            if( recode->datefrom == UNDEFINED_DATE && recode->dateto == UNDEFINED_DATE ) continue;
            for( stn_recode *rec2=recode->next; rec2; rec2=rec2->next )
            {
                if( rec2->datefrom != UNDEFINED_DATE && rec2->dateto != UNDEFINED_DATE ) continue;
                if( rec2->datefrom == UNDEFINED_DATE && rec2->dateto == UNDEFINED_DATE ) continue;
                if( stncodecmp(recode->codeto,rec2->codeto) == 0 )
                {
                    char errmsg[100+STNCODELEN*3+MAX_DATE_LEN*2];
                    sprintf(errmsg,"Recode of %.*s to %.*s %s %s inconsistent with %.*s %s %s",
                            STNCODELEN,st->Code,
                            STNCODELEN,recode->codeto,
                            recode->datefrom == UNDEFINED_DATE ? "before" : "after",
                            recode->datefrom == UNDEFINED_DATE ? 
                                 date_as_string(recode->dateto,"DT?",0) : 
                                 date_as_string(recode->datefrom,"DT?",0),
                            STNCODELEN,rec2->codeto,
                            rec2->datefrom == UNDEFINED_DATE ? "before" : "after",
                            rec2->datefrom == UNDEFINED_DATE ? 
                                 date_as_string(rec2->dateto,"DT?",0) : 
                                 date_as_string(rec2->datefrom,"DT?",0));
                    handle_error(INCONSISTENT_DATA,errmsg,NO_MESSAGE);
                    sts0=INCONSISTENT_DATA;
                }
            }
        }
        if( sts0 != OK )
        {
            sts=sts0;
            continue;
        }

        /* Process before ### updates */
        station*st0=0;
        double herror2=0.0;
        double verror2=0.0;
        for( stn_recode *recode=recode0; recode; recode=recode->next )
        {
            if( recode->datefrom != UNDEFINED_DATE || recode->dateto == UNDEFINED_DATE )
            {
                continue;
            }
            int idto=find_station( net, recode->codeto );
            station *stto=stnptr(idto);
            bool usestn=false;
            if( stto )
            {
                stn_adjustment *sato=stnadj(stto);
                if( sato && sato->obscount > 0 ) usestn=true;
            }
            if( ! usestn )
            {
                /* Update errors */
                if( recode->herror > 0.0 && recode->verror >= 0.0  && st0 )
                {
                    herror2 += recode->herror*recode->herror;
                    verror2 += recode->verror*recode->verror;
                }
                /* If disconnected then breaks link with previous versions */
                else
                {
                    st0=0;
                    herror2=0.0;
                    verror2=0.0;
                }
                continue;
            }
            if( st0 )
            {
                if( stto )
                {
                    sts0=add_colocation_constraint( st0, stto, sqrt(herror2), sqrt(verror2) );
                    if( sts0 != OK ) sts=sts0;
                }
                st0=0;
                herror2=0.0;
                verror2=0.0;
            }
            if( recode->herror > 0.0 && recode->verror >= 0.0 )
            {
                st0=stto;
                herror2 += recode->herror*recode->herror;
                verror2 += recode->verror*recode->verror;
            }
        }
        if( st0 )
        {
                sts0=add_colocation_constraint( st0, st, sqrt(herror2), sqrt(verror2) );
                if( sts0 != OK ) sts=sts0;
        }
        /* Process after ### updates */
        st0=st;
        herror2=0.0;
        verror2=0.0;
        for( stn_recode *recode=recode0; recode; recode=recode->next )
        {
            if( recode->datefrom == UNDEFINED_DATE || recode->dateto != UNDEFINED_DATE )
            {
                continue;
            }
            int idto=find_station( net, recode->codeto );
            station *stto=stnptr(idto);
            bool usestn=false;
            if( stto )
            {
                stn_adjustment *sato=stnadj(stto);
                if( sato && sato->obscount > 0 ) usestn=true;
            }
            if( ! usestn )
            {
                /* Update errors */
                if( recode->herror > 0.0 && recode->verror >= 0.0 )
                {
                    herror2 += recode->herror*recode->herror;
                    verror2 += recode->verror*recode->verror;
                }
                /* If disconnected then breaks link with previous versions */
                else
                {
                    st0=stto;
                    herror2=0.0;
                    verror2=0.0;
                }
                continue;
            }
            if( recode->herror > 0.0 && recode->verror >= 0.0 && stto && st0 )
            {
                herror2 += recode->herror*recode->herror;
                verror2 += recode->verror*recode->verror;
                sts0=add_colocation_constraint( stto, st0, sqrt(herror2), sqrt(verror2) );
                if( sts0 != OK ) sts=sts0;
            }
            st0=stto;
            herror2=0.0;
            verror2=0.0;
        }
    }
    return sts;
}

/* This routine ensures that the various flags associated with each
   station are consistent, and then sets st->rowno to the number of
   parameters to calculate for the station.  It returns the total
   number of parameters to be calculated.  It also sets the variable
   floating_stations, used to determine if there are any stations to
   be floated. */

int init_station_rowno( void )
{
    station *st;
    stn_adjustment *sa;
    int i;
    char consistency_check;
    char data_check;
    int nextprm;

    nextprm = 0;
    floating_stations = 0;
    relative_floating = 0;
    consistency_check = program_mode == DATA_CONSISTENCY;
    data_check = program_mode == DATA_CHECK;

    for( i = 0; i++ < number_of_stations(net); )
    {
        st = stnptr(i);
        sa = stnadj( st );

        /* Reject stations for which there are insufficient observations,
           unless they are being floated or fixed */

        if( dimension == 1 ) { sa->flag.adj_h=0; sa->flag.float_h=0; sa->flag.auto_h=0; }
        if( dimension == 2 ) { sa->flag.adj_v=0; sa->flag.float_v=0; sa->flag.auto_v=0; }

        if( data_check )
        {
            sa->flag.adj_h = sa->flag.adj_v = 0;
            sa->flag.float_h = sa->flag.float_v = 0;
            sa->flag.auto_h = sa->flag.auto_v = 0;
        }
        else if( consistency_check )
        {
            sa->flag.adj_h = sa->flag.adj_v = 1;
            sa->flag.float_h = sa->flag.float_v = 0;
            sa->flag.auto_h = sa->flag.auto_v = 0;
        }

        if( (sa->flag.adj_h || sa->flag.adj_v) && ! sa->flag.rejected && station_autofix_reject(i) )
        {
            sa->flag.rejected = 1; sa->flag.autoreject = 1;
        }

        if( sa->flag.rejected )
        {
            sa->flag.adj_h = 0;
            sa->flag.adj_v = 0;
        }

        if( sa->flag.adj_h == 0 ) sa->flag.float_h = 0;
        if( sa->flag.adj_v == 0 ) sa->flag.float_v = 0;
        if( sa->flag.float_h || sa->flag.float_v ) 
        {
            floating_stations = 1;
            if( sa->idcol ) relative_floating=1;
        }

        sa->hrowno = sa->flag.adj_h ? 2 : 0;
        sa->vrowno = sa->flag.adj_v ? 1 : 0;
        nextprm += sa->hrowno + sa->vrowno;

    }

    return nextprm;
}


int find_station_row( int row, char *param, int plen )
{
    int istn, maxstn;
    stn_adjustment *st;

    const char *crdname[] = {"north coordinate", "east coordinate", "height coordinate"};

    maxstn = number_of_stations(net);

    for( istn = 0; istn++ < maxstn; )
    {
        st = stnadj(stnptr( istn ) );
        if( st->hrowno && (st->hrowno==row || st->hrowno==row-1) )
        {
            strncpy( param, crdname[row-st->hrowno], plen );
            param[plen-1] = 0;
            return istn;
        }

        if( st->vrowno == row )
        {
            strncpy( param, crdname[2], plen );
            param[plen-1] = 0;
            return istn;
        }
    }
    return 0;
}

void set_station_obseq( station *st, vector3 dst, void *hA, int irow, double date )
{
    stn_adjustment *sa;
    double denu[3];

    sa = stnadj( st );

    if( deformation && date != UNDEFINED_DATE && calc_deformation( deformation, st, date, denu ) == OK  )
    {
        if( output_deformation && lst )
        {
            fprintf(lst,"Deformation at %-*s, %7.2lf (%7.4lf,%7.4lf,%7.4lf)  %s\n",
                    stn_name_width,st->Code,date_as_year(date),
                    denu[0],denu[1],denu[2],
                    st->Name);
        }
        oe_add_value( hA, irow, -(denu[0]*dst[0] + denu[1]*dst[1] + denu[2]*dst[2]) );
    }

    if( sa->hrowno )
    {
        oe_param( hA, irow, sa->hrowno, dst[0] );
        oe_param( hA, irow, sa->hrowno+1, dst[1] );
    }
    if( sa->vrowno ) oe_param( hA, irow, sa->vrowno, dst[2] );
}

static int float_station_obseq( station *st, void *hA )
{
    stn_adjustment *sa = stnadj( st );
    int nrow=(sa->flag.float_h ? 2 : 0) + (sa->flag.float_v ? 1 : 0);
    if( nrow == 0 ) return nrow;

    int hprm0=sa->hrowno;
    int vprm0=sa->vrowno;
    int hprm1=0;
    int vprm1=0;
    int ncol=1;
    double de=st->ELon;
    double dn=st->ELat;
    double dh=st->OHgt;
    if( sa->idcol )
    {
        station *stcol=stnptr(sa->idcol);
        stn_adjustment *sacol=stnadj(stcol);
        hprm1=sacol->hrowno;
        vprm1=sacol->vrowno;
        ncol=2;
        de=stcol->ELon-de;
        dn=stcol->ELat-dn;
        dh=stcol->OHgt-dh;
    }
    else
    {
        de=sa->initELon-de;
        dn=sa->initELat-dn;
        dh=sa->initOHgt-dh;
    }
    if( ! (hprm0 || hprm1 || vprm0 || vprm1) ) return 0;
    de *= st->dEdLn;
    dn *= st->dNdLt;
    init_oe(hA,nrow,ncol,OE_DIAGONAL_CVR);
    int vrow=1;
    if( sa->flag.float_h )
    {
        double var=sa->herror*sa->herror;
        oe_value( hA, 1, de );
        if( hprm0 ) oe_param( hA, 1, hprm0, 1.0 );
        if( hprm1 ) oe_param( hA, 1, hprm1, -1.0 );
        oe_covar( hA, 1, 1, var );

        oe_value( hA, 2, dn );
        if( hprm0 ) oe_param( hA, 2, hprm0+1, 1.0 );
        if( hprm1 ) oe_param( hA, 2, hprm1+1, -1.0 );
        oe_covar( hA, 2, 2, (double) sa->herror * sa->herror );

        vrow=3;
    }
    if( sa->flag.float_v )
    {
        double var=sa->verror*sa->verror;
        oe_value( hA, vrow, dh );
        if( vprm0 ) oe_param( hA, vrow, vprm0, 1.0 );
        if( vprm1 ) oe_param( hA, vrow, vprm1, -1.0 );
        oe_covar( hA, vrow, vrow, var );
    }
    return nrow;
}


void sum_floating_stations( int iteration )
{
    /*  ... need to update to include co-location constraint
     */
    char header[20];
    int istn, maxstn;
    station *st;
    void *hA;
    int nfloat=0;

    if( !floating_stations ) return;

    if( output_observation_equations )
    {
        sprintf(header,"float_stations_%d",iteration);
        print_json_start(lst,header);
        fprintf(lst,"\n{ \"float_stations\": [\n");
    }

    hA = create_oe( nprm );

    maxstn = number_of_stations(net);
    for( istn = 0; istn++ < maxstn; )
    {
        st = stnptr( istn );
        int nfprm=float_station_obseq(st,hA);
        if( ! nfprm ) continue;
        lsq_sum_obseqn( hA );
        if( output_observation_equations )
        {
            char source[100];
            sprintf(source,"{\"station\":\"%.20s\"}",st->Code);
            if( nfloat ) fprintf(lst,",\n");
            nfloat++;
            print_obseqn_json( lst, hA, source, 2 );
        }
    }

    if( output_observation_equations )
    {
        fprintf(lst,"]}\n");
        print_json_end(lst,header);
    }

    delete_oe( hA );
}


void update_station_coords( void )
{
    int istn, maxstn;
    station *st;
    stn_adjustment *sa;
    double change[2];
    double lat, lon, hgt;

    maxstn = number_of_stations(net);
    for( istn = 0; istn++ < maxstn; )
    {
        st = stnptr( istn );
        sa = stnadj( st );
        if( sa->hrowno || sa->vrowno )
        {

            lat = st->ELat;
            lon = st->ELon;
            hgt = st->OHgt;

            if( sa->hrowno )
            {
                int rows[2];
                rows[1] = sa->hrowno;
                rows[0] = rows[1]-1;
                lsq_get_params( rows, 2, change, NULL );
                lon += change[0] / st->dEdLn;
                lat += change[1] / st->dNdLt;
            }

            if( sa->vrowno )
            {
                int rows[1];
                rows[0] = sa->vrowno-1;
                lsq_get_params( rows, 1, change, NULL );
                hgt += change[0];
            }

            modify_network_station_coords( net, st, lat, lon, hgt );
        }
    }

}

void print_coordinate_changes( FILE *out )
{
    station *st;
    stn_adjustment *sa;
    double change[2];

    fprintf(out,"\nChanges to station coordinates\n\n");
    fprintf(out,"%-*s     East      North       Up       Name\n\n",
            stn_name_width,"Code");

    for( reset_station_list(net, (int)output_sorted_stations);
            NULL != (st = next_station( net )); )
    {
        sa = stnadj( st );
        if( sa->hrowno || sa->vrowno )
        {

            fprintf(out,"%-*s  ",stn_name_width,st->Code);

            if( sa->hrowno )
            {
                int rows[2];
                rows[1] = sa->hrowno;
                rows[0] = rows[1]-1;
                lsq_get_params( rows, 2, change, NULL );
                fprintf(out,"%9.*lf  %9.*lf  ", (int) coord_precision,
                        change[0], (int) coord_precision, change[1]);
            }
            else
            {
                fprintf(out,"    -          -      ");
            }

            if( sa->vrowno )
            {
                int rows[1];
                rows[0] = sa->vrowno-1;
                lsq_get_params( rows, 1, change, NULL );
                fprintf(out,"%9.*lf",(int) coord_precision,change[0]);
            }
            else
            {
                fprintf(out,"    -    ");
            }

            fprintf(out,"   %s\n",st->Name);
        }
    }

}


static void calc_confidence_limit_factors( double *perrmult, double *phgtmult )
{
    double prob;
    int dofd;
    double hgterr_factor;
    double errell_factor;

    if( errconflim )
    {
        prob = (100 - errconfval) / 100.0;
        dofd = apriori ? 0 : dof;
        hgterr_factor = inv_f_distn( prob, 1, dofd );
        hgterr_factor = hgterr_factor > 0.0 ? sqrt(hgterr_factor) : 0.0;
        errell_factor = inv_f_distn( prob, 2, dofd );
        errell_factor = errell_factor > 0.0 ? sqrt(errell_factor*2.0) : 0.0;
    }
    else
    {
        errell_factor = hgterr_factor = errconfval;
    }

    if( ! apriori )
    {
        errell_factor *= seu;
        hgterr_factor *= seu;
    }

    *perrmult=errell_factor;
    *phgtmult=hgterr_factor;
}

void print_adjusted_coordinates( FILE *lst )
{
    station *st;
    stn_adjustment *sa;
    projection *prj;
    double cvr[6], emax, emin, brng, OHgt, northing, easting, hgt;
    double dn, de, dh;
    char adjusted;
    char ellipsoidal;
    double errmult, hgtmult;
    unsigned char projection_coords;
    output_string_def os;
    coordsys *cs;

    print_section_header(lst,"STATION COORDINATES");
    print_zero_inverse_warning( lst );
    print_convergence_warning( lst );

    output_string_to_file( &os, lst );
    cs = net->crdsys;
    write_output_string( &os, "Coordinate system: " );
    write_output_string( &os, cs->name );
    write_output_string( &os, "\n" );
    describe_ellipsoid( &os, cs->rf->el );
    describe_projection( &os, cs->prj );

    ellipsoidal = net->options & NW_ELLIPSOIDAL_HEIGHTS;

    fprintf(lst, "\nStation heights are %s heights.\n",
            ellipsoidal ? "ellipsoidal" : "orthometric" );
    if( ! (net->options & NW_GEOID_HEIGHTS) )
    {
        fprintf(lst, "Geoid heights not defined - orthometric and ellipsoidal heights are the same.\n");
    }

    if( deformation )
    {
        fprintf(lst,"\nThe following deformation model has been applied\n");
        print_deformation_model( deformation, lst, "");
        fprintf(lst,"\n");
    }

    if( has_deformation_model(net->crdsys) && ignore_deformation )
    {
        fprintf(lst,"\nNote: the deformation model associated with %s has not been used\n\n",
                cs->name);
    }

    adjusted = program_mode != PREANALYSIS;

    calc_confidence_limit_factors( &errmult, &hgtmult );

    if( errconflim )
    {
        fprintf(lst,"The error ellipse and height error are the %s %.2f confidence limits\n",
            apriori ? "apriori" : "aposteriori", errconfval );
    }
    else if ( errconfval == 1.0 )
    {
        fprintf(lst,"The error ellipse and height error are the %s errors\n",
            apriori ? "apriori" : "aposteriori" );
    }
    else
    {
        fprintf(lst,"The error ellipse and height error are %s %.1f times standard errors\n",
            apriori ? "apriori" : "aposteriori", errconfval );
    }

    if( output_rejected_stations )
    {
        fprintf(lst,"\nRejected stations are flagged %c\n",REJECTED_STN_FLAG );
    }
    else
    {
        fprintf(lst,"\nRejected stations are not listed\n");
    }

    fprintf(lst,"\n");

    projection_coords = is_projection( net->crdsys ) ? 1 : 0;
    prj = net->crdsys->prj;

    if( projection_coords )
    {
        fprintf(lst," %-*s      Easting  %s  Error  Brng  Name\n",
                stn_name_width,"Code",adjusted ? " Adjustment":"");
        fprintf(lst," %-*s      Northing %s Ellipse\n",
                stn_name_width,"",adjusted ? "  (metres) ":"");
        fprintf(lst," %-*s      Height   %s Hgt err\n",
                stn_name_width,"",adjusted ? "           ":"");
    }
    else
    {
        fprintf(lst," %-*s      Latitude      %s  Error  Brng  Name\n",
                stn_name_width,"Code",adjusted ? " Adjustment":"");
        fprintf(lst," %-*s      Longitude     %s Ellipse\n",
                stn_name_width,"",adjusted ? "  (metres) ":"");
        fprintf(lst," %-*s      Height        %s Hgt err\n",
                stn_name_width,"",adjusted ? "           ":"");
        setup_latlon_format();
    }

    for( reset_station_list(net,(int)output_sorted_stations);
            NULL != (st = next_station(net)); )
    {

        sa = stnadj(st);

        if( sa->flag.ignored ) continue;
        if( sa->flag.rejected && !output_rejected_stations ) continue;

        de = dn = dh = 0.0;
        emax = emin = OHgt = brng = 0.0;

        if( sa->hrowno || sa->vrowno )
        {
            get_station_covariance( st, cvr );
            calc_error_ellipse( cvr, &emax, &emin, &brng );
            brng *= RTOD;
            while(brng < 0) brng += 180;
            while(brng > 180) brng -= 180;
            OHgt = cvr[5] > 0.0 ? sqrt(cvr[5]) : 0.0;

            dn = ( st->ELat - sa->initELat ) * st->dNdLt;
            de = ( st->ELon - sa->initELon ) * st->dEdLn;
            if( projection_coords ) { dh = dn; dn = de; de = dh; }
            dh = st->OHgt - sa->initOHgt;
        }

        easting = northing = 0.0;
        if( projection_coords )
            geog_to_proj( prj, st->ELon, st->ELat, &easting, &northing );

        /* Print the first line - latitude or easting */

        fprintf(lst,"\n%c%-*s ",stnadj(st)->flag.rejected ? REJECTED_STN_FLAG : ' ',
                stn_name_width,st->Code);
        if( projection_coords )
        {
            fprintf(lst,"%13.*lf  ",(int) coord_precision, easting);
        }
        else
        {
            fprintf(lst,"%s  ",dms_string(st->ELat*RTOD,latfmt,NULL));
        }

        if( adjusted )
        {
            if( stnadj(st)->flag.adj_h ) fprintf(lst,"%9.4lf  ",dn );
            else fprintf(lst,"    -      ");
        }

        if( stnadj(st)->flag.adj_h ) fprintf(lst,"%8.4lf %3.0lf  ",emax*errmult,brng);
        else fprintf(lst,"   -          ");
        fprintf(lst,"%s\n",st->Name);


        /* Print the second line, longitude or northing */

        fprintf(lst,"%*s",stn_name_width,"");
        if( projection_coords )
        {
            fprintf(lst,"  %13.*lf  ",(int) coord_precision, northing);
        }
        else
        {
            fprintf(lst,"  %s  ",dms_string(st->ELon*RTOD,lonfmt,NULL));
        }

        if( adjusted )
        {
            if( stnadj(st)->flag.adj_h ) fprintf(lst,"%9.4lf  ",de );
            else fprintf(lst,"    -      ");
        }


        if( stnadj(st)->flag.adj_h )
        {
            brng += 90.0;
            if( brng >= 180.0 ) brng -= 180.0;
            fprintf(lst,"%8.4lf %3.0lf\n",emin*errmult,brng);
        }
        else
        {
            fprintf(lst,"   -\n");
        }

        /* Print the third line - height */

        fprintf(lst,"%*s",stn_name_width,"");
        hgt = st->OHgt;
        if( ellipsoidal ) hgt += st->GUnd;
        if( projection_coords )
        {
            fprintf(lst,"  %13.*lf  ",(int) coord_precision, hgt);
        }
        else
        {
            fprintf(lst,"  %14.*lf      ",(int) coord_precision, hgt);
        }

        if( adjusted )
        {
            if( stnadj(st)->flag.adj_v ) fprintf(lst,"%9.4lf  ",dh );
            else fprintf(lst,"    -      ");
        }

        if( stnadj(st)->flag.adj_v ) fprintf(lst,"%8.4lf\n",OHgt*hgtmult);
        else fprintf(lst,"   -\n");
    }
    print_section_footer(lst);

}


void write_station_csv()
{
    station *st;
    stn_adjustment *sa;
    projection *prj;
    ellipsoid *elp;
    double cvr[6], emax, emin, brng, OHgt, northing, easting, height;
    double dn, de, dh;
    unsigned char projection_coords;
    unsigned char geocentric_coords;
    output_csv *csv;
    int i;
    int defl;
    int geoid;
    int ellipsoidal;
    int adjusted;
    int autofix;
    void *hA;

    geoid = net->options & NW_GEOID_HEIGHTS;
    defl = net->options & NW_DEFLECTIONS;
    ellipsoidal = net->options & NW_ELLIPSOIDAL_HEIGHTS;
    adjusted = 0;
    autofix = 0;
    for( reset_station_list(net,(int)output_sorted_stations);
            NULL != (st = next_station(net)); )
    {

        sa = stnadj(st);
        if( sa->hrowno || sa->vrowno ) adjusted = 1;
        if( sa->flag.auto_h || sa->flag.auto_v ) autofix=1;
    }

    geocentric_coords = is_geocentric( net->crdsys ) ? 1 : 0;
    projection_coords = is_projection( net->crdsys ) ? 1 : 0;
    prj = net->crdsys->prj;
    elp = net->crdsys->rf->el;

    csv = open_snap_output_csv("stn");
    if( ! csv ) return;

    write_csv_header( csv, "code" );
    write_csv_header( csv, "crdsys" );
    if( geocentric_coords )
    {
        /* Set ellipsoidal as true so that ellipsoidal height is calced */
        ellipsoidal=1;
        write_csv_header( csv,"X");
        write_csv_header( csv,"Y");
        write_csv_header( csv,"Z");
    }
    else
    {
        if( projection_coords )
        {
            write_csv_header( csv,"easting");
            write_csv_header( csv,"northing");
        }
        else
        {
            write_csv_header( csv,"longitude");
            write_csv_header( csv,"latitude");
        }
        write_csv_header( csv, ellipsoidal ? "ellheight" : "height" );
        write_csv_header( csv, "height_type" );
    }
    if( geoid ) write_csv_header( csv, "geoidhgt" );
    if( defl ) { write_csv_header( csv, "xi"); write_csv_header( csv, "eta" ); }
    if( autofix )
    {
        write_csv_header( csv, "autofix" );
    }
    if( adjusted )
    {
        write_csv_header( csv, "mode" );
        write_csv_header( csv, "adj_e" );
        write_csv_header( csv, "adj_n" );
        write_csv_header( csv, "adj_h" );

        write_csv_header( csv, "errell_max" );
        write_csv_header( csv, "errell_min" );
        write_csv_header( csv, "errell_bmax" );
        write_csv_header( csv, "errhgt" );
        if( projection_coords )
        {
            write_csv_header( csv,"easting_init");
            write_csv_header( csv,"northing_init");
        }
        else
        {
            write_csv_header( csv,"longitude_init");
            write_csv_header( csv,"latitude_init");
        }
        write_csv_header( csv, ellipsoidal ? "ellheight_init" : "height_init" );

        if( floating_stations )
        {
            if( relative_floating )
            {
                write_csv_header(csv,"rel_station");
            }
            write_csv_header(csv,"float_hor_err");
            write_csv_header(csv,"float_vrt_err");
            write_csv_header(csv,"float_de");
            write_csv_header(csv,"float_dn");
            write_csv_header(csv,"float_errell_max");
            write_csv_header(csv,"float_errell_min");
            write_csv_header(csv,"float_errell_bmax");
            write_csv_header(csv,"float_hor_stdres");
            write_csv_header(csv,"float_dh");
            write_csv_header(csv,"float_errhgt");
            write_csv_header(csv,"float_vrt_stdres");
        }
    }

    for( i = 0; i < network_classification_count(net); i++ )
    {
        char fieldname[33];
        strcpy(fieldname,"c_");
        strncpy(fieldname+2,network_class_name(net,i+1),30);
        fieldname[32] = 0;
        write_csv_header(csv,fieldname);
    }
    write_csv_header(csv,"name");
    if( output_csv_shape ) write_csv_header(csv,"shape");
    end_output_csv_record(csv);

    hA=create_oe(nprm);

    for( reset_station_list(net,(int)output_sorted_stations);
            NULL != (st = next_station(net)); )
    {

        sa = stnadj(st);

        if( sa->flag.ignored ) continue;

        height = st->OHgt;
        if( ellipsoidal ) height += st->GUnd;
        de = dn = dh = 0.0;
        emax = emin = OHgt = brng = 0.0;

        if( sa->hrowno || sa->vrowno )
        {
            get_station_covariance( st, cvr );
            calc_error_ellipse( cvr, &emax, &emin, &brng );
            brng *= RTOD;
            while(brng < 0) brng += 180;
            while(brng > 180) brng -= 180;
            OHgt = cvr[5] > 0.0 ? sqrt(cvr[5]) : 0.0;

            dn = ( st->ELat - sa->initELat ) * st->dNdLt;
            de = ( st->ELon - sa->initELon ) * st->dEdLn;
            dh = st->OHgt - sa->initOHgt;
        }

        easting = northing = 0.0;

        write_csv_string( csv, st->Code );
        write_csv_string(csv,net->crdsys->code);

        if( geocentric_coords )
        {
            double llh[3], xyz[3];
            llh[CRD_LON]=st->ELon;
            llh[CRD_LAT]=st->ELat;
            llh[CRD_HGT]=height;
            llh_to_xyz( elp, llh, xyz, 0, 0);
            write_csv_double( csv, xyz[CRD_X], coord_precision );
            write_csv_double( csv, xyz[CRD_Y], coord_precision );
            write_csv_double( csv, xyz[CRD_Z], coord_precision );
        }
        else
        {
            if( projection_coords )
            {
                geog_to_proj( prj, st->ELon, st->ELat, &easting, &northing );
                write_csv_double( csv, easting, coord_precision );
                write_csv_double( csv, northing, coord_precision );
            }
            else
            {
                write_csv_double( csv, st->ELon*RTOD, coord_precision+5 );
                write_csv_double( csv, st->ELat*RTOD, coord_precision+5 );
            }

            write_csv_double( csv, height, coord_precision );
            write_csv_string( csv, ellipsoidal ? "ellipsoidal" : "orthometric" );
        }
        if( geoid ) write_csv_double( csv, st->GUnd, coord_precision );
        if( defl )
        {
            write_csv_double( csv, st->GXi * RTOS, 2 );
            write_csv_double( csv, st->GEta * RTOS, 2 );
        }

        if( autofix )
        {
            char mode[3] = { '-', '-', 0 };
            if( sa->flag.auto_h) mode[0]='H';
            if( sa->flag.auto_v) mode[1]='V'; 
            write_csv_string(csv,mode);
        }
        if( adjusted )
        {
            if( sa->flag.autoreject ) write_csv_string(csv,"*");
            else if( sa->flag.rejected ) write_csv_string(csv,"**");
            else
            {
                char mode[3] = { '-', '-', 0 };
                if( sa->flag.float_h ) mode[0] = 'h';
                else if( sa->flag.adj_h ) mode[0] = 'H';
                if( sa->flag.float_v ) mode[1] = 'v';
                else if( sa->flag.adj_v ) mode[1] = 'V';
                write_csv_string(csv,mode);
            }

            write_csv_double( csv, de, coord_precision );
            write_csv_double( csv, dn, coord_precision );
            write_csv_double( csv, dh, coord_precision );

            write_csv_double( csv, emax, coord_precision );
            write_csv_double( csv, emin, coord_precision );
            write_csv_double( csv, brng, 1 );
            write_csv_double( csv, OHgt, coord_precision );
            if( projection_coords )
            {
                geog_to_proj( prj, sa->initELon, sa->initELat, &easting, &northing );
                write_csv_double( csv, easting, coord_precision );
                write_csv_double( csv, northing, coord_precision );
            }
            else
            {
                write_csv_double( csv, sa->initELon*RTOD, coord_precision+5 );
                write_csv_double( csv, sa->initELat*RTOD, coord_precision+5 );
            }

            write_csv_double( csv, height-dh, coord_precision );

            if( floating_stations )
            {
                if( relative_floating )
                {
                    station *stcol=0;
                    if( sa->idcol )
                    {
                        stcol=stnptr(sa->idcol);
                    }
                    if( stcol )
                    {
                        write_csv_string(csv,stcol->Code);
                    }
                    else
                    {
                        write_csv_null_field(csv);
                    }
                }
                if( sa->flag.float_h ) 
                {
                    write_csv_double(csv,sa->herror,coord_precision);
                }
                else
                {
                    write_csv_null_field(csv);
                }
                if( sa->flag.float_v ) 
                {
                    write_csv_double(csv,sa->verror,coord_precision);
                }
                else
                {
                    write_csv_null_field(csv);
                }
                
                int nfprm=float_station_obseq( st, hA );
                double calc[3];
                double calccvr[6];
                double res[3]={0.0,0.0,0.0};
                if( nfprm )
                {
                    lsq_calc_obs( hA, calc, res, 0, 0, 0, calccvr, cvr );
                }
                    
                int vrow=0;
                if( nfprm == 2 || nfprm == 3 )
                {
                    vrow=2;
                    calc_error_ellipse( cvr, &emax, &emin, &brng );
                    double cs=cos(brng);
                    double sn=sin(brng);
                    brng *= RTOD;
                    while(brng < 0) brng += 180;
                    while(brng > 180) brng -= 180;
                    write_csv_double(csv,-res[0],coord_precision);
                    write_csv_double(csv,-res[1],coord_precision);
                    write_csv_double(csv,emax,coord_precision);
                    write_csv_double(csv,emin,coord_precision);
                    write_csv_double(csv,brng,1);
                    if( emax > 1.0e-5 )
                    {
                        double dmax=cs*res[0]+sn*res[1];
                        dmax=(dmax*dmax)/(emax*emax);
                        if( emin > 1.0e-5 )
                        {
                            double dmin=sn*res[0]-cs*res[1];
                            dmax += (dmin*dmin)/(emin*emin);
                        }
                        dmax=sqrt(dmax);
                        write_csv_double(csv,dmax,3);
                    }
                    else
                    {
                        write_csv_null_field( csv );
                    }
                }
                else
                {
                    write_csv_null_field( csv );
                    write_csv_null_field( csv );
                    write_csv_null_field( csv );
                    write_csv_null_field( csv );
                    write_csv_null_field( csv );
                    write_csv_null_field( csv );
                }
                if( nfprm == 1 || nfprm == 3 )
                {
                    write_csv_double(csv,-res[vrow],coord_precision);
                    double verr=sqrt(fabs(Lij(cvr,vrow,vrow)));
                    write_csv_double(csv,verr,coord_precision);
                    if( verr < 1.0e-5 )
                    {
                        write_csv_null_field( csv );
                    }
                    else
                    {
                        write_csv_double( csv, fabs(res[vrow])/verr, 3 );
                    }
                }
                else
                {
                    write_csv_null_field( csv );
                    write_csv_null_field( csv );
                    write_csv_null_field( csv );
                }
            }
        }

        for( i = 0; i < network_classification_count(net); i++ )
        {
            int iclass = get_station_class( st, i+1 );
            write_csv_string( csv, network_class_value(net, i+1, iclass ));
        }

        write_csv_string( csv, st->Name );
        if( output_csv_shape )
        {
            char wkt[128];
            if( projection_coords )
            {
                sprintf(wkt,"POINT(%.*lf %.*lf)",
                        coord_precision,easting,coord_precision,northing);
            }
            else
            {
                sprintf(wkt,"POINT(%.*lf %.*lf)",
                        coord_precision+5,st->ELon*RTOD,coord_precision+5,st->ELat*RTOD);
            }
            write_csv_string( csv, wkt );
        }
        end_output_csv_record( csv );
    }
    delete_oe(hA);
    close_output_csv( csv );
}

void print_floated_stations( FILE *out )
{
    station *st;
    stn_adjustment *sa;
    const char *coordname[3] = { "East", "North", "Up" };
    double calccvr[6];
    double rescvr[6];
    double calc[3];
    double res[3];
    int axis;
    double semult;
    int rowno;

    if( !floating_stations ) return;

    print_section_header(lst,"FLOATED STATIONS");
    print_zero_inverse_warning( lst );
    print_convergence_warning( lst );

    fprintf(lst,"\nThis list shows %s errors\n",
            apriori ? "apriori" : "aposteriori" );
    fprintf(out,"\nAdjustments outside the %6.3lf%% confidence limit %sare flagged %s\n",
            flag_level[0], taumax[0] ? "for the maximum ":"", FLAG1 );
    fprintf(out,"Adjustments outside the %6.3lf%% confidence limit %sare flagged %s\n",
            flag_level[1], taumax[1] ? "for the maximum ":"", FLAG2 );
    if( relative_floating )
    {
        fprintf(out,"The rel column lists the reference station for co-location constraints\n");
    }

    if( apriori )
    {
        fprintf(out,"Significance is based on the Normal distribution function\n");
    }
    else if( dof < 1 )
    {
        fprintf(out,"The a posteriori significance of adjustments cannot be calculated\n");
    }
    else
    {
        fprintf(out,"Significance is based on the Tau distribution with %ld degrees of freedom\n",
                (long) dof);
    }

    semult = apriori ? 1 : seu;

    fprintf(out,"\n%-*s %-*s  coord      error      calc.err    residual    adj.err    std.res\n",
            stn_name_width, "Code",
            relative_floating ? stn_name_width : 0, 
            relative_floating ? "rel" : "" );


    void *hA = create_oe( nprm );

    for( reset_station_list(net,(int)output_sorted_stations);
            NULL != (st = next_station(net)); )
    {
        int nfprm=float_station_obseq(st,hA);
        if( ! nfprm ) continue;
    
        int axis0=nfprm==1 ? 2 : 0;
        int axis1=nfprm==2 ? 2 : 3;

        lsq_calc_obs( hA, calc, res, 0, 0, 0, calccvr, rescvr );
        sa=stnadj(st);
        station *stcol=0;
        if( relative_floating && sa->idcol ) 
        {
            stcol=stnptr(sa->idcol);
        }

        rowno=0;
        for( axis = axis0; axis < axis1; axis++ )
        {
            if( rowno==0 ) fprintf( out, "\n");
            double resval=res[rowno];
            double ser=sqrt(fabs(Lij(rescvr,rowno,rowno)))*semult;
            fprintf( out, "%-*s %-*s  %-5s  %10.4lf  %10.4lf  %10.4lf  %10.4lf ",
                     stn_name_width, (rowno==0 ? st->Code : ""), 
                     relative_floating ? stn_name_width+1 : 0,
                     stcol ? stcol->Code : "",
                     coordname[axis],
                     (axis < 2 ? sa->herror : sa->verror)*semult,
                     sqrt(fabs(Lij(calccvr,rowno,rowno)))*semult,
                     -resval, ser);
            if( ser > 1.0e-5 )
            {
                double stres = fabs(resval)/ ser;
                fprintf( out, "%8.2lf %s\n", stres, residual_flag( 0, 1, stres ));
            }
            else
            {
                fprintf( out, "     -\n");
            }
            rowno++;
            stcol=0;
        }
    }
    delete_oe( hA );
    print_section_footer(lst);
}

void print_station_offsets( FILE *out )
{
    station *st;
    char first=1;

    for( reset_station_list(net,(int)output_sorted_stations);
            NULL != (st = next_station(net)); )
    {
        if( ! station_has_offset(st)) continue;
        if( first )
        {
            first=0;

            print_section_header(lst,"STATION OFFSETS");

            fprintf(lst,"\nThe following coordinate offsets apply to stations in the network.\n");
            fprintf(lst,"Each offset is applied for determining station coordinates based on the date\n"
                    "at which the coordinate is required.\n");
        }

        fprintf(lst,"\n");
        print_station_offset(lst,st);
    }
    if( ! first ) print_section_footer(lst);
}

void max_station_adjustment( double tol, int *pmaxadjstn,
                             double *pmaxadj, int *pnstnadj )
{
    int istn, maxstn;
    stn_adjustment *sa;
    double change[2], adj, maxadj;
    int maxadjstn;
    int nstnadj;
    double tol2;

    maxstn = number_of_stations(net);
    maxadj = 0.0;
    maxadjstn = 0;
    nstnadj = 0;
    tol2 = tol*tol;
    for( istn = 0; istn++ < maxstn; )
    {
        sa = stnadj(stnptr( istn ));
        if( sa->hrowno || sa->vrowno )
        {
            adj = 0.0;
            if( sa->hrowno )
            {
                int rows[2];
                rows[1] = sa->hrowno;
                rows[0] = rows[1]-1;
                lsq_get_params( rows, 2, change, NULL );
                adj = change[0]*change[0] + change[1]*change[1];
            }

            if( sa->vrowno )
            {
                int rows[1];
                rows[0] = sa->vrowno-1;
                lsq_get_params( rows, 1, change, NULL );
                adj += change[0]*change[0];
            }

            if( adj > maxadj )
            {
                maxadj = adj;
                maxadjstn = istn;
            }

            if( adj > tol2 )
            {
                nstnadj++;
            }

        }
    }
    if( maxadj > 0.0 ) maxadj = sqrt(maxadj);

    if( pmaxadj ) *pmaxadj = maxadj;
    if( pmaxadjstn ) *pmaxadjstn = maxadjstn;
    if( pnstnadj ) *pnstnadj = nstnadj;
}



void init_rf_scale_error( double value, int adjust )
{
    scale_error = define_param( "Ref. frame scale error (ppm)", value, adjust );
}


double rf_scale_error( double dist, void *hA, int irow )
{
    if( !scale_error ) return 0;
    dist *= -1.0e-6;
    set_param_obseq( scale_error, hA, irow, dist );
    return param_value( scale_error ) * dist;
}


/* Calculate the axes of an error ellipse given the elements of a
   covariance matrix.  Assumes that the covariance matrix is lower
   triangular for E,N coordinates.  Returns the size and orientation
   of the error ellipse.  The orientation is measured from N through
   E */


void calc_error_ellipse( double cvr[], double *emax, double *emin, double *azemax )
{
    double v1, v2, v3, v4;

    v1 = (cvr[2]+cvr[0])/2.0;
    v2 = (cvr[2]-cvr[0])/2.0;
    v3 = cvr[1];
    v4 = v2*v2+v3*v3;
    if( v4 > 0.0 ) v4 = sqrt(v4);
    *azemax = v4 > 0.0 ? atan2( v3, v2 ) / 2.0 : 0.0 ;
    v2 = v1-v4;
    v1 = v1+v4;
    *emax = v1 > 0.0 ? sqrt(v1) : 0.0;
    *emin = v2 > 0.0 ? sqrt(v2) : 0.0;
}



void get_station_covariance( station *st, double cvr[] )
{
    int i, np;
    double *c;
    double val[3];
    int  col[3];
    stn_adjustment *sa;

    sa = stnadj(st);

    for(i=0; i<6; i++ ) cvr[i] = 0.0;

    np = 0;
    c = cvr;
    if( sa->hrowno )
    {
        col[np++]=sa->hrowno-1; col[np++]=sa->hrowno;
    }
    else
    {
        c += 5;
    }

    if( sa->vrowno ) col[np++] = sa->vrowno-1;

    lsq_get_params( col, np, val, c );
}


void dump_station_covariances( BINARY_FILE *b )
{
    double covar[6];
    int istn, maxstn;

    create_section( b, "STATION_COVARIANCES" );

    for( istn=0, maxstn=number_of_stations(net); istn++ < maxstn; )
    {
        get_station_covariance( stnptr(istn), covar );
        fwrite( covar, sizeof(covar), 1, b->f );
    }

    end_section( b );
}
