#ifndef SNAPPLOT_UTIL_HPP
#define SNAPPLOT_UTIL_HPP

// Miscellaneous functions to implement interface between
// snapplot windows/C++ code and utility functions from
// snap C library.

#include "wx_includes.hpp"

#include <stdarg.h>

void ImplementErrorHandler();
wxString GetLogMessages();

#endif
