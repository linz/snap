#ifndef _SNAPMAIN_H
#define _SNAPMAIN_H

/*
   $Log: snapmain.h,v $
   Revision 1.6  2003/11/27 00:15:16  ccrook
   Modified to ensure that version number is always compiled into SNAP

   Revision 1.5  1999/05/20 10:43:53  ccrook
   Changed version to 2.14

   Revision 1.4  1998/05/21 04:01:51  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.3  1996/02/23 17:01:30  CHRIS
   SNAP version number changed to 2.12

   Revision 1.2  1996/02/19 19:08:34  CHRIS
   Updated SNAP version number from 2.1 to 2.11

*/

#ifndef SNAPMAIN_H_RCSID
#define SNAPMAIN_H_RCSID "$Id: snapmain.h,v 1.6 2003/11/27 00:15:16 ccrook Exp $"
#endif

#define PROGRAM "SNAP"

/* Version history:

   Version 1.0b: Minor upgrade to version 1.0a.  Converted station ids from
                 numbers to alphanumeric codes.  Provided option of sorting
                 output by station code in output.  Provided option of
                 changing station name width in output.  Provided
                 network_analysis as an alternative to preanalysis.

   Version 1.0c: Change to least squares code to allow it to use matrices
                 of restricted bandwidth.  Incorporation of
                 code to automatically select an optimal ordering for
                 stations

   Version 1.0d: Incorporation of taumax statistics for flagging data,
                 and a better summary of residuals.


   Version 2.0a: July 1993:
                 Significant rewrite of SNAP code.  Main effects are to
                 put the data file input routines and coordinate system
                 code into separate modules.

                 Definition of network topocentre rather than gps_vertical
                 Output of reference frame rotations using both geocentric
                    and topocentric rotations
                 Option of calculating individual components of rotation
                 Support for classification of observations (for weighting
                 and for error summaries)
                 Support for user defined systematic errors.
                 Configuration files supported, definition of user directory,
                    use of snap.cfg file.

   Version 2.0b: June 1994
                 Modifying coordinate system routines, getting it going
                 again (source code has not been valid for some time!).

   Version 2.0c: July 1994
                 Modified to provide several enhancements - addition of flag for
                 ignoring stations means incompatible with 2.0b binary files.

   Version 2.0d: August 1994
                 Modified to provide options in residual listings to meet
                 Geodetic Branch requirements (hopefully!)

   Version 2.0e: April 1995
                 (Also some later cuts of 2.0d).  Further enhanced handling
                 of output columns.  Modified default reference frame for
                 GPS data from WGS84 to GPS.  Reference frames given separate
                 output section, and included in binary file.

   Version 2.1: July 25 1995
                 Changed binary file format to include calculated and residual
                 covariance matrix for vector data (to facilitate better handling
                 of multistation data in xprintf.
*/

/* #define PROGDATE "May 1992" - use __DATE__ instead */

#define DEFAULT_RETURN_STATUS 1

#define CONFIG_FILE     "snap.cfg"

#ifndef _SNAPGLOB_H
#include "snap/snapglob.h"
#endif

int snap_main( int argc, char *argv[] );

#endif
