
/*
   $Log: stnadj.h,v $
   Revision 1.2  1998/06/15 02:27:28  ccrook
   Modified to handle long integer number of observations

   Revision 1.1  1995/12/22 17:50:47  CHRIS
   Initial revision

*/

/* Definition and manipulation of geodetic stations */

#ifndef _STNADJ_H
#define _STNADJ_H

#include <optional>
#include <string>

#ifndef _NETWORK_H
#include "network/network.h"
#endif

#ifndef _STNRECODE_H
#include "snapdata/stnrecode.h"
#endif

#ifndef _BINFILE_H
#include "util/binfile.h"
#endif

/* A geodetic station is defined by a ellipsoidal coordinates,
   gravitational corrections.  It is referenced by an integer number,
   and holds a long integer which is a handle to the name of the
   file (implementation of names to be sorted out!) */

struct stn_adjustment
{
    double  initELat;  /* Initial coordinates */
    double  initELon;
    double  initOHgt;
    int   hrowno;    /* The first row number in the obs equations */
    int   vrowno;    /* The row number for verical coord */
    int   nobsprm;   /* Number of observation set parameters associated with the station */
    int32_t obscount;  /* Count of observations referencing the station */
    int     idcol;     /* Relative station for float error (co-location constraint) */
    float   herror;    /* The horizontal error if the station is floated */
    float   verror;    /* The vertical error if the station is floated */
    struct
    {
        unsigned adj_h:1;
        unsigned adj_v:1;
        unsigned float_h:1;
        unsigned float_v:1;
        unsigned observed:1;
        unsigned ignored:1;
        unsigned rejected:1;
        unsigned autoreject:1;
        unsigned noreorder:1;
        unsigned auto_h:1;
        unsigned auto_v:1;
    } flag;         /* Flags defining what is to be adjusted */
};

enum { STN_FORMAT_SNAP, STN_FORMAT_GB, STN_FORMAT_CSV };

/// The coordinate file the network was first read from.
struct StationFile
{
    const std::string filename; ///< The name of the file as it was specified
    const std::string filespec; ///< The name of the file as it was located, which may include the directory it was found in
};

extern network *net;
extern stn_recode_map *stnrecode;
extern std::optional<StationFile> station_file;
extern std::string output_station_filespec;
extern int station_filetype;
extern std::optional<std::string> geoid_file;
extern char overwrite_geoid;
extern int geoid_error_level;

#define stnptr(istn) (station_ptr(net,(istn)))
#define stnadj(st) ((stn_adjustment *)(st->hook))
#define rejected_station(istn) (istn && stnadj(stnptr(istn))->flag.rejected)
#define ignored_station(istn)  (istn && stnadj(stnptr(istn))->flag.ignored)
#define station_code(istn) (station_ptr(net,(istn))->Code)

int read_station_file( const std::string &fname, const std::string &base_dir, int format, const std::string &options, int mergeopts, double mergedate );
void set_output_station_file( const std::string &fname );
/// Writes the network to a station file. A missing fname is replaced by the output station file,
/// a missing ver by the program version and a missing rtime by the current time.
int write_station_file( const std::optional<std::string> &prog, const std::optional<std::string> &fname,
                        const std::optional<std::string> &ver, const std::optional<std::string> &rtime,
                        int coord_precision, bool rejected );
void set_stnadj_init_network( void );
void reset_stnadj_initial_coords( void );

void dump_stations( BINARY_FILE *b );
int reload_stations( BINARY_FILE *b );

void unload_stations( void );

#endif
