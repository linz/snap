#ifndef _GRDDEFORM_H
#define _GRDDEFORM_H

#include <string>

struct deformation_model;

int create_grid_deformation(deformation_model** model, const std::string& pmodel, double pepoch);

#endif
