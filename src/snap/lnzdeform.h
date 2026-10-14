#ifndef _LNZDEFORM_H
#define _LNZDEFORM_H

#include <string>
#include "snap/deform.h"

struct deformation_model;

int create_linzdef_deformation(deformation_model** model, const std::string& pmodel, double pepoch);

#endif