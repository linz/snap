#include "snapconfig.h"
/*
   $Log: networkr.c,v $
   Revision 1.2  1998/05/21 04:00:27  ccrook
   Added geodetic coordinate system to network object and facilitated getting and setting
   coordinates in the network coordinate system.

   Revision 1.1  1995/12/22 20:01:49  CHRIS
   Initial revision

   Revision 1.1  1995/12/22 17:36:53  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "network/network.h"
#include "util/chkalloc.h"
#include "util/datafile.h"
#include "util/filelist.h"
#include "util/dms.h"
#include "util/errdef.h"
#include "util/pi.h"

/* The longest field read from a station coordinate file - longer fields are cut short */
static constexpr size_t MAXFIELDLEN = 255;

/* Worker function used to delete duplicate stations */

static void delete_duplicate_station( station *st, void *data )
{
    DATAFILE *stf=static_cast<DATAFILE *>( data );
    stf->error( INVALID_DATA, "Duplicate station code " + std::string( st->Code ) );
    delete_station(st);
}

/*=============================================================*/
/* Basic routine to read a station data file                   */
/* Reads a SNAP format file, or the very similar geodetic      */
/* database format file.                                       */

int read_network( network *nw, std::string_view fname, int options )
{
    const int gbformat=options & NW_READOPT_GBFORMAT;

    int dfsts = OK;

    nw->clear();

    const std::unique_ptr<DATAFILE> stf = DATAFILE::open( fname, "station coordinate file" );
    if( ! stf ) return FILE_OPEN_ERROR;

    /* Read in the name of the network */

    stf->read_record();
    std::string name;
    if( read_remaining_text( stf->input_string().scanner, name, MAXFIELDLEN ) == FieldResult::Ok )
    {
        nw->name = name;
    }

    /* Read in the coordinate system definition */

    stf->read_record();
    std::string crdsysdef;
    read_remaining_text( stf->input_string().scanner, crdsysdef, MAXFIELDLEN );

    coordsys *cs = load_coordsys( crdsysdef );
    if( !cs )
    {
        stf->error( INVALID_DATA, "Invalid or missing definition of coordinate system");
        nw->clear();
        return INVALID_DATA;
    }

    std::string ignoredMessage;
    set_network_coordsys( nw, cs, 0.0, 0, ignoredMessage );
    delete cs;
    nw->crdsysdef = crdsysdef;
    const bool projection_coords = is_projection( nw->crdsys );
    const bool geocentric_coords = is_geocentric( nw->crdsys );

    char file_options = 0;

    /* If geodetic branch format then skip over comments section */

    if( gbformat )
    {
        stf->skip_to_blank_line();
        stf->read_record();
    }

    else
    {
        /* Otherwise check if the file includes geoid perturbations */

        stf->read_record();
        FieldScanner &scanner = stf->input_string().scanner;
        const FieldScanner unread = scanner;
        std::string word;
        read_string_field( scanner, word, MAXFIELDLEN );

        const bool options_keyword = boost::algorithm::iequals( word, "options" );
        const bool options_record = options_keyword || boost::algorithm::iequals( word, "no_geoid" );
        if( options_keyword )
        {
            file_options = 0;
        }
        else
        {
            /* Previous version used default options of NW_GEOID_HEIGHTS and NW_UNDULATIONS */
            file_options = NW_GEOID_HEIGHTS | NW_DEFLECTIONS;
            scanner = unread;
        }

        if( options_record )
        {
            while( read_string_field( scanner, word, MAXFIELDLEN ) == FieldResult::Ok )
            {
                if( boost::algorithm::iequals( word, "no_geoid" ) )
                {
                    file_options &=  ~NW_GEOID_INFO;
                }
                else if( boost::algorithm::iequals( word, "geoid" ) )
                {
                    file_options |= NW_GEOID_INFO;
                }
                else if( boost::algorithm::iequals( word, "geoid_heights" ) )
                {
                    file_options |= NW_GEOID_HEIGHTS;
                }
                else if( boost::algorithm::iequals( word, "no_geoid_heights" ) )
                {
                    file_options &= ~NW_GEOID_HEIGHTS;
                }
                else if( boost::algorithm::iequals( word, "deflections" ) )
                {
                    file_options |= NW_DEFLECTIONS;
                }
                else if( boost::algorithm::iequals( word, "no_deflections" ) )
                {
                    file_options &= ~ NW_DEFLECTIONS;
                }
                else if( boost::algorithm::iequals( word, "ellipsoidal_heights" ) )
                {
                    file_options |= NW_ELLIPSOIDAL_HEIGHTS;
                    file_options |= NW_EXPLICIT_HGT_TYPE;
                }
                else if( boost::algorithm::iequals( word, "orthometric_heights" ) )
                {
                    file_options &= ~NW_ELLIPSOIDAL_HEIGHTS;
                    file_options |= NW_EXPLICIT_HGT_TYPE;
                }
                else if( boost::algorithm::iequals( word, "degrees" ) )
                {
                    file_options |= NW_DEC_DEGREES;
                }
                else if( word.size() > 2 && boost::algorithm::istarts_with( word, "c=" ) )
                {
                    /* Get the class id, which creates the classification */
                    nw->class_id( word.substr( 2 ), 1 );
                }
                else if( boost::algorithm::iequals( word, "station_orders" ) )
                {
                    nw->class_id( STATION_ORDER_CLASS_NAME, 1 );
                }
                else if( boost::algorithm::iequals( word, "no_station_orders" ) )
                {
                    /* do nothing - option no longer possible but kept to avoid errors */
                }
                else
                {
                    stf->error( INVALID_DATA,"Invalid coordinate file options definition");
                    dfsts = INVALID_DATA;
                    break;
                }
            }
            if( file_options & NW_GEOID_INFO )
            {
                file_options |= NW_EXPLICIT_GEOID;
            }

            stf->read_record();
        }
    }

    /* If height type not explicitly defined then orthometric if have geoid info else
     * depends on height type of the coordinate system */

    if( ! (file_options & NW_EXPLICIT_HGT_TYPE) )
    {
        if( coordsys_heights_orthometric(nw->crdsys) )
        {
            file_options &= ~NW_ELLIPSOIDAL_HEIGHTS;
        }
        else
        {
            file_options |= NW_ELLIPSOIDAL_HEIGHTS;
        }
    }

    /* Geocentric coordinates always have ellipsoidal heights */

    if( geocentric_coords ) file_options |= NW_ELLIPSOIDAL_HEIGHTS;

    /* Now on to the station coordinates themselves */

    nw->options = file_options;
    nw->stnlist = new_station_list();
    double xi = 0.0;
    double eta = 0.0;
    double und = 0.0;
    double lat = 0.0;
    double lon = 0.0;
    double hgt = 0.0;
    const bool degrees = file_options & NW_DEC_DEGREES;
    const int nclass = nw->classification_count();
    std::vector<int> clsids( nclass+1 );

    do
    {
        FieldScanner &scanner = stf->input_string().scanner;
        const auto read_number = [&scanner]( double &value )
        {
            return read_double_field( scanner, value ) == FieldResult::Ok;
        };

        std::string stcode;
        bool sts = read_string_field( scanner, stcode, STNCODELEN ) == FieldResult::Ok;
        if( ! sts ) continue;
        boost::algorithm::to_upper( stcode );

        if( projection_coords )
        {
            double easting = 0.0;
            double northing = 0.0;
            sts = read_number( easting ) && read_number( northing );
            if( sts ) proj_to_geog(nw->crdsys->prj,easting,northing,&lon,&lat);
            if( sts ) sts = read_number( hgt );
        }
        else if( geocentric_coords )
        {
            double xyz[3] = { 0.0, 0.0, 0.0 };
            double llh[3];
            sts = read_number( xyz[0] ) &&
                  read_number( xyz[1] ) &&
                  read_number( xyz[2] );
            if( sts )
            {
                xyz_to_llh( nw->crdsys->rf->el, xyz, llh );
                lat = llh[CRD_LAT];
                lon = llh[CRD_LON];
                hgt = llh[CRD_HGT];
            }
        }
        else if( degrees )
        {
            sts = read_number( lat ) &&
                  read_number( lon ) &&
                  read_number( hgt );
            if( sts ) { lat *= DTOR; lon *= DTOR; }
        }
        else
        {
            std::string lth;
            std::string lnh;
            sts = read_dms_angle_field( scanner, lat ) == FieldResult::Ok &&
                  read_string_field( scanner, lth, 1 ) == FieldResult::Ok &&
                  read_dms_angle_field( scanner, lon ) == FieldResult::Ok &&
                  read_string_field( scanner, lnh, 1 ) == FieldResult::Ok;
            if( sts )
            {
                if( lth[0] == 's' || lth[0] == 'S' ) lat = -lat;
                if( lnh[0] == 'w' || lnh[0] == 'W' ) lon = -lon;
                sts = read_number( hgt );
            }
        }

        if( sts && file_options & NW_DEFLECTIONS )
        {
            sts = read_number( xi ) && read_number( eta );
            if( sts )
            {
                xi = xi*DTOR/3600.0;
                eta = eta*DTOR/3600.0;
            }
        }

        if( sts && file_options & NW_GEOID_HEIGHTS )
        {
            sts = read_number( und );
            if( sts && file_options & NW_ELLIPSOIDAL_HEIGHTS ) hgt -= und;
        }

        if( sts && nclass > 0 )
        {
            std::string value;
            for( int i = 1; i <= nclass; i++ )
            {
                sts = read_string_field( scanner, value, MAXFIELDLEN ) == FieldResult::Ok;
                if( ! sts ) break;
                clsids[i] = nw->class_value_id( i, value, 1 );
            }
        }

        std::string stname;
        if( sts && read_remaining_text( scanner, stname, MAXFIELDLEN ) != FieldResult::Ok ) stname = stcode;

        if( !sts )
        {
            stf->error( INVALID_DATA,"Invalid station coordinate definition");
            dfsts = INVALID_DATA;
            continue;
        }

        station *st = new_network_station( nw, stcode.c_str(), stname.c_str(), lat, lon, hgt, xi, eta, und );
        for( int i = 1; i <= nclass; i++ )
        {
            set_station_class( st, i, clsids[i]);
        }
    }
    while( stf->read_record() == OK );

    /*  Removing this check as can allow empty coordinate files ...
    if( number_of_stations(nw) <= 0 ) {
       df_data_file_error( stf, MISSING_DATA, "No stations defined in the coordinate file");
       if( dfsts == OK ) dfsts = MISSING_DATA;
       clear_network( nw );
       }
    */

    /*
        if( sl_find_station( nw->stnlist, stcode ) > 0 )
        {
            char errmsg [30+STNCODELEN];
            sprintf(errmsg,"Duplicate station code %s",stcode);
            df_data_file_error(stf, INVALID_DATA, errmsg);
            dfsts = INVALID_DATA;
            continue;
        }
    */

    const int sts=remove_duplicate_network_stations( nw, 1, stf.get(), delete_duplicate_station );
    if( dfsts == OK ) dfsts = sts;

    /* If recalculating geoid info then do so without raising errors */
    if( coordsys_heights_orthometric(nw->crdsys) )
    {
        calculate_network_coordsys_geoid( nw, OK );
    }

    return dfsts;
}
