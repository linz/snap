#ifndef _GET_DATE_H
#define _GET_DATE_H

/*
   $Log: get_date.h,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:49:05  CHRIS
   Initial revision

*/

#include <cstdio>
#include <string>

/// Length of the run date field in binary files, including the terminating NUL
inline constexpr int GETDATELEN = 21;

/// Returns the current date and time as text, of at most GETDATELEN-1 characters.
/// If the environment variable SNAP_TEST_FIXED_DATE is set it returns that instead.
std::string get_date();

/// Writes a run date as a GETDATELEN byte field, padded with NUL bytes.
/// The text is truncated if it is too long.
void write_run_date_field( FILE *f, const std::string &runDate );

/// Reads a run date from a GETDATELEN byte field, up to the first NUL byte.
/// Returns false, leaving runDate unchanged, if the field cannot be read.
bool read_run_date_field( FILE *f, std::string &runDate );

#endif
