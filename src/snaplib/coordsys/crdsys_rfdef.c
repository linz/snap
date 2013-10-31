#include "snapconfig.h"

/* crdsysr4.c:  Reference frame function routines
*/

/*
   $Log: crdsysr4.c,v $
   Revision 1.3  2004/04/22 02:34:21  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  2004/02/02 03:25:43  ccrook
   Fixed up copying of reference frame functions in copy_coordsys function

   Revision 1.1  2003/11/28 01:59:26  ccrook
   Updated to be able to use grid transformation for datum changes (ie to
   support official NZGD49-NZGD2000 conversion)


*/

#include <stdio.h>
#include <string.h>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_grid.h"
#include "coordsys/crdsys_rfdef_linzdef.h"
#include "coordsys/crdsys_rfdef_bw.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "coordsys/crdsys_rffunc_grid.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/pi.h"

static char rcsid[]="$Id: crdsysr4.c,v 1.3 2004/04/22 02:34:21 ccrook Exp $";

static int default_describe_func(ref_frame *rf, output_string_def *os )
{
    char buf[32];
    ref_deformation *rdf = rf->def;
    if( ! rdf ) return OK;
    write_output_string( os, "Deformation model type ");
    write_output_string( os, rdf->type);
    write_output_string( os, buf );
    write_output_string( os, "\n");
    return OK;
}

static int default_calc_func( ref_frame *rf, double lon, double lat, double epoch, double denu[3])
{
    denu[0] = denu[1] = denu[2] = 0.0;
    return INVALID_DATA;
}


int parse_ref_deformation_def ( input_string_def *is, ref_deformation **prdf )
{
    char type[20+1];
    int sts;
    long loc;
    ref_deformation *rdf;

    *prdf = 0;
    sts = OK;

    if( test_next_string_field( is, "DEFORMATION" ))
    {
        sts = next_string_field(is,type,20);
        if( sts != OK )
        {
            report_string_error(is,INVALID_DATA,"DEFORMATION type is missing");
            return INVALID_DATA;
        }

        _strupr(type);
        rdf = (ref_deformation *) check_malloc(sizeof(ref_deformation));
        rdf->type = copy_string(type);
        rdf->copy_func = 0;
        rdf->identical = 0;
        rdf->delete_func = 0;
        rdf->describe_func = default_describe_func;
        rdf->calc_denu = default_calc_func;
        rdf->apply_llh = 0;
        rdf->data = 0;

        if( strcmp(type,"LINZDEF") == 0 )
        {
            sts = rfdef_parse_linzdef( rdf, is );
        }
        else if( strcmp(type,"VELGRID") == 0 )
        {
            sts = rfdef_parse_griddef( rdf, is );
        }
        else if( strcmp(type,"BW14") == 0 )
        {
            sts = rfdef_parse_bw14def( rdf, is );
        }
        else if( strcmp(type,"EULER") == 0 )
        {
            sts = rfdef_parse_eulerdef( rdf, is );
        }
        else if( strcmp(type,"NONE") == 0 )
        {
            delete_ref_deformation(rdf);
            rdf = 0;
        }
        else
        {
            char errmsg[80];
            sprintf(errmsg,"Invalid DEFORMATION type %s",type);
            report_string_error(is, INVALID_DATA, errmsg);
        }

        if( sts != OK )
        {
            delete_ref_deformation(rdf);
            rdf = 0;
        }
        *prdf = rdf;
    }

    return sts;
}

void delete_ref_deformation( ref_deformation *rdf )
{
    if( ! rdf ) return;
    if( rdf->type ) check_free( rdf->type );
    if( rdf->delete_func) (*(rdf->delete_func))(rdf->data);
    check_free( rdf );
}

ref_deformation * copy_ref_deformation( ref_deformation *rdf )
{
    ref_deformation *rdf1;
    if( ! rdf ) return NULL;
    rdf1 = (ref_deformation *)check_malloc( sizeof(ref_deformation) );
    rdf1->type = copy_string( rdf->type );
    rdf1->data = 0;
    if( rdf->copy_func ) rdf1->data = (*(rdf->copy_func))( rdf->data );
    rdf1->delete_func = rdf->delete_func;
    rdf1->describe_func = rdf->describe_func;
    rdf1->copy_func = rdf->copy_func;
    rdf1->identical = rdf->identical;
    rdf1->calc_denu = rdf->calc_denu;
    rdf1->apply_llh = rdf->apply_llh;
    return rdf1;
}


int identical_ref_deformation(  ref_deformation *def1,  ref_deformation *def2 )
{
    if( def1 && ! def2 ) return 0;
    if( def2 && ! def1 ) return 0;
    if( !def1 && ! def2 ) return 1;
    if( strcmp( def1->type, def2->type ) != 0 ) return 0;
    if( ! def1->identical ) return 0;
    return (*(def1->identical))(def1->data,def2->data);
}
