#ifndef SNAPPLOT_UTIL_HPP
#define SNAPPLOT_UTIL_HPP

// Miscellaneous functions to implement interface between
// snapplot windows/C++ code and utility functions from
// snap C library.

#include <cstdarg>
#include "wx_includes.hpp"

void ImplementErrorHandler();
wxString GetLogMessages();

#endif
