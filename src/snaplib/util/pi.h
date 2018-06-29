
/*
   $Log: pi.h,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:53:37  CHRIS
   Initial revision

*/

/* Simply defines PI (from Abramowitz and Stegun Table 1.1) */

#ifndef PI
#define PI 3.1415926535898
#define TWOPI (PI*2.0)
#define DTOR (PI/180.0)
#define RTOD (180.0/PI)
#define STOR (PI/(180.0*60.0*60.0))
#define RTOS (180.0*60.0*60.0/PI)
#endif
