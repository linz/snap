#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>


using namespace std;

//extern "C"
//{
#include "dbl4/snap_dbl4_interface.h"
#include "dbl4/dbl4_tfm_crd.h"
#include "dbl4_utl_binsrc.h"
#include "dbl4_utl_blob.h"
#include "dbl4_utl_lnzdef.h"
#include "dbl4_utl_shiftmodel.h"
#include "dbl4_utl_error.h"
#include "util/dateutil.h"
//}


class LinzDefModel
{

public:
    LinzDefModel( char *filename, double epoch = 2000.0 );
    ~LinzDefModel();
    bool CalcDeformation( double lon, double lat, double date, double *uxyz );
    bool CalcShift( double lon, double lat, double *uxyz, double *shifted );
    // Calculate the offset between dates if deformation model, or just the offset if shift model
    bool CalcOffset( double lon, double lat, double startdate, double enddate, double *uxyz );
    bool isValid() { return mValid; }
    bool isShiftModel() { return shiftModel != NULL; }

private:
    hBlob blob;
    hBinSrc binsrc;
    hLinzDefModel linzdef;
    hPointShiftModel shiftModel;
    double epoch;
    bool mValid;

};

/* Called when the configuration file includes a deformation command - the
   command is passed to define_deformation as the string model */

// #pragma warning (disable : 4100)


LinzDefModel::LinzDefModel( char *filename, double pepoch )
{
    int sts;

    blob = NULL;
    binsrc = NULL;
    linzdef = NULL;
    shiftModel = NULL;
    epoch = pepoch;

    sts = utlCreateReadonlyFileBlob( filename, &(blob) );
    if( sts == STS_OK ) sts = utlCreateBinSrc( blob, &(binsrc) );
    if( sts == STS_OK )
    {
        sts = utlCreateLinzDef( binsrc, &(linzdef) );
        if( sts != STS_OK )
        {
            sts = utlCreateShiftModel( binsrc, &(shiftModel) );
        }
    }
    mValid = sts == STS_OK;
}


LinzDefModel::~LinzDefModel()
{
    if( linzdef ) { utlReleaseLinzDef(linzdef); linzdef = NULL; }
    if( shiftModel ) { utlReleaseShiftModel(shiftModel); shiftModel = NULL; }
    if( binsrc ) { utlReleaseBinSrc(binsrc); binsrc = NULL; }
    if( linzdef ) { utlBlobClose(blob); blob = NULL; }
}

bool LinzDefModel::CalcDeformation(double lon, double lat, double date, double *uxyz)
{
    StatusType sts;
    if( ! isValid() ) return false;
    if( isShiftModel() ) return false;
    sts = utlCalcLinzDef( linzdef, date, lon, lat, uxyz );
    return sts == STS_OK;
}

bool LinzDefModel::CalcShift( double lon, double lat, double *uxyz, double *shifted )
{
    if( ! isValid() ) return false;
    if( ! isShiftModel()) return false;
    StatusType sts = STS_OK;
    if( uxyz ) sts = utlCalcShiftModel( shiftModel, lon, lat, uxyz );
    if( shifted && sts == STS_OK )
    {
        shifted[CRD_LON] = lon;
        shifted[CRD_LAT] = lat;
        shifted[2] = 0.0;
        sts = utlApplyShiftModel( shiftModel, shifted );
    }
    return sts == STS_OK;
}

bool LinzDefModel::CalcOffset( double lon, double lat, double startdate, double enddate, double *uxyz )
{
    bool result;
    if( isShiftModel())
    {
        result = CalcShift(lon,lat,uxyz,0);
    }
    else
    {
        double uxyz0[3];
        result = CalcDeformation(lon, lat, startdate, uxyz0 );
        result &= CalcDeformation(lon, lat, enddate, uxyz );
        uxyz[0] -= uxyz0[0];
        uxyz[1] -= uxyz0[1];
        uxyz[2] -= uxyz0[2];
    }
    return result;
}

bool readDate( const char *dateStr, double &date )
{
    istringstream is(dateStr);
    int day, month, year;
    char c;
    if( is >> day >> c >> month >> c >> year )
    {
        date = snap_date(year, month, day );
        date = date_as_year(date);
        return true;
    }
    is.clear();
    is.seekg(0);
    if( is >> date )
    {
        return true;
    }
    return false;
}

int main( int argc, char *argv[] )
{
    bool grid = false;
    bool point = false;
    bool shift = false;

    if( argc == 12 && string(argv[4]) == "grid" )
    {
        grid = true;
    }
    else if ( argc == 8 && string(argv[4]) == "point" )
    {
        point = true;
    }
    else if ( argc != 6 )
    {
        cout << "Syntax: deformation_file start_epoch end_epoch points_file output_file" << endl;
        cout << "    or: deformation_file start_epoch end_epoch grid lonmin lonmax latmin latmax nlon nlat output_file" << endl;
        cout << "    or: deformation_file start_epoch end_epoch point lat lon" << endl;
        cout << "Dates entered as decimal year or dd-mm-yyyy" << endl;
        return 0;
    }

    LinzDefModel model( argv[1] );
    if( ! model.isValid() )
    {
        cout << "Deformation model " << argv[1] << " could not be loaded." << endl;
        return 0;
    }

    double startdate = 0, enddate = 0;

    if( model.isShiftModel() )
    {
        cout << "Loading point shift model - dates will be ignored." << endl;
        shift = true;
    }
    else
    {
        if( ! readDate(argv[2],startdate))
        {
            cout << "Invalid startdate date." << endl;
            return 0;
        }
        if( ! readDate(argv[3],enddate))
        {
            cout << "Invalid enddate date." << endl;
            return 0;
        }
    }

    ifstream in;
    double minlon, maxlon, minlat, maxlat, lat, lon;
    int nlat, nlon;

    if( grid )
    {
        minlon = atof(argv[5]);
        maxlon = atof(argv[6]);
        minlat = atof(argv[7]);
        maxlat = atof(argv[8]);
        nlon = atoi(argv[9]);
        nlat = atoi(argv[10]);
        if( minlon == 0.0 || maxlon == 0.0 || minlat == 0.0 || maxlat == 0.0 || nlon < 2 || nlat < 2 )
        {
            cout << "Invalid grid definition." << endl;
        }
    }
    else if ( point )
    {
        lon = atof(argv[5]);
        lat = atof(argv[6]);
        if( lon == 0.0 || lat == 0.0 )
        {
            cout << "Invalid point definition." << endl;
        }
    }
    else
    {
        in.open(argv[4]);
        if( ! in )
        {
            cout << "Could not read input points file " << argv[4] << endl;
            return 0;
        }
    }
    ostream *out = &cout;
    ofstream *fout = 0;
    string outfile(argv[argc-1]);
    if( outfile != "-" )
    {
        fout = new ofstream(outfile.c_str());
        if( ! *fout )
        {
            cout << "Could not create output points file " << argv[5] << endl;
        }
        out = fout;
    }

    if(point)
    {
        double uxyz0[3], uxyz1[3];
        if( out == &cout )
        {
            (*out) << setprecision(8);
            (*out) << "Location: " << resetiosflags(ios::fixed) << lon << " " << lat << endl;
            if( shift )
            {
                model.CalcShift(lon,lat,uxyz0,uxyz1);
                (*out) << setiosflags( ios::fixed ) <<  setprecision(4);
                (*out) << "Point shift: " << uxyz0[0] << " " << uxyz0[1] << " " << uxyz0[2] << endl;
                (*out) << setiosflags( ios::fixed ) <<  setprecision(8);
                (*out) << "Shifted coords: " << uxyz1[CRD_LON] << " " << uxyz1[CRD_LAT] << " " << uxyz1[2] << endl;
            }
            else
            {
                model.CalcDeformation(lon, lat, startdate, uxyz0 );
                model.CalcDeformation(lon, lat, enddate, uxyz1 );
                (*out) << "Start date: " << startdate << endl;
                (*out) << "End date: " << enddate << endl;
                (*out) << setiosflags( ios::fixed ) <<  setprecision(4);
                (*out) << "Start dislocation: " << uxyz0[0] << " " << uxyz0[1] << " " << uxyz0[2] << endl;
                (*out) << "End dislocation: " << uxyz1[0] << " " << uxyz1[1] << " " << uxyz1[2] << endl;
                (*out) << "Difference: " << (uxyz1[0]-uxyz0[0])
                       << " " << (uxyz1[1]-uxyz0[1])
                       << " " << (uxyz1[2]-uxyz0[2])
                       << endl;
            }
        }
        else
        {
            model.CalcOffset(lon, lat, startdate, enddate, uxyz1 );
            (*out) << "lon\tlat\tux\tuy\tuz\n";
            (*out) << resetiosflags( ios::fixed ) << setprecision(10)
                   << lon << "\t" << lat
                   << setiosflags( ios::fixed )
                   << setprecision(4)
                   << "\t" << uxyz1[0]
                   << "\t" << uxyz1[1]
                   << "\t" << uxyz1[2]
                   << endl;
        }
    }
    else if( grid )
    {
        (*out) << "lon\tlat\tux\tuy\tuz\n";
        double dlon = (maxlon-minlon)/(nlon-1);
        double dlat = (maxlat-minlat)/(nlat-1);
        int nlt = nlat;
        for( double lat = minlat; nlt--; lat+=dlat)
        {
            int nln = nlon;
            for( double lon = minlon; nln--; lon+=dlon)
            {
                double uxyz[3];
                model.CalcOffset(lon, lat, startdate, enddate, uxyz );
                (*out) << resetiosflags( ios::fixed ) << setprecision(10)
                       << lon << "\t" << lat
                       << setiosflags( ios::fixed )
                       << setprecision(4)
                       << "\t" << uxyz[0]
                       << "\t" << uxyz[1]
                       << "\t" << uxyz[2]
                       << endl;
            }
        }
    }
    else
    {
        (*out) << "lon\tlat\tux\tuy\tuz\n";
        string buffer;
        while( getline(in,buffer))
        {
            istringstream is(buffer);
            double lon, lat;
            if( is >> lon >> lat )
            {
                double uxyz[3];
                model.CalcOffset(lon, lat, startdate, enddate, uxyz );
                (*out)  << resetiosflags( ios::fixed ) << setprecision(10)
                        << lon << "\t" << lat
                        << setiosflags( ios::fixed )
                        << setprecision(4)
                        << "\t" << uxyz[0]
                        << "\t" << uxyz[1]
                        << "\t" << uxyz[2]
                        << endl;
            }
        }
        in.close();
    }
    if( fout ) fout->close();
}