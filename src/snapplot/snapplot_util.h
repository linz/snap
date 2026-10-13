#ifndef SNAPPLOT_UTIL_H
#define SNAPPLOT_UTIL_H

#include <cstdarg>

int print_log(const char* format, ...);
int print_log_args(const char* format, std::va_list args);

#endif
