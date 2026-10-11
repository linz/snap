#ifndef _GRDDEFORM_H
#define _GRDDEFORM_H

#include <string>

#include "snap/deform.h"

int create_grid_deformation( deformation_model **model, const std::string &pmodel, double pepoch );

#endif
