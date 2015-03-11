#include "snapconfig.h"
/*
   deform.c: Module to manage deformation in SNAP.

   May 1998: Implementation supports only simple velocity model.  Interface
   would need reworking to provide more sophisticated models - time constraints
   prevent generalizing in the first hit.

   $Log: deform.c,v $
   Revision 1.3  2004/04/22 02:35:43  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  1998/05/22 01:39:46  ccrook
   Fixes to handle Watcom math.h idiosyncrasies

   Revision 1.1  1998/05/21 01:49:58  ccrook
   Initial revision


*/

#include <stdio.h>
#include <math.h>

#include "snap/deform.h"
#include "network/network.h"
#include "util/chkalloc.h"
#include "util/errdef.h"

static char rcsid[]="$Id: deform.c,v 1.3 2004/04/22 02:35:43 ccrook Exp $";


deformation_model *create_deformation_model(
    void *data,
    int (*init_deformation)( void *model ),
    int (*calc_deformation)( void *model, station *st, double date, double denu[3] ),
    int (*print_model)( void *model, FILE *out, char *prefix ),
    int (*delete_model)( void *model )
)
{
    deformation_model *model = (deformation_model *) check_malloc(sizeof(deformation_model));
    model->data = data;
    model->init_deformation = init_deformation;
    model->calc_deformation = calc_deformation;
    model->print_model = print_model;
    model->delete_model = delete_model;
    return model;
}


int init_deformation( deformation_model *deformation )
{
    return (*(deformation->init_deformation))( deformation->data );
}

int calc_deformation( deformation_model *deformation, station *st, double date, double denu[3] )
{
    int sts = OK;
    int hasoffset=station_has_offset(st);

    if( hasoffset && station_offset_is_deformation(st) ) 
    {
        denu[0]=denu[1]=denu[2]=0.0;
    }
    else
    {
        sts=(*(deformation->calc_deformation))( deformation->data, st, date, denu );
    }
    if( hasoffset )
    {
        double dsenu[3];
        calc_station_offset( st, date, dsenu );
        denu[0] += dsenu[0];
        denu[1] += dsenu[1];
        denu[2] += dsenu[2];
    }
    return sts;
}

int print_deformation_model( deformation_model *deformation, FILE *out, char *prefix )
{
    return (*(deformation->print_model))( deformation->data, out, prefix );
}

int delete_deformation( deformation_model *deformation )
{
    return (*(deformation->delete_model))( deformation->data );
}

