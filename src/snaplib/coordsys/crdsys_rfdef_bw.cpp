#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <math.h>
#include "util/errdef.h"
#include "util/iostring.h"
#include "util/pi.h"
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_bw.h"


/// The opaque payload behind a BW14/EULER ref_deformation's data member.
/// File-private to this translation unit - never exposed via a header, so
/// it has no external callers. tmat/shift are the only fields ever
/// mutated after construction - rfdef_parse_bw14def/rfdef_parse_eulerdef
/// each fill in the actual transform coefficients once rf_xyz_create has
/// built the base object with everything else already settled.
struct ref_deformation_xyz
{
    ref_deformation_xyz( std::string description, double refepoch ) :
        description( std::move(description) ), refepoch( refepoch )
    {
        for( int i = 0; i < 3; i++ )
        {
            shift[i] = 0;
            for( int j=0; j<3; j++) tmat[i][j] = 0;
        }
    }
    ref_deformation_xyz( const ref_deformation_xyz& ) = delete;

    const std::string description; ///< Human-readable description of the transform
    const double refepoch;          ///< Reference epoch the transform is relative to
    double tmat[3][3];                ///< Linear transform matrix, filled in after construction
    double shift[3];                    ///< Constant shift vector, filled in after construction
};


#define READ_DOUBLE( name, pdouble ) \
     if( sts == OK ) { \
         bad = name; \
         sts = double_from_string( is.scanner, pdouble ); \
         }


static void rf_xyz_delete( void *pdxyz )
{
    delete (ref_deformation_xyz *) pdxyz;
}

static void *rf_xyz_copy( void *pdxyz )
{
    ref_deformation_xyz *dxyz = (ref_deformation_xyz *) pdxyz;
    ref_deformation_xyz *dxyz2 = new ref_deformation_xyz( dxyz->description, dxyz->refepoch );
    for( int i = 0; i < 3; i++ )
    {
        dxyz2->shift[i] = dxyz->shift[i];
        for( int j = 0; j < 3; j++ ) dxyz2->tmat[i][j] = dxyz->tmat[i][j];
    }
    return dxyz2;
}

static int rf_xyz_identical( void *pgd1, void *pgd2 )
{
    ref_deformation_xyz *gd1 = (ref_deformation_xyz *) pgd1;
    ref_deformation_xyz *gd2 = (ref_deformation_xyz *) pgd2;
    if( gd1->refepoch != gd2->refepoch ) return 0;
    return gd1->description == gd2->description ? 1 : 0;
}

static int rf_xyz_describe(  ref_frame *rf, output_string_def *os )
{
    ref_deformation *def = rf->def;
    ref_deformation_xyz *dxyz = (ref_deformation_xyz *)(def->data);
    if( dxyz && ! dxyz->description.empty() )
    {
        write_output_string(os,dxyz->description.c_str());
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

static ref_deformation_xyz *rf_xyz_create( const std::string &description, double refepoch )
{
    return new ref_deformation_xyz( description, refepoch );
}

ref_deformation *rfdef_parse_bw14def( input_string_def &is )
{
    double refepoch=0;
    double tx=0, ty=0, tz=0;
    double rx=0, ry=0, rz=0;
    double sf=0;
    int sts = OK;
    const char *bad = 0;
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
        std::string errmess = sts == MISSING_DATA ? std::string(bad) + " is missing"
                                                    : "Invalid value for " + std::string(bad);
        report_string_error( is, sts, errmess.c_str() );
        return nullptr;
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

    dxyz = rf_xyz_create( description, refepoch );
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

    return new ref_deformation( "BW14", dxyz, rf_xyz_delete, rf_xyz_copy, rf_xyz_identical,
                                 rf_xyz_describe, rf_xyz_calc, rf_xyz_apply );
}

ref_deformation *rfdef_parse_eulerdef( input_string_def &is )
{

    double refepoch=0;
    double lon=0,lat=0,rate=0;
    int sts = OK;
    const char *bad = 0;
    double clt, slt, cln, sln;
    char description[256];
    ref_deformation_xyz *dxyz;

    READ_DOUBLE( "Euler base epoch", &refepoch );

    READ_DOUBLE( "Euler pole longitude", &lon );
    READ_DOUBLE( "Euler pole latitude", &lat );
    READ_DOUBLE( "Euler rotation rate", &rate );

    if( sts!=  OK && bad)
    {
        std::string errmess = sts == MISSING_DATA ? std::string(bad) + " is missing"
                                                    : "Invalid value for " + std::string(bad);
        report_string_error( is, sts, errmess.c_str() );
        return nullptr;
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

    dxyz = rf_xyz_create( description, refepoch );
    dxyz->tmat[0][0] = dxyz->tmat[1][1] = dxyz->tmat[2][2] = 1.0;
    dxyz->tmat[0][1] = -clt*rate;
    dxyz->tmat[1][0] = clt*rate;
    dxyz->tmat[0][2] = slt*sln*rate;
    dxyz->tmat[2][0] = -slt*sln*rate;
    dxyz->tmat[1][2] = -slt*cln*rate;
    dxyz->tmat[2][1] = slt*cln*rate;

    return new ref_deformation( "EULER", dxyz, rf_xyz_delete, rf_xyz_copy, rf_xyz_identical,
                                 rf_xyz_describe, rf_xyz_calc, rf_xyz_apply );
}
