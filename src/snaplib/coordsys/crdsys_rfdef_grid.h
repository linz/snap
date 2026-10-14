#ifndef CRDSYS_RFDEF_GRID_H
#define CRDSYS_RFDEF_GRID_H

#include "util/iostring.h"
#include "coordsys/coordsys.h"

struct input_string_def;
struct ref_deformation;

ref_deformation* rfdef_parse_griddef(input_string_def& is);

#endif
