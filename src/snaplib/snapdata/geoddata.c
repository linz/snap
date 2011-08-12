#include "snapconfig.h"
/* Routines for reading geodetic branch format data files */

/*
   $Log: geoddata.c,v $
   Revision 1.1  1995/12/22 18:44:45  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "snapdata/geoddata.h"
#include "snapdata/datatype.h"
#include "snapdata/loaddata.h"
#include "util/datafile.h"
#include "util/errdef.h"
#include "util/dateutil.h"
#include "util/pi.h"

#define NAMELEN 20

static char rcsid[]="$Id: geoddata.c,v 1.1 1995/12/22 18:44:45 CHRIS Exp $";

static struct
{
    char *code;
    int type;
    char heights;
    char dms;
    char refcoef;
}


gb_types[] =
{
    /* Code Type H DMS RC */
    { "DS", SD, 1, 0, 0 },
    { "ED", ED, 0, 0, 0 },
    { "HA", HA, 0, 1, 0 },
    { "AZ", AZ, 0, 1, 0 },
    { "VA", ZD, 1, 1, 1 },
    { NULL, 0, 0, 0, 0 }
};


static double gb_date( long ldate, int itime );


int read_gb_data( DATAFILE *d, int (*check_progress)( DATAFILE *d ) )
{
    char type[3], errmess[80];
    int dtype;
    char heights;
    char dms;
    char refcoef;
    int refclassid;
    double errfct;
    char rcname[NAMELEN+1];
    int i, sts;
    int rtnsts;

    char fromcode[NAMELEN+1], tocode[NAMELEN+1];
    int from, to, oldfrom, oldto, itime;
    double value, error, fromhgt, tohgt, dt;
    long ldate;
    char unused;
    char inobs = 0;

    df_read_data_file( d );   /* Skip the header line */

    df_read_data_file( d );   /* Read the file type */

    dtype = -1;
    dms = 0;
    heights = 0;
    refcoef = 0;
    refclassid = -1;
    if( df_read_field( d, type, 3 ) )
    {
        for( i=0; gb_types[i].code; i++ )
        {
            if( strcmp(type,gb_types[i].code) == 0 )
            {
                dtype = gb_types[i].type;
                heights = gb_types[i].heights;
                dms = gb_types[i].dms;
                refcoef = gb_types[i].refcoef;
                break;
            }

        }
    }

    if( dtype < 0 )
    {
        df_data_file_error( d, INVALID_DATA, "Missing or invalid type of data file");
        return INVALID_DATA;
    }

    errfct = dms ? PI/(180.0*3600.0) : 1.0;
    fromhgt = tohgt = 0.0;
    inobs = 0;
    oldfrom = -1; oldto = -1;

    df_skip_to_blank_line( d );   /* Skip over comments section */

    rtnsts = OK;
    while( df_read_data_file( d ) == OK )
    {

        if( check_progress && !(*check_progress)( d ) )
        {
            rtnsts = OPERATION_ABORTED;
            break;
        }

        sts = df_read_code( d, fromcode, NAMELEN+1 ) &&
              df_read_code( d, tocode, NAMELEN+1 );

        /* Reciprocal zenith distances are denoted by 0 station numbers */
        /* In SNAP they are split into the two independent obs */

        if( inobs && strcmp(fromcode,"0") == 0 )
        {
            from = -1;
        }
        else
        {
            from = ldt_get_id( ID_STATION, 0, fromcode );
        }
        if( inobs && strcmp(tocode,"0") == 0 )
        {
            to = -1;
        }
        else
        {
            to = ldt_get_id( ID_STATION, 0, tocode );
        }


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

        value = error = 0;
        itime = 0;
        ldate = 0;
        rcname[0] = 0;

        if( sts )
        {
            sts = dms ? df_read_dmsangle( d, &value ) :
                  df_read_double( d, &value );
        }

        if( sts ) sts = df_read_double( d, &error );
        if( sts && heights )
        {
            sts = df_read_double( d, &fromhgt ) && df_read_double( d, &tohgt );
        }
        if( sts && refcoef )
        {
            sts = df_read_field( d, rcname, NAMELEN+1 );
        }
        if( sts )
        {
            sts = df_read_long( d, &ldate ) && df_read_int( d, &itime );
        }

        if( !sts )
        {
            df_data_file_error(d, INVALID_DATA, "Cannot interpret data");
            continue;
        }

        if( from >= 0 )
        {
            if( inobs ) ldt_end_data();
            inobs = 0;
            if( from == 0 )
            {
                sprintf(errmess,"Station number %s in the data file is missing from the coordinate file",fromcode );
                df_data_file_error(d, INVALID_DATA, errmess );
                continue;
            }

            dt = gb_date( ldate, itime );
            ldt_inststn( from, fromhgt );
            ldt_date( dt );
            inobs = 1;
        }

        if( !inobs ) continue;

        if( to <= 0 )
        {
            sprintf(errmess,"Station number %s in data file is missing from the coordinate file",tocode);
            df_data_file_error(d, INVALID_DATA, errmess );
            continue;
        }

        unused = error < 0 ? 1 : 0;
        if( unused ) error = -error;
        error *= errfct;

        if( error < 1.0e-12 )
        {
            df_data_file_error(d, INVALID_DATA, "Error specified for data is too small");
            continue;
        }

        ldt_tgtstn( to, tohgt );
        ldt_nextdata( dtype );
        ldt_lineno( df_line_number(d));
        ldt_value( &value );
        ldt_error( &error );
        if( unused ) ldt_unused();
        if( refcoef )
        {
            int nameid;
            if( refclassid == -1 )
            {
                refclassid = ldt_get_id( ID_CLASSTYPE, 0, coef_class(COEF_CLASS_REFCOEF)->default_classname );
            }
            nameid = ldt_get_id( ID_CLASSNAME, 0, rcname );
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

