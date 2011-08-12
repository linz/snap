#ifndef CRDSYS_RFDEF_BW_H
#define CRDSYS_RFDEF_BW_H

#include "util/iostring.h"
#include "coordsys/coordsys.h"

int rfdef_parse_bw14def( ref_deformation *def, input_string_def *is );
int rfdef_parse_eulerdef( ref_deformation *def, input_string_def *is );

#endif
