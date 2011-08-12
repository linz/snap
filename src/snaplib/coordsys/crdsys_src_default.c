#include "snapconfig.h"
/* crdsysf1.c: Code to install a default coordinate system file.  Includes
   registering projections and obtaining the file name for the file.

The coordinate system file is located in one of the following places.
   1) a file pointed to by the environment variable COORDSYSFILE.
   3) a file located in a directory named by the parameter, with name
      "coordsys.def"
   4) a file called "coordsys.def" in the current file.
*/

/*
   $Log: crdsysf1.c,v $
   Revision 1.2  1996/05/20 16:36:08  CHRIS
   Removed redundant calls to register projections.

   Revision 1.1  1995/12/22 16:34:59  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coordsys/coordsys.h"
#include "coordsys/nzmgr.h"
#include "coordsys/tmprojr.h"
#include "coordsys/lambertr.h"
#include "coordsys/psprojr.h"
#include "util/errdef.h"
#include "util/chkalloc.h"
#include "util/fileutil.h"

static char rcsid[]="$Id: crdsysf1.c,v 1.2 1996/05/20 16:36:08 CHRIS Exp $";

static char *CRDSYSENV = "COORDSYSDEF";
static char *CRDSYSFILE = "coordsys.def";

int install_default_crdsys_file( const char *path )
{
    char *filename;
    install_default_projections();

    /* Try for an environment variable definition */

    filename = getenv(CRDSYSENV);
    if( filename && file_exists(filename) && install_crdsys_file(filename) == OK ) return OK;

    /* Now try the path with CRDSYSFILE appended */

    if( path )
    {
        int len;
        int sts;
        len = path_len(path,0);
        filename = (char *) check_malloc( len + strlen(CRDSYSFILE) + 1 );
        memcpy(filename,path,len);
        strcpy(filename+len,CRDSYSFILE);
        sts = FILE_OPEN_ERROR;
        if( file_exists( filename ) ) sts = install_crdsys_file(filename);
        check_free( filename );
        if( sts == OK ) return sts;
    }

    if( !file_exists( CRDSYSFILE ) ) return FILE_OPEN_ERROR;
    return install_crdsys_file( CRDSYSFILE );
}


