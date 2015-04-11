#include "snapconfig.hpp"

#include <iostream>
#include <sstream>
#include <memory>
#include <regex>
#include <vector>
#include <algorithm>
#include <boost/algorithm/string.hpp>

#include "util/fileutil.h"
#include "util/dateutil.h"
#include "util/datafile.h"

#include "snapdata/stnrecodefile.hpp"
#include "snapdata/stnrecodefile.h"
#include "network/network.h"

using namespace LINZ;
using namespace DelimitedTextFile;
using namespace SNAP;

/////////////////////////////////////////////////////////////////
// CsvClassification


/////////////////////////////////////////////////////////////////
// SnapCsvRecode

SnapCsvRecode::SnapCsvRecode( stn_recode_map *stt, const std::string &name ) :
    SnapCsvBase(name),
    _codefrom("Code from"),
    _codeto("Code to"),
    _datefrom("Date from"),
    _dateto("Date to"),
    _stt(stt)
{
    _parts.push_back(&_codefrom);
    _parts.push_back(&_codeto);
    _parts.push_back(&_datefrom);
    _parts.push_back(&_dateto);
}

SnapCsvRecode::~SnapCsvRecode()
{
}

void SnapCsvRecode::readerAttached()
{
    for( auto i = _parts.begin(); i < _parts.end(); i++ )
    {
        (*i)->attach(reader());
    }
}


void SnapCsvRecode::loadDefinitionCommand( const string &command, RecordStream &rs )
{

        if( command == "codefrom" ) { loadValueDefinition(rs,_codefrom); }
        else if( command == "codeto" ) { loadValueDefinition(rs,_codeto); }
        else if( command == "datefrom" ) { loadValueDefinition(rs,_datefrom); }
        else if( command == "dateto" ) { loadValueDefinition(rs,_dateto); }
        else
        {
            definitionError(string("Invalid option ") + command + " in CSV recode format definition");
        }
}

void SnapCsvRecode::loadRecord()
{
    const std::string &codefrom = _codefrom.value();
    const std::string &codeto = _codeto.value();
    const std::string &datefromstr = _datefrom.value();
    const std::string &datetostr = _dateto.value();
    double datefrom=UNDEFINED_DATE;
    double dateto=UNDEFINED_DATE;

    if( datefromstr != "" && datefromstr != "" )
    {
        datefrom=snap_datetime_parse( datefromstr.c_str(), 0 );
        if( datefrom == UNDEFINED_DATE ) dataError(string("Invalid from date ")+datefromstr);
        return;
    }

    if( datetostr != "" && datetostr != "" )
    {
        dateto=snap_datetime_parse( datetostr.c_str(), 0 );
        if( dateto == UNDEFINED_DATE ) dataError(string("Invalid to date ")+datetostr);
        return;
    }

    add_stn_recode_to_map( _stt, codefrom.c_str(), codeto.c_str(), datefrom, dateto );
}

void SnapCsvRecode::dataError( const std::string &message )
{
    _dataError = true;
    runtimeError( message );
}


/////////////////////////////////////////////////////////////////////////////
//
// Global CSV load function

#include "snap/stnadj.h"

int read_station_recode_file( stn_recode_map *stt, const char *filename, const char *basefile  )
{
    int sts = OK;
    try
    {
        const char *recodefile;
        recodefile=find_file(filename,DFLTSTRCD_EXT,basefile,FF_TRYALL,0);
        if( ! recodefile )
        {
            std::ostringstream os;
            os << "Cannot find station recode file " << filename;
            handle_error( INVALID_DATA, os.str().c_str(), 0 );
            return INVALID_DATA;
        }
        const char *formatfile;
        formatfile = find_file( "stnrecode", ".dtf", recodefile, FF_TRYALL, CSVFORMAT_CONFIG );
        if( ! formatfile )
        {
            std::ostringstream os;
            os << "Undefined delimited text file format stnrecode";
            handle_error( INVALID_DATA, os.str().c_str(), 0 );
            return INVALID_DATA;
        }
        SnapCsvRecode csvstnrecode( stt, formatfile );
        DatafileInput dfi( recodefile,"station recode file" );
        csvstnrecode.load( dfi );
        if( dfi.errorCount()) sts = INVALID_DATA;
    }
    catch( Error &error )
    {
        handle_error( INVALID_DATA, error.message().c_str(), error.location().c_str() );
        return INVALID_DATA;
    }
    return sts;
}
