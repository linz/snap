#include "snapconfig.h"
/* Routines for reading geodetic branch format data files */

/*
   $Log: geoddata.c,v $
   Revision 1.1  1995/12/22 18:44:45  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string>
#include <string_view>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include "util/snapctype.h"

#include "snapdata/geoddata.h"
#include "snapdata/datatype.h"
#include "snapdata/loaddata.h"
#include "util/datafile.h"
#include "util/errdef.h"
#include "util/dateutil.h"
#include "util/pi.h"

using boost::numeric_cast;

static constexpr size_t NAMELEN = 20;

static const struct
{
    std::string_view code;
    int type;
    bool heights;
    bool dms;
    bool refcoef;
}


gb_types[] =
{
    /* Code Type H DMS RC */
    { "DS", SD, true, false, false },
    { "ED", ED, false, false, false },
    { "HA", HA, false, true, false },
    { "AZ", AZ, false, true, false },
    { "VA", ZD, true, true, true }
};


static double gb_date( long ldate, int itime );


int read_gb_data( DATAFILE &d, bool (*check_progress)( DATAFILE &d ) )
{
    d.read_record();   /* Skip the header line */

    d.read_record();   /* Read the file type */

    int dtype = -1;
    bool dms = false;
    bool heights = false;
    bool refcoef = false;
    std::string type;
    if( read_string_field( d.input_string().scanner, type, 2 ) == FieldResult::Ok )
    {
        for( const auto &gb_type : gb_types )
        {
            if( type == gb_type.code )
            {
                dtype = gb_type.type;
                heights = gb_type.heights;
                dms = gb_type.dms;
                refcoef = gb_type.refcoef;
                break;
            }
        }
    }

    if( dtype < 0 )
    {
        d.error( INVALID_DATA, "Missing or invalid type of data file");
        return INVALID_DATA;
    }

    const double errfct = dms ? PI/(180.0*3600.0) : 1.0;
    double fromhgt = 0.0;
    double tohgt = 0.0;
    bool inobs = false;
    int oldfrom = -1;
    int oldto = -1;
    int refclassid = -1;

    d.skip_to_blank_line();   /* Skip over comments section */

    int rtnsts = OK;
    while( d.read_record() == OK )
    {

        if( check_progress && !(*check_progress)( d ) )
        {
            rtnsts = OPERATION_ABORTED;
            break;
        }

        FieldScanner &scanner = d.input_string().scanner;
        std::string fromcode;
        std::string tocode;
        bool sts = read_string_field( scanner, fromcode, NAMELEN ) == FieldResult::Ok &&
                   read_string_field( scanner, tocode, NAMELEN ) == FieldResult::Ok;
        boost::to_upper( fromcode );
        boost::to_upper( tocode );

        /* Reciprocal zenith distances are denoted by 0 station numbers */
        /* In SNAP they are split into the two independent obs */

        int from = inobs && fromcode == "0" ? -1 : numeric_cast<int>( ldt_get_id( ID_STATION, 0, fromcode ) );
        int to = inobs && tocode == "0" ? -1 : numeric_cast<int>( ldt_get_id( ID_STATION, 0, tocode ) );

        if( dtype == ZD && from < 0 && to < 0 )
        {
            from  = oldfrom; to = oldto;
            oldfrom = oldto = -1;
        }
        else
        {
            oldfrom = from;
            oldto = to;
        }

        double value = 0.0;
        double error = 0.0;
        int itime = 0;
        long ldate = 0;
        std::string rcname;

        if( sts )
        {
            sts = ( dms ? read_dms_angle_field( scanner, value ) :
                    read_double_field( scanner, value ) ) == FieldResult::Ok;
        }

        if( sts ) sts = read_double_field( scanner, error ) == FieldResult::Ok;
        if( sts && heights )
        {
            sts = read_double_field( scanner, fromhgt ) == FieldResult::Ok &&
                  read_double_field( scanner, tohgt ) == FieldResult::Ok;
        }
        if( sts && refcoef )
        {
            sts = read_string_field( scanner, rcname, NAMELEN ) == FieldResult::Ok;
        }
        if( sts )
        {
            sts = read_long_field( scanner, ldate ) == FieldResult::Ok &&
                  read_int_field( scanner, itime ) == FieldResult::Ok;
        }

        if( !sts )
        {
            d.error( INVALID_DATA, "Cannot interpret data");
            continue;
        }

        if( from >= 0 )
        {
            if( inobs ) ldt_end_data();
            inobs = false;
            if( from == 0 )
            {
                d.error( INVALID_DATA,
                         "Station number " + fromcode + " in the data file is missing from the coordinate file" );
                continue;
            }

            const double dt = gb_date( ldate, itime );
            ldt_inststn( from, fromhgt );
            ldt_date( dt );
            inobs = true;
        }

        if( !inobs ) continue;

        if( to <= 0 )
        {
            d.error( INVALID_DATA,
                     "Station number " + tocode + " in data file is missing from the coordinate file" );
            continue;
        }

        const bool unused = error < 0;
        if( unused ) error = -error;
        error *= errfct;

        if( error < 1.0e-12 )
        {
            d.error( INVALID_DATA, "Error specified for data is too small");
            continue;
        }

        ldt_tgtstn( to, tohgt );
        ldt_nextdata( dtype );
        ldt_lineno( d.line_number() );
        ldt_value( &value );
        ldt_error( &error );
        if( unused ) ldt_unused();
        if( refcoef )
        {
            if( refclassid == -1 )
            {
                refclassid = numeric_cast<int>( ldt_get_id( ID_CLASSTYPE, 0, coef_class(COEF_CLASS_REFCOEF)->default_classname ) );
            }
            const int nameid = numeric_cast<int>( ldt_get_id( ID_CLASSNAME, 0, rcname ) );
            ldt_classification( refclassid, nameid );
        }
    }

    if( inobs ) ldt_end_data();

    return rtnsts;
}


static double gb_date( long ldate, int itime )
{
    int dy, mon, yr, hr, min;
    double dt;

    yr =  (int) (ldate / 10000);
    mon = (int) (ldate / 100 - yr * 100);
    dy =  (int) (ldate % 100);

    hr =  itime / 100;
    min = itime % 100;

    dt = snap_datetime( dy, mon, yr, hr, min, 0 );
    return dt;
}

