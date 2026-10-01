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

std::optional<std::string> get_default_crdsys_file()
{
    install_default_projections();

    /* Try for an environment variable definition */

    const char *envfile = getenv(CRDSYSENV);
    if( envfile ) return std::string(envfile);

    /* Now try the user and system configuration directories  */
    return find_file(CRDSYSFILE,"",std::nullopt,FF_TRYALL,COORDSYS_CONFIG_SECTION);
}

int install_default_crdsys_file()
{
    auto filename=get_default_crdsys_file();
    if( ! filename )  return FILE_OPEN_ERROR;
    install_default_projections();
    return install_crdsys_file( *filename );
}

std::optional<std::string> find_coordsys_data_file( const std::string &filename, const std::string &extension )
{
    auto found = find_file(filename,extension,std::nullopt,FF_TRYALL,"");
    if( found ) return found;
    found = get_crdsys_file(filename,extension);
    if( found ) return found;
    return find_file(filename,extension,std::nullopt,FF_TRYNONE,COORDSYS_CONFIG_SECTION);
}


