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
#include "util/errdef.h"
#include "coordsys/paramdef.h"
#include "util/dms.h"
#include "util/pi.h"

#define DO_PRINT(fmt,type) \
	char buf[40];           \
   sprintf(buf,fmt,*(type*)address); \
   return (*os->write)(buf,os->sink)

void *latfmt = NULL;
void *lonfmt = NULL;

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

static void define_dms_formats( void )
{
    latfmt = create_dms_format(3,4,0,NULL,NULL,NULL,"N","S");
    lonfmt = create_dms_format(3,4,0,NULL,NULL,NULL,"E","W");
}

int print_latitude( output_string_def *os, void *address )
{
    const char *buf;
    if( !latfmt ) define_dms_formats();
    buf = dms_string( * (double *) address * RTOD, latfmt, NULL );
    return (*os->write)(buf,os->sink);
}

int print_longitude( output_string_def *os, void *address )
{
    const char *buf;
    if( !latfmt ) define_dms_formats();
    buf = dms_string( * (double *) address * RTOD, lonfmt, NULL );
    return (*os->write)(buf,os->sink);
}


int read_radians( input_string_def *is, void *address )
{
    double rad;
    int sts;
    sts = double_from_string( is, &rad );
    if( sts == OK ) rad *= DTOR;
    *(double *)address = rad;
    return sts;
}

int read_param_list( input_string_def *is, param_def *prms, int nprm, void *base )
{
    int sts;
    int iprm;
    char errmess[128];
    sts = OK;
    for( iprm = 0; iprm < nprm; iprm++, prms++ )
    {
        sts = (*prms->read)( is, OFFSET_ADDRESS(base,prms->offset) );
        if( sts == MISSING_DATA )
        {
            sprintf(errmess,"%s is missing",prms->name);
            report_string_error( is, sts, errmess );
            break;
        }
        else if ( sts != OK )
        {
            sprintf(errmess,"Invalid definition of %s",prms->name);
            report_string_error( is, sts, errmess );
            break;
        }
    }
    return sts;
}

void print_param_list( output_string_def *os, param_def *prms, int nprm,
                       void *base, const char *prefix )
{
    int iprm;
    int maxlen = 0;
    for( iprm = 0; iprm < nprm; iprm++ )
    {
        int prmlen;
        if( !prms[iprm].print ) continue;
        prmlen = strlen( prms[iprm].name );
        if( prmlen > maxlen ) maxlen = prmlen;
    }
    if( maxlen > 80 ) maxlen = 80;
    for( iprm = 0; iprm < nprm; iprm++ )
    {
        char buf[81];
        param_def *pd = prms+iprm;
        if( !pd->print ) continue;
        if( prefix ) write_output_string( os, prefix );
        sprintf( buf, "%-*.*s",maxlen,maxlen,pd->name );
        write_output_string( os, buf );
        write_output_string( os, "  " );
        (*pd->print)( os, OFFSET_ADDRESS( base, pd->offset ));
        write_output_string( os, "\n" );
    }
}




