#include "snapconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <iostream>
#include <string>
#include <string_view>
#include "dbl4_utl_error.h"
#include "dbl4_utl_trace.h"
#include "dbl4_utl_yield.h"
#include "dbl4_utl_progress.h"
#include "dbl4_utl_blob.h"
#include "util/errdef.h"
#include "dbl4/snap_dbl4_interface.h"

static int lastpercent = 0;
static std::string laststate;
static int tracing = -1;

StatusType utlCheckAbort( )
{
    return STS_OK;
}

StatusType utlShowProgress( const std::string_view state, int percent )
{
    /*
        if( strcmp(state,laststate) != 0 || percent != lastpercent ) {
           printf("%s",state);
           if( percent >= 0 && percent <= 100 ) {
              printf(": %d%%\n",percent);
              }
           else {
              printf("\n");
              }
           fflush(stdout);
           }
    */
    if( state != laststate )
    {
        printf("%s\n",std::string(state).c_str());
    }
    laststate = state;
    lastpercent = percent;

    return utlCheckAbort();
}


void utlAbort( const std::string_view message )
{
    std::cerr << message << '\n';
    exit(1);
}




// #pragma warning (disable : 4100)

void start_trace( const char *c, long l )
{
    if( tracing < 0 )
    {
        tracing = getenv("XATRACE") ? 1 : 0;
    }
    if( tracing )
    {
        printf("Trace %s %ld: ",c,l);
    }
    return;
}

void add_trace( const char *fmt, ... )
{
    if( tracing )
    {
        va_list argptr;
        va_start( argptr, fmt );
        vprintf(fmt,argptr);
        va_end(argptr);

    }
}

void end_trace( void )
{
    if( tracing )
    {
        printf("\n");
    }
    return;
}


StatusType utlReadBlobDB( void * blob, long lngOffset, long lngBufSize,
                          void * pvBuffer)
{
    FILE *f;

    f = (FILE *) blob;
    if( fseek( f, lngOffset, SEEK_SET ) != 0 ) RETURN_STATUS(STS_INVALID_DATA);
    if( fread( pvBuffer, lngBufSize, 1, f ) != 1 ) RETURN_STATUS(STS_INVALID_DATA);
    return STS_OK;
}


StatusType utlWriteBlobDB( void *blob, long lngBufSize, void *pvBuffer )
{
    if( ! fwrite( pvBuffer, (size_t) lngBufSize, 1, (FILE *) blob ) )
        RETURN_STATUS(STS_INVALID_DATA);
    return STS_OK;

}

StatusType utlSeekBlobDB( void *blob, long lngOffset, int whence )
{
    int w;
    switch( whence )
    {
    case BLOB_SEEK_CUR: w = SEEK_CUR; break;
    case BLOB_SEEK_END: w = SEEK_END; break;
    default:            w = SEEK_SET; break;
    }
    if( fseek((FILE *) blob, lngOffset, w ) != 0 ) RETURN_STATUS(STS_INVALID_DATA);
    return STS_OK;
}

StatusType utlTellBlobDB( void *blob, long *lngOffset )
{
    (*lngOffset) = (long) ftell( (FILE *) blob );
    return STS_OK;
}

void utlReleaseBlobDB( void * blob)
{
    fclose((FILE *) blob);
}

int utlCreateReadonlyFileBlob( const std::string &filename, hBlob *blob )
{
    FILE *f;
    StatusType sts;
    *blob = NULL;
    f = fopen(filename.c_str(),"rb");
    if( ! f ) return FILE_OPEN_ERROR;
    sts = utlCreateBlobHandle( NULL, blob );
    if( *blob ) (*blob)->pvBlob=f;
    return sts;
}

