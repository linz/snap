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
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "coordsys/crdsys_rffunc_grid.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/pi.h"

int parse_ref_frame_func_def ( input_string_def *is, ref_frame_func **rff )
{
    int sts;

    *rff = 0;
    if( ! test_next_string_field( is, "GRID" ) )
    {
        sts = OK;
    }
    else
    {
        char gridtype[20+1];
        const char *gfile = 0;
        char gridfile[MAX_FILENAME_LEN+1];
        char description[255+1];
        sts = next_string_field( is, gridtype, 20 );
        if( sts == OK )
            sts = next_string_field( is, gridfile, MAX_FILENAME_LEN );
        if( sts == OK )
        {
            gfile = find_relative_file( is->sourcename, gridfile, ".grd" );
            if( ! gfile )
            {
                sts = INVALID_DATA;
                report_string_error(is, sts,"Reference frame grid file does not exist");
            }
        }
        description[0] = 0;
        next_string_field( is, description, 255 );
        if( sts == OK )
        {
            *rff =
                create_rf_grid_func( gridtype, gfile, description );
            if( ! *rff )
            {
                report_string_error( is, INVALID_DATA, "Reference frame GRID"
                                     " could not be loaded" );
                sts=INVALID_DATA;
            }
            else
            {
                (*rff)->type = copy_string( "GRID" );
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

void delete_ref_frame_func( ref_frame_func *rff )
{
    if( ! rff ) return;
    if( rff->type ) check_free( rff->type );
    if( rff->description ) check_free( rff->description );
    (*(rff->delete_func))(rff->data);
    check_free( rff );
}

ref_frame_func * copy_ref_frame_func( ref_frame_func *rff )
{
    ref_frame_func *rff1;
    if( ! rff ) return NULL;
    rff1 = (ref_frame_func *)check_malloc( sizeof(ref_frame_func) );
    rff1->type = copy_string( rff->type );
    rff1->description = copy_string( rff->description );
    rff1->data = (*(rff->copy_func))( rff->data );
    rff1->delete_func = rff->delete_func;
    rff1->describe_func = rff->describe_func;
    rff1->copy_func = rff->copy_func;
    rff1->identical = rff->identical;
    rff1->xyz_to_std_func = rff->xyz_to_std_func;
    rff1->std_to_xyz_func = rff->std_to_xyz_func;
    return rff1;
}

int identical_ref_frame_func(  ref_frame_func *rff1,  ref_frame_func *rff2 )
{
    if( rff1 && ! rff2 ) return 0;
    if( rff2 && ! rff1 ) return 0;
    if( !rff1 && ! rff2 ) return 1;
    if( strcmp( rff1->type, rff2->type ) != 0 ) return 0;
    if( ! rff1->identical ) return 0;
    return (*(rff1->identical))(rff1->data,rff2->data);
}
