#ifndef CRDSYS_HRS_FUNC_H
#define CRDSYS_HRS_FUNC_H


struct height_ref_func_s
{
    char *type;
    char *description;
    height_ref *hrs;
    void *data;
    void (*delete_func)(void *data);
    void (*describe_func)(height_ref_func *hrf, output_string_def *os );
    void *(*copy_func)(void *data);
    int (*identical)(void *data1, void *data2);
    int (*calc_height)( height_ref_func *hrf, double llh[3], double *height, double *exu );
};

height_ref_func *create_offset_height_ref_func( double offset );
height_ref_func *create_grid_height_ref_func( const char *grid_file, int isgeoid );
void delete_height_ref_func( height_ref_func *hrf );
height_ref_func *copy_height_ref_func( height_ref_func *hrf );
int identical_height_ref_func( height_ref_func *hrf1, height_ref_func *hrf2 );
int calc_height_ref_func( height_ref_func *hrf, double llh[3], double *height, double *exu );

#endif
