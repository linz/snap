#include "snapconfig.h"
/*
   $Log: paramdef.c,v $
   Revision 1.2  2004/04/22 02:34:22  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1996/01/04 00:06:04  CHRIS
   Initial revision

   Revision 1.1  1995/12/22 16:58:24  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include "util/errdef.h"
#include "coordsys/paramdef.h"
#include "util/dms.h"
#include "util/pi.h"

#define DO_PRINT(fmt,type) \
	char buf[40];           \
   sprintf(buf,fmt,*(type*)address); \
   return (*os->write)(buf,os->sink)

int print_int( output_string_def *os, void *address )
{
    DO_PRINT("%d",int);
}

int print_short( output_string_def *os, void *address )
{
    DO_PRINT("%hd",short);
}

int print_long( output_string_def *os, void *address )
{
    DO_PRINT("%ld",long);
}

int print_double0( output_string_def *os, void *address )
{
    DO_PRINT("%.0lf",double);
}

int print_double3( output_string_def *os, void *address )
{
    DO_PRINT("%.3lf",double);
}

int print_double6( output_string_def *os, void *address )
{
    DO_PRINT("%.6lf",double);
}

int print_radians( output_string_def *os, void *address )
{
    char buf[40];
    double deg = *(double *)(address) * RTOD;
    sprintf( buf, "%.6lf", deg );
    return (*os->write)(buf,os->sink);
}

int print_latitude( output_string_def *os, void *address )
{
    static const DmsFormat latitudeFormat( 3, 4, 0, std::nullopt, std::nullopt, std::nullopt, "N", "S" );
    const std::string text = dms_string( * static_cast<double *>( address ) * RTOD, latitudeFormat );
    return (*os->write)(text,os->sink);
}

int print_longitude( output_string_def *os, void *address )
{
    static const DmsFormat longitudeFormat( 3, 4, 0, std::nullopt, std::nullopt, std::nullopt, "E", "W" );
    const std::string text = dms_string( * static_cast<double *>( address ) * RTOD, longitudeFormat );
    return (*os->write)(text,os->sink);
}


int read_radians( FieldScanner &scanner, void *address )
{
    double rad;
    int sts;
    sts = double_from_string( scanner, &rad );
    if( sts == OK ) rad *= DTOR;
    *(double *)address = rad;
    return sts;
}

int read_param_list( input_string_def &is, param_def *prms, int nprm, void *base )
{
    int sts;
    int iprm;
    sts = OK;
    for( iprm = 0; iprm < nprm; iprm++, prms++ )
    {
        sts = (*prms->read)( is.scanner, OFFSET_ADDRESS(base,prms->offset) );
        if( sts == MISSING_DATA )
        {
            report_string_error( is, sts, std::string( prms->name ) + " is missing" );
            break;
        }
        else if ( sts != OK )
        {
            report_string_error( is, sts, "Invalid definition of " + std::string( prms->name ) );
            break;
        }
    }
    return sts;
}

void print_param_list( output_string_def *os, param_def *prms, int nprm,
                       void *base, const std::string_view prefix )
{
    int iprm;
    size_t maxlen = 0;
    for( iprm = 0; iprm < nprm; iprm++ )
    {
        if( !prms[iprm].print ) continue;
        maxlen = std::max( maxlen, prms[iprm].name.size() );
    }
    maxlen = std::min( maxlen, static_cast<size_t>( 80 ) );
    for( iprm = 0; iprm < nprm; iprm++ )
    {
        param_def *pd = prms+iprm;
        if( !pd->print ) continue;
        if( !prefix.empty() ) write_output_string( os, prefix );
        const std::string_view name = pd->name.substr( 0, maxlen );
        write_output_string( os, name );
        write_output_string( os, std::string( maxlen - name.size(), ' ' ) );
        write_output_string( os, "  " );
        (*pd->print)( os, OFFSET_ADDRESS( base, pd->offset ));
        write_output_string( os, "\n" );
    }
}




