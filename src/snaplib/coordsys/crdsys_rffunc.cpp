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
#include "coordsys/crdsys_rffunc_grid.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/pi.h"

int parse_ref_frame_func_def ( input_string_def &is, ref_frame_func **rff )
{
    int sts;

    *rff = 0;
    if( ! test_next_string_field( is.scanner, "GRID" ) )
    {
        sts = OK;
    }
    else
    {
        std::string gridtype;
        std::optional<std::string> gfile;
        std::string gridfile;
        std::string description;
        sts = next_string_field( is.scanner, gridtype, 20 );
        if( sts == OK )
            sts = next_string_field( is.scanner, gridfile, MAX_FILENAME_LEN );
        if( sts == OK )
        {
            gfile = find_relative_file( is.sourcename, gridfile, ".grd" );
            if( ! gfile )
            {
                sts = INVALID_DATA;
                report_string_error(is, sts,"Reference frame grid file does not exist");
            }
        }
        next_string_field( is.scanner, description, 255 );
        if( sts == OK )
        {
            *rff =
                create_rf_grid_func( gridtype, *gfile, description );
            if( ! *rff )
            {
                report_string_error( is, INVALID_DATA, "Reference frame GRID"
                                     " could not be loaded" );
                sts=INVALID_DATA;
            }
        }
        else
        {
            report_string_error( is, INVALID_DATA, "Reference frame GRID function "
                                 "requires type and filename parameters");
        }
    }
    return sts;
}

ref_frame_func::ref_frame_func( std::string type_, std::optional<std::string> description_, void *data_,
                                 void (*delete_data_)(void *data),
                                 int (*describe_func_)(ref_frame *rf, output_string_def *os),
                                 void *(*copy_data_)(void *data),
                                 int (*identical_)(void *data1, void *data2),
                                 int (*xyz_to_std_func_)( ref_frame *rf, double xyz[3], double date ),
                                 int (*std_to_xyz_func_)( ref_frame *rf, double xyz[3], double date ) ) :
    type( std::move(type_) ),
    description( std::move(description_) ),
    data( data_ ),
    delete_data( delete_data_ ),
    describe_func( describe_func_ ),
    copy_data( copy_data_ ),
    identical( identical_ ),
    xyz_to_std_func( xyz_to_std_func_ ),
    std_to_xyz_func( std_to_xyz_func_ )
{
}

ref_frame_func::~ref_frame_func()
{
    delete_data( data );
}

ref_frame_func * copy_ref_frame_func( ref_frame_func *rff )
{
    if( ! rff ) return nullptr;
    return new ref_frame_func( rff->type, rff->description, rff->copy_data( rff->data ),
                                rff->delete_data, rff->describe_func, rff->copy_data,
                                rff->identical, rff->xyz_to_std_func, rff->std_to_xyz_func );
}

int identical_ref_frame_func(  ref_frame_func *rff1,  ref_frame_func *rff2 )
{
    if( rff1 && ! rff2 ) return 0;
    if( rff2 && ! rff1 ) return 0;
    if( !rff1 && ! rff2 ) return 1;
    if( rff1->type != rff2->type ) return 0;
    return rff1->identical( rff1->data, rff2->data );
}
