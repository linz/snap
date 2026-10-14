#ifndef CRDSYS_RFDEF_BW_H
#define CRDSYS_RFDEF_BW_H

struct input_string_def;
struct ref_deformation;

ref_deformation* rfdef_parse_bw14def(input_string_def& is);
ref_deformation* rfdef_parse_eulerdef(input_string_def& is);

#endif
