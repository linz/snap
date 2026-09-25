#ifndef CRDSYS_HRS_FUNC_H
#define CRDSYS_HRS_FUNC_H

#include <string>

/// Always fully-formed once constructed (no default constructor, all
/// fields const except hrs) - copy via copy_vdatum_func() (a real deep
/// copy: this struct's own data member is deep-copied via copy_data),
/// never via the copy constructor, which is deleted. Destroying one calls
/// delete_data(data) - note this is a different thing from destroying the
/// vdatum_func itself: delete_data only knows how to free the opaque data
/// payload, it's an internal detail the destructor uses, never a public
/// destruction API of its own. 2 concrete implementations exist today (a
/// plain height offset, and a grid/geoid transform shared by both the
/// "GRID" and "GEOID" types), each reached only through its own factory
/// in crdsys_hrs_func.cpp. hrs is a non-owning back-pointer to the owning
/// vdatum, not knowable at vdatum_func's own construction time - it's
/// wired up by vdatum's own constructor right afterward, so it alone
/// stays non-const.
struct vdatum_func
{
    vdatum_func( std::string type, std::string description, void *data,
                 void (*delete_data)(void *data),
                 void (*describe_func)(vdatum_func *hrf, output_string_def *os),
                 void *(*copy_data)(void *data),
                 int (*identical)(void *data1, void *data2),
                 int (*calc_height)( vdatum_func *hrf, double llh[3], double *height, double *exu ) );
    vdatum_func( const vdatum_func& ) = delete;
    ~vdatum_func();

    const std::string type;        ///< Discriminator tag for the concrete implementation, e.g. "OFFSET"
    const std::string description; ///< Human-readable description used in reporting
    vdatum *hrs;                     ///< Non-owning back-pointer to the owning vdatum, set by vdatum's own constructor
    void * const data;              ///< Opaque payload for the concrete implementation, freed by delete_data
    void (* const delete_data)(void *data); ///< Frees the object pointed to by this struct's own data member
    void (* const describe_func)(vdatum_func *hrf, output_string_def *os ); ///< Writes a human-readable description - never actually invoked anywhere today
    void *(* const copy_data)(void *data); ///< Deep-copies this struct's own data member
    int (* const identical)(void *data1, void *data2); ///< Compares two data payloads for equality
    int (* const calc_height)( vdatum_func *hrf, double llh[3], double *height, double *exu ); ///< Computes the height/deflection offset this datum applies
};

vdatum_func *create_offset_vdatum_func( double offset );
vdatum_func *create_grid_vdatum_func( const std::string &grid_file, int isgeoid );
vdatum_func *copy_vdatum_func( vdatum_func *hrf );
int identical_vdatum_func( vdatum_func *hrf1, vdatum_func *hrf2 );
int calc_vdatum_func( vdatum_func *hrf, double llh[3], double *height, double *exu );

#endif
