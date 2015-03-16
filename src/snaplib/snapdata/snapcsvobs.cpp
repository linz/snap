#include "snapconfig.hpp"

#include <iostream>
#include <sstream>
#include <memory>
#include <vector>
#include <algorithm>
#include <boost/algorithm/string.hpp>

#include "snapdata/snapcsvobs.hpp"
#include "snapdata/snapcsvobs.h"

#include "snapdata/datatype.h"
#include "snapdata/loaddata.h"

#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/datafile.h"
#include "util/pi.h"
#include "util/calcdltfile.hpp"
#include "util/recordstream.hpp"
#include "util/errdef.h"


using namespace LINZ;
using namespace DelimitedTextFile;
using namespace SNAP;

/////////////////////////////////////////////////////////////////
// CsvClassification

SnapCsvObs::CsvClassification::CsvClassification( const std::string &name ) :
    CsvValue( name ), _classId(0)
{
    _classId = ldt_get_id( ID_CLASSTYPE, 0, name.c_str() );
}

int SnapCsvObs::CsvClassification::classValue()
{
    return ldt_get_id( ID_CLASSNAME, _classId, value().c_str());
}

/////////////////////////////////////////////////////////////////
// CsvClassColumn

SnapCsvObs::CsvClassColumn::CsvClassColumn( const std::string &classname, const Column *column )
{
    _classId = ldt_get_id( ID_CLASSTYPE, 0, classname.c_str());
    _column = column;
}

/////////////////////////////////////////////////////////////////
// CsvObservation

SnapCsvObs::CsvObservation::CsvObservation( SnapCsvObs *owner ) :
    _type("Data type"),
    _fromstn("From station"),
    _fromhgt("From inst height"),
    _tostn("To station"),
    _tohgt("To inst height"),
    _value("Obs value"),
    _error("Obs error"),
    _time("Obs data/time"),
    _errorfactor("Error factor"),
    _obsid("Observation id"),
    _projection("Projection"),
    _note("Note"),
    _setid("Observation set id"),
    _rejected("Observation rejected"),
    _ignoremissingobs(false),
    _disterrorcalced( false ),
    _angleerrorcalced( false ),
    _zderrorcalced( false ),
    _hderrorcalced( false ),
    _vecerrorformat(CVR_TOPOCENTRIC),
    _owner(owner),
    _dateformat("YMDhms")
{
    _parts.push_back(&_type);
    _parts.push_back(&_fromstn);
    _parts.push_back(&_fromhgt);
    _parts.push_back(&_tostn);
    _parts.push_back(&_tohgt);
    _parts.push_back(&_value);
    _parts.push_back(&_error);
    _parts.push_back(&_time);
    _parts.push_back(&_errorfactor);
    _parts.push_back(&_obsid);
    _parts.push_back(&_projection);
    _parts.push_back(&_note);
    _parts.push_back(&_setid);
    _parts.push_back(&_rejected);
}

datatypedef *SnapCsvObs::CsvObservation::getDataType()
{
    const std::string & obstype = _type.value();
    return datatypedef_from_code( obstype.c_str());
}

void SnapCsvObs::CsvObservation::definitionError( const string &message )
{
    if( _owner != 0 )
    {
        _owner->definitionError( message );
    }
    else
    {
        throw Error(message);
    }
}

void SnapCsvObs::CsvObservation::runtimeError( const string &message )
{
    if( _owner != 0 )
    {
        _owner->runtimeError( message );
    }
    else
    {
        throw Error(message);
    }
}

string SnapCsvObs::CsvObservation::missing()
{
    std::string missingparts("");
    bool need_tostn = false;
    datatypedef *type = getDataType();
    if( type && ! type->ispoint )
    {
        need_tostn = true;
    }

    if( ! _type.hasValue() ) { missingparts += "type, "; }
    if( ! _fromstn.hasValue() ) { missingparts += "from station, "; }
    if( need_tostn && ! _tostn.hasValue() ) { missingparts += "to station, "; }
    if( ! _value.hasValue() ) { missingparts += "value, "; }
    if( ! _error.hasValue() ) { missingparts += "error, "; }
    if( missingparts.size() > 0) missingparts.resize(missingparts.size()-2);
    return missingparts;
}

void SnapCsvObs::CsvObservation::attach( CalcReader *reader )
{
    for( auto i = _parts.begin(); i < _parts.end(); i++ )
    {
        (*i)->attach(reader);
    }
    for( auto i = _classifications.begin(); i < _classifications.end(); i++ )
    {
        (*i)->attach(reader);
    }
    _classCols.clear();
    if( reader )
    {
        for( auto s = _classColNames.begin(); s != _classColNames.end(); s++ )
        {
            findClassColumns( reader, *s );
        }
    }
}

void SnapCsvObs::CsvObservation::findClassColumns( CalcReader *reader, const std::string &colName )
{
    int len = colName.size() - 1;
    if( len < 0 ) return;
    if( colName[len] != '*' )
    {
        Column *c = reader->findColumn( colName );
        if( c )
        {
            _classCols.push_back( CsvClassColumn(colName,c) );
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
        _classCols.push_back( CsvClassColumn( name, c) );
    }
}

SnapCsvObs::CsvClassification *SnapCsvObs::CsvObservation::addClassification( const string &classname )
{
    CsvClassification *clsf = classification(classname);
    if( clsf != 0 )
    {
        definitionError(string("Classification ") + classname + " defined more than once");
    }
    else
    {
        clsf = new CsvClassification( classname );
        _classifications.push_back( std::unique_ptr<CsvClassification>(clsf) );
    }
    return clsf;
}

void SnapCsvObs::CsvObservation::addColumnClassification( const string &colname )
{
    _classColNames.push_back( colname );
}

SnapCsvObs::CsvClassification *SnapCsvObs::CsvObservation::classification( const string &classname )
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

void SnapCsvObs::CsvObservation::dataError( const std::string &message )
{
    ldt_cancel_inst();
    runtimeError( message );
}

bool SnapCsvObs::CsvObservation::loadObservation()
{
    datatypedef *type = getDataType();
    if( ! type )
    {
        string message("Invalid datatype \"");
        message += _type.value();
        message += "\"";
        dataError(message);
        return false;
    }
    const std::string &setid = _setid.value();
    const std::string &fromcode = _fromstn.value();
    bool continued = false;
    // CSV format cannot handle covariance matrix for vector sets...
    if( setid == "" || type->isvector )
    {
        _owner->EndSet();
    }
    else
    {
        continued = _owner->ContinueSet( setid, fromcode );
    }
    if( _ignoremissingobs && _value.value() == "" )
    {
        return false;
    }
    int idfrom = 0;
    if( _fromstn.value() == "" )
    {
        dataError("From station code is missing");
    }
    else
    {
        idfrom = ldt_get_id( ID_STATION, 0, _fromstn.value().c_str());
        if( idfrom == 0 ) dataError("From station code is invalid");
        _stnidfrom = idfrom;
    }
    int idto = 0;
    if( ! type->ispoint )
    {
        if( _tostn.value() == "" )
        {
            dataError("To station code is missing");
        }
        else
        {
            idto = ldt_get_id( ID_STATION, 0, _tostn.value().c_str());
            if( idto == 0 ) dataError("To station code is invalid");
            _stnidto = idto;
        }
    }
    double fromhgt = 0.0;
    double tohgt = 0.0;
    if( _fromhgt.hasValue() ) _fromhgt >> fromhgt;
    if( _tohgt.hasValue() ) _tohgt >> tohgt;
    double value[3];
    double error[9];
    double errorfactor = 1.0;
    if( _errorfactor.hasValue()) _errorfactor >> errorfactor;
    if( ! type->isvector )
    {
        bool ishorangle = type->id == HA || type->id == AZ || type->id == PB;
        _value >> value[0];
        if( _disterrorcalced && type->isdistance)
        {
            double errcomp;
            error[0] = 0.0;
            string component="";
            // bool ok = true;
            std::istringstream istr(_error.value());
            for( int i = 0; i < 2; i++ )
            {
                istr >> errcomp >> component;
                if( istr.fail()) break;
                double factor;
                if( boost::iequals(component,"mm")) factor = 0.001;
                else if( boost::iequals( component,"ppm")) factor = value[0]*0.000001;
                else
                {
                    dataError(string("Invalid distance error component ") + component);
                    // ok = false;
                    break;
                }
                error[0] += (errcomp*errcomp*factor*factor);

            }
            if( component == "" )
            {
                    dataError(string("Missing distance error") + component);
                    // ok = false;
            }

            if( error[0] > 0.0 ) error[0] = sqrt(error[0]);
        }
        else if( _angleerrorcalced && ishorangle )
        {
            double errcomp;
            double mmerr = 0.0;
            double secerr = 0.0;
            double *pcomp;

            string component="";
            // bool ok = true;
            std::istringstream istr(_error.value());
            for( int i = 0; i < 2; i++ )
            {
                istr >> errcomp >> component;
                if( istr.fail()) break;
                if( boost::iequals(component,"mm")) { errcomp *= 0.001; pcomp = &mmerr; }
                else if( boost::iequals( component,"sec")) { errcomp *= STOR; pcomp = &secerr; }
                else
                {
                    dataError(string("Invalid angle error component ") + component);
                    // ok = false;
                    break;
                }
                *pcomp += errcomp*errcomp;
            }
            if( component == "" )
            {
                    dataError(string("Missing angle error") + component);
                    // ok = false;
            }
 
            if( mmerr > 0.0 )
            {
                double hdist = ldt_calc_value( CALC_HDIST, _stnidfrom, _stnidto );
                hdist *= hdist;
                if( hdist < mmerr/4.0) hdist=mmerr/4.0;
                mmerr /= hdist;
            }
            error[0] = secerr + mmerr;
            if( error[0] > 0.0 ) error[0] = sqrt(error[0])*RTOD;
        }
        else if( _hderrorcalced && type->id == LV )
        {
            double errcomp;
            double runlen;
            double mmerr = 0.0;
            double ppmerr = 0.0;
            double *pcomp;

            string component="";
            // bool ok = true;
            std::istringstream istr(_error.value());
            for( int i = 0; i < 2; i++ )
            {
                istr >> errcomp >> component;
                if( istr.fail()) break;
                if( boost::iequals(component,"mm")) { errcomp *= 0.001; pcomp = &mmerr; }
                else if( boost::iequals(component,"mmrkm")) 
                { 
                    istr >> component >> runlen;
                    if( istr.fail()) 
                    {
                        dataError(string("Incomplete height difference distance error mmrkm component"));
                        // ok = false;
                        break;
                    }
                    if( boost::iequals(component,"sqrt"))
                    {
                        runlen=sqrt(fabs(runlen)/1000.0);
                        errcomp *= 0.001*runlen; pcomp = &ppmerr; 
                    }
                    else
                    {
                        dataError(string("Invalid height difference distance error mmrkm component"));
                        // ok = false;
                        break;
                    }
                }
                else
                {
                    dataError(string("Invalid zenith distance error component ") + component);
                    // ok = false;
                    break;
                }
                *pcomp += errcomp*errcomp;
            }
            if( component == "" )
            {
                    dataError(string("Missing height difference error") + component);
                    // ok = false;
            }
            error[0] = ppmerr + mmerr;
            if( error[0] > 0.0 ) error[0] = sqrt(error[0]);
        }
        else if( _zderrorcalced && type->id == ZD )
        {
            double errcomp;
            double mmverr = 0.0;
            double mmherr = 0.0;
            double secerr = 0.0;
            double *pcomp;

            string component="";
            // bool ok = true;
            std::istringstream istr(_error.value());
            for( int i = 0; i < 3; i++ )
            {
                istr >> errcomp >> component;
                if( istr.fail()) break;
                if( boost::iequals(component,"mmh")) { errcomp *= 0.001; pcomp = &mmherr; }
                else if( boost::iequals(component,"mmv")) { errcomp *= 0.001; pcomp = &mmverr; }
                else if( boost::iequals( component,"sec")) { errcomp *= STOR; pcomp = &secerr; }
                else
                {
                    dataError(string("Invalid zenith distance error component ") + component);
                    // ok = false;
                    break;
                }
                *pcomp += errcomp*errcomp;
            }
            if( component == "" )
            {
                    dataError(string("Missing angle error") + component);
                    // ok = false;
            }
 
            if( mmherr > 0.0 || mmverr > 0.0 )
            {
                double distance = ldt_calc_value( CALC_DISTANCE, _stnidfrom, _stnidto );
                double zendist = value[0] * DTOR;
                distance *= distance;
                if( mmherr < distance/4 ) mmherr /= distance; else mmherr = 4.0;
                if( mmverr < distance/4 ) mmverr /= distance; else mmverr = 4.0;
                mmherr *= cos(zendist)*cos(zendist);
                mmverr *= sin(zendist)*sin(zendist);
            }
            error[0] = secerr + mmherr + mmverr;
            if( error[0] > 0.0 ) error[0] = sqrt(error[0])*RTOD;
        }
        else
        {
            _error >> error[0];
        }
        error[0] *= errorfactor;
        if( type->isangle )
        {
            value[0] *= DTOR;
            error[0] *= DTOR;
        }
    }
    else
    {
        std::istringstream istr;

        istr.str(_value.value());
        istr >> value[0];
        if( type->isvector ) istr >> value[1] >> value[2];
        if( istr.fail() || ! istr.eof() ) dataError("Invalid observation value");

        istr.str(_error.value());
        istr.seekg(0);
        istr.clear();
        if( _nvecerror > 0 )
        {

            for( int i = 0; i < _nvecerror; i++ )
            {
                istr >> error[i];
            }
            if( errorfactor != 1.0 )
            {
                switch( _vecerrorformat )
                {
                case CVR_DEFAULT: break;
                case CVR_FULL:
                {
                    double factor = errorfactor*errorfactor;
                    for( int i = 0; i < 6; i++ ) error[i] *= factor;
                }
                break;
                case CVR_CORRELATION:
                case CVR_ENU_CORRELATION:
                case CVR_TOPOCENTRIC:
                case CVR_DIAGONAL:
                    error[0] *= errorfactor;
                    error[1] *= errorfactor;
                    error[2] *= errorfactor;
                    break;
                default:
                    dataError("Error in CsvObservation::loadObservation - unknown error format");
                }
            }
        }
        else
        {
            for( int i = 0; i < 9; i++ ) error[i] = 0;
            double errcomp[3];
            string component;
            bool ok = true;
            for( int i = 0; i < 2; i++ )
            {
                istr >> errcomp[0] >> errcomp[1] >> errcomp[2] >> component;
                if( istr.fail()) break;
                int offset = 0;
                if( boost::iequals(component,"mm")) offset = 0;
                else if( boost::iequals( component,"ppm")) offset = 3;
                else
                {
                    dataError(string("Invalid error component ") + component);
                    ok = false;
                    break;
                }
                error[offset+0] = errcomp[0]*errorfactor;
                error[offset+1] = errcomp[1]*errorfactor;
                error[offset+2] = errcomp[2]*errorfactor;

                if( istr.eof() ) break;
            }
            if( ok  && (istr.fail() || ! istr.eof())) dataError("Invalid observation error");
        }
    }

    int obsid = 0;
    double snaptime = UNKNOWN_DATE;
    if( _obsid.hasValue() && _obsid.value() != "") _obsid >> obsid;
    if( _time.value() != "" )
    {
        snaptime = snap_datetime_parse( _time.value().c_str(), _dateformat.c_str());
        if( snaptime == 0.0 ) dataError("Invalid date or date format");
    }
    bool obsrejected = false;
    if( _rejected.hasValue() )
    {
        std::string rejstr = _rejected.value();
        boost::to_upper(rejstr);
        if( rejstr == "Y" || rejstr == "YES" )
        {
            obsrejected = true;
        }
    }

    // Add the observation to SNAP...

    ldt_lineno( _owner->reader()->lineNo());

    // Strange SNAP thing about vector point data ...
    if( type->ispoint && type->isvector )
    {
        idto = idfrom; idfrom = 0;
        tohgt = fromhgt; fromhgt = 0;
    }
    if( ! continued ) ldt_inststn( idfrom, fromhgt );
    ldt_tgtstn( idto, tohgt );
    ldt_nextdata( type->id );
    ldt_obs_id( obsid );
    if( obsrejected ) ldt_unused();
    ldt_value( value );
    if( ! type->isvector ) ldt_error( error );
    if( _time.hasValue()) ldt_date( snaptime );
    for( auto c = _classifications.begin(); c < _classifications.end(); c++ )
    {
        ldt_classification( (*c)->classId(), (*c)->classValue() );
    }
    for( auto cc = _classCols.begin(); cc != _classCols.end(); cc++ )
    {
        string value = cc->value();
        if( value != "" )
        {
            int idclass = cc->classId();
            int idvalue = ldt_get_id( ID_CLASSNAME, idclass, value.c_str() );
            ldt_classification( idclass, idvalue );
        }
    }
    if( type->isvector )
    {
        ltmat cvr = ldt_covariance( _vecerrorformat, error );
        switch( _vecerrorformat )
        {
        case CVR_DEFAULT: break;
        case CVR_FULL:
            for( int i3 = 0; i3 < 6; i3++ ) cvr[i3] = error[i3];
            break;

        case CVR_CORRELATION:
        case CVR_ENU_CORRELATION:
            Lij(cvr,0,1) = error[3];
            Lij(cvr,0,2) = error[4];
            Lij(cvr,1,2) = error[5];

            // Then fall through to ...

        case CVR_TOPOCENTRIC:
        case CVR_DIAGONAL:
            Lij(cvr,0,0) = error[0];
            Lij(cvr,1,1) = error[1];
            Lij(cvr,2,2) = error[2];
            break;

        default:
            dataError("Error in CsvObservation::loadObservation - unknown error format (2)");
        }

    }
    if( type->projctn && _projection.value() != "" )
    {
        int id = ldt_get_id( ID_PROJCTN, 0, _projection.value().c_str());
        ldt_projection( id );
    }
    if( _note.value() != "" )
    {
        ldt_note( _note.value().c_str() );
    }
    if( ! _owner->InSet()) ldt_end_data();

    return true;
}

bool SnapCsvObs::CsvObservation::setDistanceErrorType( const string &format )
{
    bool ok = true;
    if( boost::iequals(format,"error"))
    {
        _disterrorcalced = false;
    }
   else if( boost::iequals(format,"calculated"))
    {
        _disterrorcalced = true;
    }
    else
    {
        definitionError(string("Invalid distance error type ") + format );
        ok = false;
    }
    return ok;
}

bool SnapCsvObs::CsvObservation::setAngleErrorType( const string &format )
{
    bool ok = true;
    if( boost::iequals(format,"error"))
    {
        _angleerrorcalced = false;
    }
   else if( boost::iequals(format,"calculated"))
    {
        _angleerrorcalced = true;
    }
    else
    {
        definitionError(string("Invalid angle error type ") + format );
        ok = false;
    }
    return ok;
}

bool SnapCsvObs::CsvObservation::setZenDistErrorType( const string &format )
{
    bool ok = true;
    if( boost::iequals(format,"error"))
    {
        _zderrorcalced = false;
    }
   else if( boost::iequals(format,"calculated"))
    {
        _zderrorcalced = true;
    }
    else
    {
        definitionError(string("Invalid zenith distance error type ") + format );
        ok = false;
    }
    return ok;
}

bool SnapCsvObs::CsvObservation::setHgtDiffErrorType( const string &format )
{
    bool ok = true;
    if( boost::iequals(format,"error"))
    {
        _hderrorcalced = false;
    }
   else if( boost::iequals(format,"calculated"))
    {
        _hderrorcalced = true;
    }
    else
    {
        definitionError(string("Invalid zenith distance error type ") + format );
        ok = false;
    }
    return ok;
}

bool SnapCsvObs::CsvObservation::setVectorErrorType( const string &format )
{
    bool ok = true;
    if( boost::iequals(format,"full"))
    {
        _vecerrorformat = CVR_FULL;
        _nvecerror = 6;
    }
    else if( boost::iequals(format,"correlation"))
    {
        _vecerrorformat = CVR_CORRELATION;
        _nvecerror = 6;
    }
    else if( boost::iequals(format,"diagonal"))
    {
        _vecerrorformat = CVR_DIAGONAL;
        _nvecerror = 3;
    }
    else if( boost::iequals(format,"enu"))
    {
        _vecerrorformat = CVR_TOPOCENTRIC;
        _nvecerror = 3;
    }
    else if( boost::iequals(format,"enu_correlation"))
    {
        _vecerrorformat = CVR_ENU_CORRELATION;
        _nvecerror = 6;
    }
    else if( boost::iequals(format,"calculated"))
    {
        _vecerrorformat = CVR_DEFAULT;
        _nvecerror = 0;
    }
    else
    {
        definitionError(string("Invalid vector error type ") + format );
        ok = false;
    }
    return ok;
}

bool SnapCsvObs::CsvObservation::setDateTimeFormat( const string &format )
{
    bool ok = true;
    if( format.find_first_not_of("YMDhmsN ") != std::string::npos )
    {
        definitionError(string("Invalid date/time format ") + format );
        ok = false;
    }
    else
    {
        _dateformat = format;
    }
    return ok;
}

/////////////////////////////////////////////////////////////////
// SnapCsvObs

SnapCsvObs::SnapCsvObs( const std::string &name, const OptionString &config ) :
    SnapCsvBase( name, config )
{

}

SnapCsvObs::~SnapCsvObs()
{
}

void SnapCsvObs::loadDefinitionCommand( const std::string &command, RecordStream &rs )
{
    if( command == "observation")
    {
        CsvObservation *obs = new CsvObservation( this );
        _observations.push_back( std::unique_ptr<CsvObservation>( obs ) );
        loadObservationDefinition( rs, *obs );
    }
    else
    {
        definitionError(string("Invalid option ") + command + " in CSV observation format definition");
    }
}

void SnapCsvObs::readerAttached()
{
    for( auto o = _observations.begin(); o != _observations.end(); o++ )
    {
        (*o)->attach(reader());
    }
}

static std::string &unquoteString( std::string &value )
{
    if( value.size() > 1 && value[0] == '"' && value[value.size()-1] == '"' )
    {
        value = value.substr(1,value.size()-2);
    }
    return value;
}

void SnapCsvObs::loadObservationDefinition( RecordStream &rs, CsvObservation &obs )
{
    std::string command;
    // bool finished = false;
    while( rs.readRecord() )
    {
        rs.record() >> command;
        boost::to_lower(command);
        if( command == "end_observation" )
        {
            // finished = true;
            break;
        }
        else if( command == "type" ) { loadValueDefinition(rs,obs.type()); }
        else if( command == "set_id" ) { loadValueDefinition(rs,obs.setId()); }
        else if( command == "rejected" ) { loadValueDefinition(rs,obs.rejected()); }
        else if( command == "instrument_station" ) { loadValueDefinition(rs,obs.fromStn()); }
        else if( command == "instrument_height" ) { loadValueDefinition(rs,obs.fromHgt()); }
        else if( command == "target_station" ) { loadValueDefinition(rs,obs.toStn()); }
        else if( command == "target_height" ) { loadValueDefinition(rs,obs.toHgt()); }
        else if( command == "value" ) { loadValueDefinition(rs,obs.value()); }
        else if( command == "error" ) { loadValueDefinition(rs,obs.error()); }
        else if( command == "error_factor" ) { loadValueDefinition(rs,obs.errorFactor()); }
        else if( command == "datetime" ) { loadValueDefinition(rs,obs.time()); }
        else if( command == "id" ) { loadValueDefinition(rs,obs.obsId()); }
        else if( command == "note" ) { loadValueDefinition(rs,obs.note()); }
        else if( command == "projection" ) { loadValueDefinition(rs,obs.projection()); }
        else if( command == "reference_frame" ) { loadCoefDefinition( rs, obs, COEF_CLASS_REFFRM ); }
        else if( command == "refraction_coefficient" ) { loadCoefDefinition( rs, obs, COEF_CLASS_REFCOEF ); }
        else if( command == "bearing_orientation_error" ) { loadCoefDefinition( rs, obs, COEF_CLASS_BRNGREF ); }
        else if( command == "distance_scale_factor" ) { loadCoefDefinition( rs, obs, COEF_CLASS_DISTSF ); }
        else if( command == "classification" )
        {
            string classname;
            if( rs.record() >> classname )
            {
                CsvClassification *clsf = obs.addClassification( classname );
                loadValueDefinition(rs,*clsf);
            }
            else
            {

            }
        }
        else if( command == "vector_error_type" )
        {
            string format;
            if( rs.record() >> format )
            {
                obs.setVectorErrorType(unquoteString(format));
            }
            else
            {
                definitionError("Vector_error_type value missing");
            }
        }
        else if( command == "distance_error_type" )
        {
            string format;
            if( rs.record() >> format )
            {
                obs.setDistanceErrorType(unquoteString(format));
            }
            else
            {
                definitionError("Distance_error_type value is missing");
            }
        }
        else if( command == "angle_error_type" )
        {
            string format;
            if( rs.record() >> format )
            {
                obs.setAngleErrorType(unquoteString(format));
            }
            else
            {
                definitionError("Angle_error_type value is missing");
            }
        }
        else if( command == "zenith_distance_error_type" )
        {
            string format;
            if( rs.record() >> format )
            {
                obs.setZenDistErrorType(unquoteString(format));
            }
            else
            {
                definitionError("Zenith_distance_error_type value is missing");
            }
        }
        else if( command == "height_difference_error_type" )
        {
            string format;
            if( rs.record() >> format )
            {
                obs.setHgtDiffErrorType(unquoteString(format));
            }
            else
            {
                definitionError("Height_difference_error_type value is missing");
            }
        }
        else if( command == "classification_columns" )
        {
            string column;
            while( rs.record() >> column )
            {
                obs.addColumnClassification( column );
            }
        }
        else if( command == "datetime_format" )
        {
            string format;
            if( rs.record() >> format )
            {
                obs.setDateTimeFormat(unquoteString(format));
            }
            else
            {
                definitionError("Date/time format value missing");
            }
        }
        else if( command == "ignore_missing_observations" )
        {
            obs.setIgnoreMissingObs();
        }
        else
        {
            definitionError(string("Invalid component ") + command + " specified for observation");
        }
    }
}

void SnapCsvObs::loadCoefDefinition( RecordStream &rs, CsvObservation &obs, int coef )
{
    CsvClassification *clsf = obs.addClassification( coef_class(coef)->default_classname );
    loadValueDefinition(rs,*clsf);
}

void SnapCsvObs::initiallizeLoadData()
{
    _inset = false;
}

void SnapCsvObs::loadRecord()
{
    for( auto o = _observations.begin(); o != _observations.end(); o++ )
    {
        (*o)->loadObservation();
    }
}

void SnapCsvObs::terminateLoadData()
{
    EndSet();
}

bool SnapCsvObs::ContinueSet( const std::string &setid, const std::string &fromcode )
{
    bool continued = _inset && setid == _setId && fromcode == _setFromCode;
    if( ! continued )
    {
        _setId = setid;
        _setFromCode = fromcode;
        _inset = true;
    }
    return continued;
}

void SnapCsvObs::EndSet()
{
    if( _inset )
    {
        ldt_end_data();
        _inset = false;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// Global CSV load function

int load_snap_csv_obs( const char *options, DATAFILE *df, int (*check_progress)( DATAFILE *df ) )
{
    try
    {
        OptionString config(options ? options : ""); 
        std::string format = config.valueOf("format","obs");
        const char *formatfile;
        formatfile = find_file( format.c_str(), ".dtf", df->fname, FF_TRYALL, CSVFORMAT_CONFIG );
        if( ! formatfile )
        {
            std::ostringstream os;
            os << "Undefined delimited text file format " << format;
            handle_error( INVALID_DATA, os.str().c_str(), 0 );
            return INVALID_DATA;
        }
        SnapCsvObs csvobs( formatfile, config );
        DatafileInput dfi( df, check_progress );
        csvobs.load( dfi );
        if( dfi.aborted() ) return OPERATION_ABORTED;
    }
    catch( Error &error )
    {
        handle_error( INVALID_DATA, error.message().c_str(), error.location().c_str() );
        return INVALID_DATA;
    }
    return OK;
}
