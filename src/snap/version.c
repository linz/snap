#include "snapconfig.h"
/*
   version.c: File to ensure that each compilation receives up to date version
   number and date.  Always compiled by make.


   $Log: version.c,v $
   Revision 1.3  2004/03/10 21:18:48  ccrook
   Update version number after identifying error in specification testing.

   Revision 1.2  2003/11/28 01:56:47  ccrook
   Updated to defer formulation of full normal equation matrix to calculation
   of inverse when it is required, rather than bypassing station reordering.

   Revision 1.1  2003/11/27 00:15:16  ccrook
   Modified to ensure that version number is always compiled into SNAP


*/

#include "version.h"
#include "util/versioninfo.h"

static char rcsid[] = "$Id: version.c,v 1.3 2004/03/10 21:18:48 ccrook Exp $";

char *version_number( void ) { return ProgramVersion.version; }
char *version_date( void ) { return ProgramVersion.builddate; }

