#ifndef _DEFORM_H
#define _DEFORM_H

/*
   $Log: deform.h,v $
   Revision 1.1  1998/06/04 02:24:53  ccrook
   Initial revision


*/

#ifndef DEFORM_H_RCSID
#define DEFORM_H_RCSID "$Id: deform.h,v 1.1 1998/06/04 02:24:53 ccrook Exp $"
#endif

#ifndef _NETWORK_H
#include "network/network.h"
#endif

/* Structure for general deformation model */

typedef struct
{
    void *data;
    int (*init_deformation)( void *model );
    int (*calc_deformation)( void *model, station *st, double date, double denu[3] );
    int (*print_model)( void *model, FILE *out, const char *prefix );
    int (*delete_model)( void *model );
} deformation_model;


deformation_model *create_deformation_model(
    void *data,
    int (*init_deformation)( void *model ),
    int (*calc_deformation)( void *model, station *st, double date, double denu[3] ),
    int (*print_model)( void *model, FILE *out, const char *prefix ),
    int (*delete_model)( void *model )
);


/* Called before the first adjustment iteration to perform any necessary
   preparation.  All obs and stations are available at this point.
   Should perform any kind preprocessing/loading, and should check that
   there will be no problems in terms of extents etc of the model.  */

int init_deformation( deformation_model *deformation );

/* Called for each observation to determine the east, north, and vertical
   offset that the model predicts for a specific time */

int calc_deformation( deformation_model *deformation, station *st, double date, double denu[3] );

/* Describe the deformation model in an output file */

int print_deformation_model( deformation_model *deformation, FILE *out, const char *prefix );

/* Delete the deformation model */

int delete_deformation( deformation_model *deformation );

#endif
