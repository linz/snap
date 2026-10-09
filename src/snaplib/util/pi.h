
/*
   $Log: pi.h,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:53:37  CHRIS
   Initial revision

*/

/* Simply defines PI (from Abramowitz and Stegun Table 1.1) */

#ifndef _PI_H
#define _PI_H

inline constexpr double PI = 3.1415926535898;
inline constexpr double TWOPI = PI*2.0;
inline constexpr double DTOR = PI/180.0;
inline constexpr double RTOD = 180.0/PI;
inline constexpr double STOR = PI/(180.0*60.0*60.0);
inline constexpr double RTOS = 180.0*60.0*60.0/PI;

#endif
