#include "snapconfig.hpp"

#include <iostream>
#include <sstream>
#include <memory>
#include <regex>
#include <vector>
#include <algorithm>
#include <boost/algorithm/string.hpp>

#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/datafile.h"
#include "util/pi.h"

#include "snap/snapcsvstn.hpp"
#include "snap/snapcsvstn.h"
#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "network/network.h"

using namespace LINZ;
using namespace DelimitedTextFile;
using namespace SNAP;

/////////////////////////////////////////////////////////////////
// CsvClassification

SnapCsvStn::CsvClassification::CsvClassification( network *net, const std::string &name ) :
    CsvValue( name ), _classId(0), _net(net)
{
    _classId = network_class_id(_net, name.c_str(), 1 );
}

int SnapCsvStn::CsvClassification::classValue()
{
    return network_class_value_id(_net, _classId, value().c_str(), 1);
}

/////////////////////////////////////////////////////////////////
// CsvClassColumn

SnapCsvStn::CsvClassColumn::CsvClassColumn( network *net, const std::string &classname, const Column *column ) :
    _column(column),
    _net(net)
{
    _classId = network_class_id(_net, classname.c_str(), 1 );
}

/////////////////////////////////////////////////////////////////
// SnapCsvStn

SnapCsvStn::SnapCsvStn( network *net, const std::string &name, const OptionString &config) :
    SnapCsvBase(name, config),
    _code("Station code"),
    _name("Station name"),
    _crdlon("Longitude/easting"),
    _crdlat("Latitude/northing"),
    _crdhgt("Height"),
    _crdund("Geoid height"),
    _crdxi("Deflection east"),
    _crdeta("Deflection north"),
    _crdsys("Coordinate system"),
    _hgttype("Height type"),
    _net(net),
    _haveGeoid(false),
    _ellipsoidalHeights(false)
{
    _parts.push_back(&_code);
    _parts.push_back(&_name);
    _parts.push_back(&_crdlon);
    _parts.push_back(&_crdlat);
    _parts.push_back(&_crdhgt);
    _parts.push_back(&_crdund);
    _parts.push_back(&_crdxi);
    _parts.push_back(&_crdeta);
    _parts.push_back(&_crdsys);
    _parts.push_back(&_hgttype);
}

SnapCsvStn::~SnapCsvStn()
{
}

void SnapCsvStn::readerAttached()
{
    for( auto i = _parts.begin(); i < _parts.end(); i++ )
    {
        (*i)->attach(reader());
    }
    for( auto i = _classifications.begin(); i < _classifications.end(); i++ )
    {
        (*i)->attach(reader());
    }
    _classCols.clear();
    if( reader() )
    {
        for( auto s = _classColNames.begin(); s != _classColNames.end(); s++ )
        {
            findClassColumns( reader(), *s );
        }
    }
}

void SnapCsvStn::findClassColumns( CalcReader *reader, const std::string &colName )
{
    int len = colName.size() - 1;
    if( len < 0 ) return;
    if( colName[len] != '*' )
    {
        Column *c = reader->findColumn( colName );
        if( c )
        {
            _classCols.push_back( CsvClassColumn(_net,colName,c) );
        }
        else
        {
            runtimeError(string("Classification column ") + colName + " is missing in " + reader->streamName());
        }
        return;
    }

    // Handle wildcard names - simple wildcard matches anything starting with a prefix.
    // These are defined as prefix*, or prefix**.
    // prefix** means the classification does not include the matched prefix in the class name

    int offset = 0;
    if( len > 1 && colName[len] == '*')
    {
        len--;
        offset = len;
    }
    string prefix = colName.substr(0,len);

    for( int col = 0; col < reader->columnCount(); col++ )
    {
        const Column *c = &(reader->column(col));
        string name = c->name();
        if( len > 0 )
        {
            string colprefix = name.substr(0,len);
            if( ! boost::iequals(prefix,colprefix) ) continue;
            if( offset ) name = name.substr(offset);
        }
        _classCols.push_back( CsvClassColumn( _net,name, c) );
    }
}

SnapCsvStn::CsvClassification *SnapCsvStn::addClassification( const string &classname )
{
    CsvClassification *clsf = classification(classname);
    if( clsf != 0 )
    {
        definitionError(string("Classification ") + classname + " defined more than once");
    }
    else
    {
        clsf = new CsvClassification( _net, classname );
        _classifications.push_back( std::unique_ptr<CsvClassification>(clsf) );
    }
    return clsf;
}

void SnapCsvStn::addColumnClassification( const string &colname )
{
    _classColNames.push_back( colname );
}

SnapCsvStn::CsvClassification *SnapCsvStn::classification( const string &classname )
{
    for( auto c = _classifications.begin(); c < _classifications.end(); c++ )
    {
        if( boost::iequals( (*c)->name(), classname ) )
        {
            return (*c).get();
        }
    }
    return 0;
}

void SnapCsvStn::loadDefinitionCommand( const string &command, RecordStream &rs )
{

        if( command == "code" ) { loadValueDefinition(rs,_code); }
        else if( command == "name" ) { loadValueDefinition(rs,_name); }
        else if( command == "longitude" ) { loadValueDefinition(rs,_crdlon); }
        else if( command == "easting" ) { loadValueDefinition(rs,_crdlon); }
        else if( command == "latitude" ) { loadValueDefinition(rs,_crdlat); }
        else if( command == "northing" ) { loadValueDefinition(rs,_crdlat); }
        else if( command == "height" ) { loadValueDefinition(rs,_crdhgt); }
        else if( command == "geoid_undulation" ) { loadValueDefinition(rs,_crdund); }
        else if( command == "deflection_east" ) { loadValueDefinition(rs,_crdxi); }
        else if( command == "deflection_north" ) { loadValueDefinition(rs,_crdeta); }
        else if( command == "coordinate_system" ) { loadValueDefinition(rs,_crdsys); }
        else if( command == "height_type" ) { loadValueDefinition(rs,_hgttype); }
        else if( command == "classification" )
        {
            string classname;
            if( rs.record() >> classname )
            {
                CsvClassification *clsf = addClassification( classname );
                loadValueDefinition(rs,*clsf);
            }
            else
            {

            }
        }
        else if( command == "classification_columns" )
        {
            string column;
            while( rs.record() >> column )
            {
                addColumnClassification( column );
            }
        }

        else
        {
            definitionError(string("Invalid option ") + command + " in CSV station format definition");
        }
}


void SnapCsvStn::initiallizeLoadData()
{
    _cscode = "";
    _cs = 0;
    _haveGeoid = _crdund.hasValue() || _crdxi.hasValue() || _crdeta.hasValue();
    _geoidDefined = false;
    _deflectionDefined = false;
    _dataError = false;
    _heightType = "";


}

void SnapCsvStn::loadRecord()
{
    const std::string &cscode = _crdsys.value();
    std::string heightType = _hgttype.value();
    boost::to_lower( heightType );
    if( heightType == "" ) heightType = "orthometric";

    if( cscode == "")
    {
        dataError("Coordinate system not specified");
        return;
    }
    if( _cscode == "" )
    {
        _cscode = cscode;
        _cs = load_coordsys( cscode.c_str());
        if( ! _cs )
        {
            dataError(string("Invalid coordinate system code ") + cscode);
        }
        else
        {
            set_network_coordsys( _net, _cs, 0.0 );
            _projection = (bool) is_projection( _net->crdsys );
            _geocentric = (bool) is_geocentric( _net->crdsys );
        }

    }

    if( ! _cs ) return;
    if( cscode != _cscode )
    {
        dataError(string("Coordinate system code not consistent ")+_cscode+" and "+cscode);
        return;
    }

    if( heightType != "ellipsoidal" && heightType != "orthometric" )
    {
        dataError(string("Invalid height type  ") + heightType );
        return;
    }
    if( _heightType == "" )
    {
        _heightType = heightType;
        _ellipsoidalHeights = heightType == "ellipsoidal";
    }
    else if( _heightType != heightType )
    {
        dataError(string("Inconsistent height types ") + heightType + " and " + _heightType );
        return;
    }

    double crdlat = 0.0;
    double crdlon = 0.0;
    double crdhgt = 0.0;
    double crdund = 0.0;
    double crdxi = 0.0;
    double crdeta = 0.0;

    if( ! (_crdlon >> crdlon) ) dataError("Longitude/easting is missing or invalid");
    if( ! (_crdlat >> crdlat) ) dataError("Latitude/northing is missing or invalid");
    if( _crdhgt.value() != "" && ! (_crdhgt >> crdhgt)) dataError("Invalid height coordinate");

    if( _haveGeoid )
    {
        if( _crdund.value() != "" )
        {
            _geoidDefined = true;
            if( ! (_crdund >> crdund) ) dataError("Invalid geoid undulation");
            if( _ellipsoidalHeights ) crdhgt -= crdund;
        }
        if( _crdxi.value() != "" )
        {
            _deflectionDefined = true;
            if( ! (_crdxi >> crdxi) ) dataError("Invalid deflection east");
        }
        if( _crdeta.value() != "" )
        {
            _deflectionDefined = true;
            if( ! (_crdeta >> crdeta) ) dataError("Invalid deflection north");
        }

    }

    const string &code = _code.value();
    if( code == "")
    {
        dataError("Missing stations code");
    }
    string name = _name.value();
    if( name == "") name = code;

    if( _dataError ) return;

    if( _projection )
    {
        proj_to_geog(_net->crdsys->prj,crdlon,crdlat,&crdlon,&crdlat);
    }
    else if( _geocentric )
    {
        double llh[3];
        llh[0] = crdlon;
        llh[1] = crdlat;
        llh[2] = crdhgt;
        xyz_to_llh( _net->crdsys->rf->el, llh, llh );
        crdlat = llh[CRD_LAT];
        crdlon = llh[CRD_LON];
        crdhgt = llh[CRD_HGT];
    }
    else
    {
        crdlat *= DTOR;
        crdlon *= DTOR;
    }

    crdxi *= STOR;
    crdeta*= STOR;
    station *st = new_network_station( _net,code.c_str(), name.c_str(), crdlat, crdlon, crdhgt, crdxi, crdeta, crdund );

    for( auto c = _classifications.begin(); c < _classifications.end(); c++ )
    {
        set_station_class( st, (*c)->classId(), (*c)->classValue() );
    }
    for( auto cc = _classCols.begin(); cc != _classCols.end(); cc++ )
    {
        string value = cc->value();
        if( value != "" )
        {
            int idclass = cc->classId();
            int idvalue = network_class_value_id( net, idclass, value.c_str(), 1 );
            set_station_class( st, idclass, idvalue );
        }
    }
}

void SnapCsvStn::dataError( const std::string &message )
{
    _dataError = true;
    runtimeError( message );
}

void SnapCsvStn::terminateLoadData()
{
    if( _dataError ) { return; }
    _net->options = 0;
    if( _ellipsoidalHeights ) _net->options += NW_ELLIPSOIDAL_HEIGHTS;
    if( _geoidDefined ) _net->options += NW_GEOID_HEIGHTS;
    if( _deflectionDefined ) _net->options += NW_DEFLECTIONS;
    _net->options += NW_DEC_DEGREES;
}


/////////////////////////////////////////////////////////////////////////////
//
// Global CSV load function

#include "snap/stnadj.h"

int load_snap_csv_stations( const char *filename, const char *options )
{
    int sts = OK;
    try
    {
        OptionString config(options ? options : "");
        std::string format = config.valueOf("format","stn");
        const char *formatfile;
        formatfile = find_file( format.c_str(), ".dtf", filename, FF_TRYALL, CSVFORMAT_CONFIG );
        if( ! formatfile )
        {
            std::ostringstream os;
            os << "Undefined delimited text file format " << format;
            handle_error( INVALID_DATA, os.str().c_str(), 0 );
            return INVALID_DATA;
        }
        string netname = string("Read from ") + filename;
        set_network_name( net, netname.c_str());
        SnapCsvStn csvstn( net, formatfile, config );
        DatafileInput dfi( filename,"station file" );
        csvstn.load( dfi );
        if( dfi.errorCount()) sts = INVALID_DATA;
    }
    catch( Error &error )
    {
        handle_error( INVALID_DATA, error.message().c_str(), error.location().c_str() );
        return INVALID_DATA;
    }
    return sts;
}
