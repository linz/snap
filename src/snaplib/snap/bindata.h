#ifndef _BINDATA_H
#define _BINDATA_H

/*
   $Log: bindata.h,v $
   Revision 1.1  1995/12/22 17:39:32  CHRIS
   Initial revision

*/

/* The following structure holds indexes data types */

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <cstdio>
#include "snapdata/survdata.h"

struct survdata;
struct trgtdata;

struct bindata {
    std::int64_t loc = 0;   ///< Location of structure on the file
    std::int64_t size = 0;  ///< The size of the data element on the file
    int bintype = 0;        ///< The binary data format - see enum below
    std::vector<unsigned char>
        buffer;  ///< The data element, for SURVDATA a survdata followed by its variable width data

    /// Creates a buffer with room for the largest data element in the file
    bindata();

    /// The buffer as a survdata, which is only valid when bintype is SURVDATA
    survdata* survey_data() {
        return reinterpret_cast<survdata*>(buffer.data());
    }
    [[nodiscard]] const survdata* survey_data() const {
        return reinterpret_cast<const survdata*>(buffer.data());
    }
};

/* Types of binary data format */

enum {
    SURVDATA, /* Survey data */
    NOTEDATA, /* Note to be copied to output file */
    ENDDATA,  /* Marks the end of data in the file */
    NOBINDATATYPES,
    ANYDATATYPE = NOBINDATATYPES
};

int init_bindata(std::FILE* f);
void end_bindata(void);
std::int64_t write_bindata_header(long size, int type);
int read_bindata_header(long* size, int* type);

void init_get_bindata(std::int64_t loc);
int get_bindata(int datatype, bindata& b);
void update_bindata(bindata& b);

std::int64_t save_survdata(survdata* sd);
std::int64_t save_survdata_subset(survdata* sd, int iobs, int type);

std::optional<std::string> get_obs_classification_name(survdata* sd, trgtdata* t, int class_id);

void print_json_observation_types(std::FILE* out);
void print_json_observations(std::FILE* out);

#ifdef BINDATA_C
#define SCOPE
#else
#define SCOPE extern
#endif

SCOPE std::FILE* bindata_file;
SCOPE long nbindata;

#undef SCOPE

#endif
