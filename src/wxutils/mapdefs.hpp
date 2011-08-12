#ifndef MAPDEFS_HPP
#define MAPDEFS_HPP

// Simple mapping support classes...

class MapPoint
{
public:
    double x, y;
    MapPoint( double x, double y ) : x(x), y(y) {}
    MapPoint() : x(0.0), y(0.0) {}
    MapPoint Shift( double dx, double dy ) const { return MapPoint(x + dx, y + dy); }
    bool operator == (const MapPoint &pt ) { return x == pt.x && y == pt.y; }
};

class MapRect
{
public:
    MapPoint min, max;
    MapRect( double emin, double nmin, double emax, double nmax ) :
        min( emin, nmin ), max( emax, nmax ) {}
    MapRect( const MapPoint &min, const MapPoint &max ) : min(min), max(max) {}
    MapRect() {}
    MapRect ExpandBy( double factor ) const
    {
        double dx = (max.x-min.x)*(factor-1);
        double dy = (max.y-min.y)*(factor-1);
        return MapRect(min.x - dx, min.y - dy, max.x + dx, max.y + dy );
    }
    MapRect Shift( double dx, double dy ) const
    {
        return MapRect( min.Shift( dx, dy ), max.Shift(dx,dy) );
    }
    bool operator == ( const MapRect &rect ) { return min == rect.min && max == rect.max; }
    bool Contains( const MapPoint &pt ) const
    {
        return pt.x >= min.x && pt.y >= min.y && pt.x <= max.x && pt.y <= max.y;
    }

};

#endif // defined MAPDEFS_HPP
