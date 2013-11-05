#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <math.h>
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/errdef.h"
#include "util/iostring.h"
#include "util/pi.h"
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_bw.h"


typedef struct
{
    char *description;
    double refepoch;
    double tmat[3][3];
    double shift[3];
} ref_deformation_xyz;


#define READ_DOUBLE( name, pdouble ) \
     if( sts == OK ) { \
         bad = name; \
         sts = double_from_string( is, pdouble ); \
         }


static void rf_xyz_delete( void *pdxyz )
{
    ref_deformation_xyz *dxyz = (ref_deformation_xyz *) pdxyz;
    if( ! dxyz ) return;
    if( dxyz->description ) check_free( dxyz->description );
    dxyz->description = NULL;
    check_free( dxyz );
}

static void *rf_xyz_copy( void *pdxyz )
{
    ref_deformation_xyz *dxyz = (ref_deformation_xyz *) pdxyz;
    ref_deformation_xyz *dxyz2 = (ref_deformation_xyz *) check_malloc( sizeof(ref_deformation_xyz) );
    memcpy( dxyz2, dxyz, sizeof(ref_deformation_xyz));
    dxyz2->description = copy_string( dxyz->description );
    return dxyz2;
}

static int rf_xyz_identical( void *pgd1, void *pgd2 )
{
    ref_deformation_xyz *gd1 = (ref_deformation_xyz *) pgd1;
    ref_deformation_xyz *gd2 = (ref_deformation_xyz *) pgd2;
    if( gd1->refepoch != gd2->refepoch ) return 0;
    if( strcmp(gd1->description,gd2->description) != 0 ) return 0;
    return 1;
}

static int rf_xyz_describe(  ref_frame *rf, output_string_def *os )
{
    ref_deformation *def = rf->def;
    ref_deformation_xyz *dxyz = (ref_deformation_xyz *)(def->data);
    if( dxyz && dxyz->description )
    {
        write_output_string(os,dxyz->description);
    }
    return OK;
}

static void calc_dxyz( ref_deformation_xyz *dxyz, double xyz[3], double years, double xyzdif[3] )
{
    int i, j;
    for( i = 0; i < 3; i++ )
    {
        double dv = dxyz->shift[i];
        for( j=0; j < 3; j++ ) dv += dxyz->tmat[i][j]*xyz[j];
        xyzdif[i] = dv*years;
    }
}

static int rf_xyz_calc( ref_frame *rf, double lon, double lat, double epoch, double denu[3])
{
    double xyz[3], xyzdif[3], dedln, dndlt;
    int i;
    ref_deformation *def = rf->def;
    ref_deformation_xyz *dxyz = (ref_deformation_xyz *)(def->data);
    xyz[CRD_LON] = lon;
    xyz[CRD_LAT] = lat;
    xyz[CRD_HGT] = 0.0;

    llh_to_xyz( rf->el, xyz, xyz,&dedln,&dndlt);
    calc_dxyz(dxyz,xyz,epoch-dxyz->refepoch,xyzdif);
    for( i = 0; i < 3; i++ ) xyz[i] += xyzdif[i];
    xyz_to_llh( rf->el, xyz, xyz );
    denu[0] = (xyz[CRD_LON]-lon)*dedln;
    denu[1] = (xyz[CRD_LAT]-lat)*dndlt;
    denu[2] = xyz[CRD_HGT];
    return OK;
}

static int rf_xyz_apply( ref_frame *rf,  double llh[3], double epochfrom, double epochto )
{
    double xyz[3], xyzdif[3];
    int i;
    ref_deformation *def = rf->def;
    ref_deformation_xyz *dxyz = (ref_deformation_xyz *)(def->data);
    if( epochfrom == 0 ) epochfrom = dxyz->refepoch;
    if( epochto == 0 ) epochto = dxyz->refepoch;
    if( epochfrom == epochto ) return OK;
    llh_to_xyz( rf->el, llh, xyz, 0, 0);
    calc_dxyz(dxyz,xyz,epochfrom-epochto,xyzdif);
    for( i = 0; i < 3; i++ ) xyz[i] += xyzdif[i];
    xyz_to_llh( rf->el, xyz, llh );
    return OK;
}

static ref_deformation_xyz *rf_xyz_create( ref_deformation *def, char *description, double refepoch )
{
    int i, j;
    ref_deformation_xyz *dxyz;
    dxyz = (ref_deformation_xyz *) check_malloc( sizeof(ref_deformation_xyz) );
    dxyz->description = copy_string(description);
    dxyz->refepoch = refepoch;
    for( i = 0; i < 3; i++ )
    {
        dxyz->shift[i] = 0;
        for( j=0; j<3; j++) dxyz->tmat[i][j] = 0;
    }

    def->data = dxyz;
    def->apply_llh = rf_xyz_apply;
    def->calc_denu = rf_xyz_calc;
    def->delete_func = rf_xyz_delete;
    def->describe_func = rf_xyz_describe;
    def->identical = rf_xyz_identical;
    return dxyz;
}

int rfdef_parse_bw14def( ref_deformation *def, input_string_def *is )
{
    double refepoch=0;
    double tx=0, ty=0, tz=0;
    double rx=0, ry=0, rz=0;
    double sf=0;
    int sts = OK;
    char *bad = 0;
    ref_deformation_xyz *dxyz;
    char description[256];

    READ_DOUBLE( "deformation epoch", &refepoch );

    READ_DOUBLE( "x translation rate", &tx );
    READ_DOUBLE( "y translation rate", &ty );
    READ_DOUBLE( "z translation rate", &tz );

    READ_DOUBLE( "x rotation rate", &rx );
    READ_DOUBLE( "y rotation rate", &ry );
    READ_DOUBLE( "z rotation rate", &rz );

    READ_DOUBLE( "scale change rate", &sf );

    if( sts !=  OK && bad)
    {
        char errmess[80];
        strcpy(errmess,bad);
        if( sts == MISSING_DATA )
        {
            strcpy(errmess,bad);
            strcat(errmess," is missing");
        }
        else
        {
            strcpy(errmess,"Invalid value for ");
            strcat(errmess,bad);
        }
        report_string_error( is, sts, errmess );
        return sts;
    }

    sprintf(description,"14 parameter Bursa-Wolf transformation referenced to epoch %.1lf\n",
            refepoch);
    if( tx != 0.0 || ty != 0.0 || tz != 0.0 )
    {
        sprintf(description+strlen(description),
                "    translation %.3lf %.3lf %.3lf mm/year\n",tx,ty,tz);
    }
    if( rx != 0.0 || ry != 0.0 || rz != 0.0 )
    {
        sprintf(description+strlen(description),
                "    rotation %.3lf %.3lf %.3lf msec/year\n",rx,ry,rz);
    }
    if( sf != 0.0 )
    {
        sprintf(description+strlen(description),
                "    scale change %.3lf ppb/year\n",sf);
    }

    dxyz = rf_xyz_create( def, description, refepoch );
    dxyz->shift[0] = tx*0.001;
    dxyz->shift[1] = ty*0.001;
    dxyz->shift[2] = tz*0.001;
    dxyz->tmat[0][0] = dxyz->tmat[1][1] = dxyz->tmat[2][2] = sf*1.0e-9;
    rx *= STOR*0.001;
    ry *= STOR*0.001;
    rz *= STOR*0.001;
    dxyz->tmat[0][1] = -rz;
    dxyz->tmat[1][0] = rz;
    dxyz->tmat[0][2] = ry;
    dxyz->tmat[2][0] = -ry;
    dxyz->tmat[1][2] = -rz;
    dxyz->tmat[2][1] = rz;

    return OK;
}

int rfdef_parse_eulerdef( ref_deformation *def, input_string_def *is )
{

    double refepoch=0;
    double lon=0,lat=0,rate=0;
    int sts = OK;
    char *bad = 0;
    double clt, slt, cln, sln;
    char description[256];
    ref_deformation_xyz *dxyz;

    READ_DOUBLE( "Euler base epoch", &refepoch );

    READ_DOUBLE( "Euler pole longitude", &lon );
    READ_DOUBLE( "Euler pole latitude", &lat );
    READ_DOUBLE( "Euler rotation rate", &rate );

    if( sts!=  OK && bad)
    {
        char errmess[80];
        strcpy(errmess,bad);
        if( sts == MISSING_DATA )
        {
            strcpy(errmess,bad);
            strcat(errmess," is missing");
        }
        else
        {
            strcpy("Invalid value for ",errmess);
            strcat(errmess,bad);
        }
        report_string_error( is, sts, errmess );
        return sts;
    }

    sprintf(description,"Euler rotation referenced to epoch %.1lf\n",
            refepoch);
    sprintf(description+strlen(description),
            "    Pole of rotation lon %.1lf lat %.1lf\n",lon,lat);
    sprintf(description+strlen(description),
            "    Rotation rate %.3lf msec/year\n",rate);
    rate *= STOR*0.001;
    lat *= DTOR;
    lon *= DTOR;
    clt = cos(lat);
    slt = sin(lat);
    cln = cos(lon);
    sln = sin(lon);

    dxyz = rf_xyz_create( def, description, refepoch );
    dxyz->tmat[0][0] = dxyz->tmat[1][1] = dxyz->tmat[2][2] = 1.0;
    dxyz->tmat[0][1] = -clt*rate;
    dxyz->tmat[1][0] = clt*rate;
    dxyz->tmat[0][2] = slt*sln*rate;
    dxyz->tmat[2][0] = -slt*sln*rate;
    dxyz->tmat[1][2] = -slt*cln*rate;
    dxyz->tmat[2][1] = slt*cln*rate;

    return OK;
}
