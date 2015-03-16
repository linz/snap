#ifndef _XPRINTF_H
#define _XPRINTF_H

#include <stdarg.h>

typedef int (*xprintf_func)( const char *format, va_list args );

int xprintf( const char *format, ... );
xprintf_func set_printf_target( xprintf_func newfunc );


#endif
