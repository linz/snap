#ifndef CRDSYS_HRS_FUNC_H
#define CRDSYS_HRS_FUNC_H


struct vdatum_func_s
{
    char *type;
    char *description;
    vdatum *hrs;
    void *data;
    void (*delete_func)(void *data);
    void (*describe_func)(vdatum_func *hrf, output_string_def *os );
    void *(*copy_func)(void *data);
    int (*identical)(void *data1, void *data2);
    int (*calc_height)( vdatum_func *hrf, double llh[3], double *height, double *exu );
};

vdatum_func *create_offset_vdatum_func( double offset );
vdatum_func *create_grid_vdatum_func( const char *grid_file, int isgeoid );
void delete_vdatum_func( vdatum_func *hrf );
vdatum_func *copy_vdatum_func( vdatum_func *hrf );
int identical_vdatum_func( vdatum_func *hrf1, vdatum_func *hrf2 );
int calc_vdatum_func( vdatum_func *hrf, double llh[3], double *height, double *exu );

#endif
