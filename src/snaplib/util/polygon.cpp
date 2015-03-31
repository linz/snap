
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cstring>

#include "util/polygon.h"

using namespace std;

class wktRing
{
    public:
        wktRing( bool islatlon=1 ) : islatlon( islatlon ) {}
        bool isValid();
        void addPoint( double lon, double lat );
        bool contains( double lon, double lat );
    private:
        struct point 
        { 
            point( double lon, double lat ) : lon(lon), lat(lat) {}
            double lon;
            double lat;
        };
        bool islatlon;
        double minLon;
        double maxLon;
        double minLat;
        double maxLat;
        vector<wktRing::point> points;
};

bool wktRing::isValid()
{
    int npt=points.size();
    if( npt < 3 ) return false;
    if( points[0].lon != points[npt-1].lon || points[0].lat != points[npt-1].lat ) return false;
    return true;
}

void wktRing::addPoint( double lon, double lat )
{
    if( points.size() == 0 ) { minLon=maxLon=lon; minLat=maxLat=lat; }
    if( lon < minLon ) minLon=lon; else if( lon > maxLon ) maxLon=lon;
    if( lat < minLat ) minLat=lat; else if( lat > maxLat ) maxLat=lat;
    points.push_back( wktRing::point( lon, lat ));
}

/* Calc inside by number of crossings of hypothetical line from
 * (-infinity,lat) to (lon,lat)
 */

bool wktRing::contains( double lon, double lat )
{
    int npt=points.size();
    if( npt < 3 ) return false;
    if( islatlon )
    {
        while( lon < minLon ) lon +=360.0;
        while( lon > maxLon ) lon -= 360.0;
    }
    if( lon < minLon || lon > maxLon ) return false;
    if( lat < minLat || lat > maxLat ) return false;
    bool inside=false;
    double lon1=points[0].lon-lon;
    double lat1=points[0].lat-lat;
    for( int i=1; i<npt; i++ )
    {
        double lon0=lon1;
        double lat0=lat1;
        lon1=points[i].lon-lon;
        lat1=points[i].lat-lat;
        if( lat0 >= 0.0 && lat1 >= 0.0 ) continue;
        if( lat0 < 0.0 && lat1 < 0.0 ) continue;
        double lonc=(lat1*lon0-lat0*lon1)/(lat1-lat0);
        if( lonc < 0.0 ) inside = ! inside;
    }
    return inside;
}

class wktPolygon
{
    public:
        wktPolygon( bool islatlon=1 );
        ~wktPolygon();
        int readFile( const char *file );
        bool contains( double lon, double lat );
    private:
        bool islatlon;
        vector<wktRing *> rings;
};
        
wktPolygon::wktPolygon( bool islatlon ) : islatlon(islatlon)
{
}

wktPolygon::~wktPolygon()
{
    int nrings=rings.size();
    for( int i=0; i<nrings; i++ )
    {
        delete rings[i];
        rings[i]=0;
    }
}

bool wktPolygon::contains( double lon, double lat )
{
    bool inside=false;
    int nrings=rings.size();
    for( int i=0; i<nrings; i++ )
    {
        if( rings[i]->contains(lon,lat)) inside=!inside;
    }
    return inside;
}

int wktPolygon::readFile( const char *file )
{
    ifstream f(file);
    if( ! f )
    {
        return 0;
    }

    // Very crude loop to extract rings from WKT and toggle contents.
    // Assumes anything after a '(' is potentially a WKT ring.

    
    while( f )
    {
        char c;
        while(f.get(c)) { if( c == '(' ) break; }
        if( ! f ) break;
        wktRing *ring = 0;
        while( f )
        {
            double lon, lat;
            f >> lon >> lat;
            if (! f ) { if( ! f.eof() ) f.clear();  break; }
            while( f.get(c)) { if( c == ')' || c == ',' ) break; }
            if( ! ring )
            {
                ring = new wktRing( islatlon );
                rings.push_back(ring);
            }
            ring->addPoint(lon,lat);
        }
    }
    int isOk=1;
    for( vector<wktRing *>::const_iterator ri=rings.begin(); ri != rings.end(); ri++ )
    {
        if( ! (*ri)->isValid()){ isOk=0; }
    }
    return isOk;
}

void *read_polygon_wkt( const char *filename, int islonlat )
{
    wktPolygon *pgn = new wktPolygon( islonlat != 0 );
    if( ! pgn->readFile( filename ) )
    {
        return 0;
    }
    return (void *) pgn;
}

void delete_polygon( void *pgn)
{
    wktPolygon *ppgn = (wktPolygon *) pgn;
    if( ppgn ) delete ppgn;
}

int polygon_contains_point( void *pgn, double lon, double lat )
{
    return pgn && ((wktPolygon *)pgn)->contains(lon,lat);
}
