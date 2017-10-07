#include "snapconfig.h"
#include <stdio.h>
#include "snapmain.h"
#include "snap/snapglob.h"
#include "output.h"

int main( int argc, char *argv[] )
{
    get_date( run_time );
    print_report_header( stdout );
    return snap_main( argc, argv );
}
