#include "snapconfig.h"
#include <stdio.h>
#include "util/xprintf.h"

static xprintf_func pf = 0;

int xprintf( const char *format, ... )
{
    int result;
    va_list args;
    va_start( args, format );
    if( pf )
    {
        result = (*pf)(format, args );
    }
    else
    {
        result = vprintf( format, args );
    }
    va_end(args);
    return result;
}

xprintf_func set_printf_target( xprintf_func newfunc )
{
    xprintf_func opf = pf;
    pf = newfunc;
    return opf;
}
