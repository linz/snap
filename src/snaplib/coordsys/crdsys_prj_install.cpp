#include "snapconfig.h"
/* crdsysp4.c: Installs the favourite projections */

/*
   $Log: crdsysp4.c,v $
   Revision 1.1  1995/12/22 17:13:26  CHRIS
   Initial revision

*/

#include <stdio.h>

#include "coordsys/coordsys.h"
#include "coordsys/nzmgr.h"
#include "coordsys/tmprojr.h"
#include "coordsys/lambertr.h"
#include "coordsys/psprojr.h"

void install_default_projections( void )
{
    register_nzmg_projection();
    register_tm_projection();
    register_lcc_projection();
    register_ps_projection();
}


