#include "snapconfig.h"
/* Routines to print a description of the coordinate system to a file */

/*
   $Log: crdsysc4.c,v $
   Revision 1.2  2003/11/28 01:59:25  ccrook
   Updated to be able to use grid transformation for datum changes (ie to
   support official NZGD49-NZGD2000 conversion)

   Revision 1.1  1995/12/22 16:27:18  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>

#include "util/errdef.h"
#include "coordsys/crdsys_prj.h"
#include "util/pi.h"


static char rcsid[]="$Id: crdsysc4.c,v 1.2 2003/11/28 01:59:25 ccrook Exp $";

int  describe_ellipsoid( output_string_def *os, ellipsoid *el )
{
    char out[80];
    if( !el ) return OK;
    write_output_string( os, "Ellipsoid: ");
    write_output_string( os, el->name );
    sprintf(out,"\n   a = %.3lf  1/f = %.6lf\n",el->a,el->rf);
    write_output_string( os, out );
    return OK;
}

int  describe_ref_frame( output_string_def *os, ref_frame *rf )
{
    char translation;
    char scale;
    char rotation;

    write_output_string( os, "Reference frame: " );
    write_output_string( os, rf->name );
    write_output_string( os, "\n" );

    scale = rf->scale != 0.0;

    translation = rf->txyz[0] != 0.0 ||
                  rf->txyz[1] != 0.0 ||
                  rf->txyz[2] != 0.0;

    rotation    = rf->rxyz[0] != 0.0 ||
                  rf->rxyz[1] != 0.0 ||
                  rf->rxyz[2] != 0.0;

    if( rf->refcode && (scale || translation || rotation ))
    {
        char out[256];
        write_output_string( os, "   Relative to ");
        write_output_string( os, rf->refcode);
        write_output_string( os, "\n" );
        if( scale )
        {
            sprintf(out,"      scale (ppm)     %7.2lf\n",rf->scale);
            write_output_string( os, out );
        }
        if( translation )
        {
            sprintf(out,"      translation (m) %7.2lf %7.2lf %7.2lf\n",
                    rf->txyz[0], rf->txyz[1], rf->txyz[2] );
            write_output_string( os, out );
        }
        if( rotation )
        {
            sprintf(out,"      rotation (sec)  %7.2lf %7.2lf %7.2lf\n",
                    rf->rxyz[0], rf->rxyz[1], rf->rxyz[2] );
            write_output_string( os, out );
        }
    }
    if( rf->func && rf->func->describe_func ) (*(rf->func->describe_func))(rf, os);
    if( rf->def && rf->def->describe_func ) (*(rf->def->describe_func))(rf, os);
    describe_ellipsoid( os, rf->el );
    return OK;
}

int describe_projection( output_string_def *os, projection *prj )
{
    if( !prj ) return OK;
    if( prj->type->name )
    {
        write_output_string( os, "Projection: " );
        write_output_string( os, prj->type->name);
        write_output_string( os, "\n" );
    }

    if( prj->type->nparams )
    {
        print_param_list( os, prj->type->params, prj->type->nparams,
                          prj->data, "   ");
    }
    return OK;
}

int  describe_coordsys(  output_string_def *os, coordsys *cs )
{
    write_output_string( os, "Coordinate system: " );
    write_output_string( os, cs->name );
    write_output_string( os, "\n" );
    describe_ref_frame( os, cs->rf );
    if( has_deformation_model(cs) && cs->epoch != 0.0)
    {
        char buf[32];
        write_output_string(os,"Calculated at epoch ");
        sprintf(buf,"%.1lf\n",cs->epoch);
        write_output_string(os,buf);
    }
    describe_projection( os, cs->prj );
    return OK;
}

int  describe_deformation_model( output_string_def *os, ref_frame *rf )
{
    if( rf->def )
    {
        return rf->def->describe_func( rf, os );
    }
    return OK;
}

