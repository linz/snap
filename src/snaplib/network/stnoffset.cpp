#include "snapconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <boost/numeric/conversion/cast.hpp>

using boost::numeric_cast;

#include "network/network.h"
#include "network/stnoffset.h"
#include "util/dstring.h"
#include "util/datafile.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/geodetic.h"
#include "util/dms.h"
#include "util/errdef.h"
#include "util/pi.h"

/* The longest word read from an offset file - longer words are cut short */
static constexpr size_t WORDLEN = 32;

void add_stn_offset_comp_to_station( station *st, stn_offset_comp comp, const int isdeformation )
{
    if( ! st->ts ) st->ts = new stn_offset;
    stn_offset *sto = static_cast<stn_offset *>( st->ts );
    if( isdeformation ) sto->isdeformation=1;
    sto->components.push_back( std::move( comp ) );
}

/*=============================================================*/
/* Read station offsets from a station offset file             */
/* Reads a SNAP format file, or the very similar geodetic      */


/// Reads the next word if it is one of the keywords, ignoring case. Any other
/// word is left to be read again.
/// \return the keyword read, or nullopt if the next word is not one of them
static std::optional<std::string_view> read_keyword(
    FieldScanner &scanner,                                 ///< the scanner to read from
    std::initializer_list<std::string_view> keywords )     ///< the keywords that may be read
{
    const FieldScanner unread = scanner;
    if( const auto word = scanner.checkAndRecoverQuotedValue( true, std::nullopt ) )
    {
        for( const std::string_view keyword : keywords )
        {
            if( compare_ignoring_case( *word, keyword ) == 0 ) return keyword;
        }
    }
    scanner = unread;
    return std::nullopt;
}

int read_network_station_offsets( network *nw, std::string_view filename )
{
    const std::unique_ptr<DATAFILE> tsf = DATAFILE::open( filename, "station offset file" );
    if( ! tsf ) return FILE_OPEN_ERROR;

    int result=OK;
    std::vector<stn_tspoint> tsdata;
    stn_tspoint basepoint{};

    /* Read in the offsets of each station */

    while( tsf->read_record() == OK )
    {
        FieldScanner &scanner = tsf->input_string().scanner;
        int isxyz=STN_TS_ENU;
        int ord0=0;
        int isdef=0;
        int mode=STN_TS_STEP;
        bool ok=true;
        int sts=OK;
        int ists=0;

        std::string stcode;
        if( read_string_field( scanner, stcode, STNCODELEN ) != FieldResult::Ok ) continue;

        const int stnid=find_station(nw,stcode);
        if( stnid == 0 ) continue;

        const auto coordtype = read_keyword( scanner, { "xyz", "height", "enu" } );
        if( coordtype == "xyz" )
        {
            isxyz=STN_TS_XYZ;
        }
        else if( coordtype == "height" )
        {
            ord0=2;
        }

        if( read_keyword( scanner, { "deformation", "offset" } ) == "deformation" )
        {
            isdef=1;
        }

        const auto modetype = read_keyword( scanner, { "velocity", "time_series", "step" } );
        if( modetype == "velocity" )
        {
            mode=STN_TS_VELOCITY;
        }
        else if( modetype == "time_series" )
        {
            mode=STN_TS_SERIES;
            basepoint.date=0;
        }


        ists=-1; 
        while( ! scanner.atEnd() )
        {
            stn_tspoint *tsp=&basepoint;
            if( ists >= 0 )
            {
                if( static_cast<size_t>( ists ) >= tsdata.size() )
                {
                    tsdata.resize( std::max<size_t>( 32, tsdata.size() * 2 ) );
                }
                tsp=&tsdata[ists];
            }
            tsp->date=0.0;
            if( mode != STN_TS_SERIES || ists >= 0 )
            {
                std::string word;
                ok = read_string_field( scanner, word, WORDLEN ) == FieldResult::Ok;
                if( ok )
                {
                    tsp->date=snap_datetime_parse(word);
                    if( tsp->date==0) ok=false;
                }
            }
            tsp->denu[0]=tsp->denu[1]=tsp->denu[2]=0.0;
            for( int iord=ord0; iord<3; iord++ )
            {
                if( ok ) ok = read_double_field( scanner, tsp->denu[iord] ) == FieldResult::Ok;
            }
            if( ! ok ) break;
            ists++;
            if( mode != STN_TS_SERIES ) break;
        }

        if( ! ok ) sts=INVALID_DATA;
        else if( ists < 0 ) sts=MISSING_DATA;
        else if( ! scanner.atEnd() ) sts=TOO_MUCH_DATA;
        else sts=OK;

        if( sts != OK )
        {
            result=sts;
            tsf->error(sts,"Invalid data in station offset file");
            continue;
        }
        station *stn=station_ptr(nw, stnid);
        stn_offset_comp component( mode, isxyz, numeric_cast<size_t>( ists ) );
        component.basepoint = basepoint;
        std::copy( tsdata.begin(), tsdata.begin() + ists, component.tspoints.begin() );
        add_stn_offset_comp_to_station( stn, std::move( component ), isdef );
    }
    return result;
}

void delete_station_offset( station *st )
{
    delete static_cast<stn_offset *>( st->ts );
    st->ts=nullptr;
}

int station_has_offset( station *st )
{
    return st->ts ? 1 : 0;
}

int station_offset_is_deformation( station *st )
{
    return st->ts ? ((stn_offset *)(st->ts))->isdeformation : 0;
}

void calc_station_offset( station *st, double date, vector3 denu )
{
    stn_offset *sto=static_cast<stn_offset *>( st->ts );

    denu[0]=denu[1]=denu[2]=0;
    if( ! sto ) return;
    for( stn_offset_comp &comp : sto->components )
    {
        vector3 cenu={0.0,0.0,0.0};
        stn_tspoint *tsp=&(comp.basepoint);
        if( comp.mode==STN_TS_STEP )
        {
            if( date < tsp->date ) continue;
            veccopy( tsp->denu, cenu );
        }
        else if( comp.mode==STN_TS_VELOCITY )
        {
            double factor=(date-tsp->date)/DAYS_PER_YEAR;
            veccopy( tsp->denu, cenu );
            scalevec( cenu, factor );
        }
        else if( comp.mode==STN_TS_SERIES && ! comp.tspoints.empty() )
        {
            stn_tspoint *tsp1=tsp;
            double factor=0.0;
            int nts=numeric_cast<int>( comp.tspoints.size() )-1;
            tsp1=comp.tspoints.data();
            if( date >= tsp1->date )
            {
                while( nts > 0 && date >= tsp1->date )
                {
                    tsp=tsp1;
                    tsp1++;
                    nts--;
                }
                if( date >= tsp1->date ) 
                {
                    factor=1.0;
                }
                else
                {
                    factor=(date-tsp->date)/(tsp1->date-tsp->date);
                }
            }
            vecadd2( tsp->denu, 1.0-factor, tsp1->denu, factor, cenu );
        }
        else
        {
            continue;
        }
        if( comp.isxyz == STN_TS_XYZ )
        {
            st->rTopo.rotvec( cenu, cenu );
        }
        vecadd( denu, cenu, denu );
    }
}

void print_station_offset( FILE *lst, station *st )
{
    stn_offset *sto=static_cast<stn_offset *>( st->ts );

    if( ! sto ) return;
    fprintf(lst,"%s %s\n",st->Code.c_str(), sto->isdeformation ? "deformation" : "offset");
    for( stn_offset_comp &comp : sto->components )
    {
        stn_tspoint *tsp=&(comp.basepoint);
        for( int i = -1; i < numeric_cast<int>( comp.tspoints.size() ); i++ )
        {
            int ndp=comp.mode==STN_TS_VELOCITY ? 6 : 4;
            char datestr[20]={0};
            if( i >= 0 ) tsp=&comp.tspoints[i];
            if( i >= 0 || comp.mode != STN_TS_SERIES )
            {
                int y,m,d;
                date_as_ymd(tsp->date,&y,&m,&d);
                sprintf(datestr,"%02d-%02d-%04d",d,m,y);
            }
            fprintf(lst,"    %3s %-11s  %10s  %10.*lf %10.*lf %10.*lf\n",
                    i >= 0 ? "" :
                    comp.isxyz ? "XYZ" : "ENU",
                    i >= 0 ? "" :
                    comp.mode==STN_TS_SERIES ? "time series" :
                    comp.mode==STN_TS_VELOCITY ? "velocity" :
                    comp.mode==STN_TS_STEP ? "offset" : "undefined" ,
                    datestr,
                    ndp,tsp->denu[0],
                    ndp,tsp->denu[1],
                    ndp,tsp->denu[2]
                    );
        }
    }
}
