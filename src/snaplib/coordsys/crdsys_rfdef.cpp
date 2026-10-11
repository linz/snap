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
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_grid.h"
#include "coordsys/crdsys_rfdef_linzdef.h"
#include "coordsys/crdsys_rfdef_bw.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/pi.h"
#include <boost/algorithm/string/predicate.hpp>

int parse_ref_deformation_def ( input_string_def &is, ref_deformation **prdf )
{
    std::string type;
    int sts = OK;

    *prdf = nullptr;

    if( test_next_string_field( is.scanner, "DEFORMATION" ))
    {
        sts = next_string_field(is.scanner,type,20);
        if( sts != OK )
        {
            report_string_error(is,INVALID_DATA,"DEFORMATION type is missing");
            return INVALID_DATA;
        }

        if( boost::algorithm::iequals(type,"LINZDEF") )
        {
            *prdf = rfdef_parse_linzdef( is );
        }
        else if( boost::algorithm::iequals(type,"VELGRID") )
        {
            *prdf = rfdef_parse_griddef( is );
        }
        else if( boost::algorithm::iequals(type,"BW14") )
        {
            *prdf = rfdef_parse_bw14def( is );
        }
        else if( boost::algorithm::iequals(type,"EULER") )
        {
            *prdf = rfdef_parse_eulerdef( is );
        }
        else if( boost::algorithm::iequals(type,"NONE") )
        {
            return OK;
        }
        else
        {
            std::string errmsg = "Invalid DEFORMATION type " + type;
            report_string_error(is, INVALID_DATA, errmsg);
            return INVALID_DATA;
        }

        if( ! *prdf ) sts = INVALID_DATA;
    }

    return sts;
}

ref_deformation::ref_deformation( std::string type_, void *data_,
                                   void (*delete_data_)(void *data),
                                   void *(*copy_data_)(void *data),
                                   int (*identical_)(void *data1, void *data2),
                                   int (*describe_func_)( ref_frame *rf, output_string_def *os ),
                                   int (*calc_denu_)( ref_frame *rf, double lon, double lat, double epoch, double denu[3]),
                                   int (*apply_llh_)( ref_frame *rf, double llh[3], double epochfrom, double epochto ) ) :
    type( std::move(type_) ),
    data( data_ ),
    delete_data( delete_data_ ),
    copy_data( copy_data_ ),
    identical( identical_ ),
    describe_func( describe_func_ ),
    calc_denu( calc_denu_ ),
    apply_llh( apply_llh_ )
{
}

ref_deformation::~ref_deformation()
{
    delete_data( data );
}

ref_deformation * copy_ref_deformation( ref_deformation *rdf )
{
    if( ! rdf ) return nullptr;
    return new ref_deformation( rdf->type, rdf->copy_data( rdf->data ),
                                 rdf->delete_data, rdf->copy_data, rdf->identical,
                                 rdf->describe_func, rdf->calc_denu, rdf->apply_llh );
}


int identical_ref_deformation(  ref_deformation *def1,  ref_deformation *def2 )
{
    if( def1 && ! def2 ) return 0;
    if( def2 && ! def1 ) return 0;
    if( !def1 && ! def2 ) return 1;
    if( def1->type != def2->type ) return 0;
    return def1->identical( def1->data, def2->data );
}
