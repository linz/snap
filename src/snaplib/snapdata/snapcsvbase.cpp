#include "snapconfig.hpp"

#include <iostream>
#include <sstream>
#include <memory>
#include <vector>
#include <algorithm>
#ifdef REGEX_BOOST
#include <boost/regex.hpp>
#define RGX boost
#else
#include <regex>
#define RGX std
#endif
#include <boost/algorithm/string.hpp>

#include "snapdata/snapcsvbase.hpp"

#include "util/calcdltfile.hpp"
#include "util/recordstream.hpp"
#include "util/errdef.h"


using namespace LINZ;
using namespace DelimitedTextFile;
using namespace SNAP;

/////////////////////////////////////////////////////////////////
// CsvValue

SnapCsvBase::CsvValue::~CsvValue()
{
}

void SnapCsvBase::CsvValue::attach( CalcReader *reader )
{
    _column = reader ? &reader->addColumn(_name,_value ) : 0;
}

/////////////////////////////////////////////////////////////////
// SnapCsvBase

SnapCsvBase::SnapCsvBase( const std::string &name, const OptionString &config) :
    _definitionLoaded(false),
    _name(name),
    _format( new CsvFormat()),
    _config(config),
    _checkColumnEnabled( false ),
    _reader(0)
{
}

SnapCsvBase::~SnapCsvBase()
{
}

void SnapCsvBase::initiallizeLoadDefinition()
{
}

void SnapCsvBase::terminateLoadDefinition()
{
}

void SnapCsvBase::loadDefinition()
{
    if( ! _definitionLoaded )
    {
        RecordStream rs( _name );
        try
        {
            _defstream = &rs;
            rs.setLeadingComment('!');
            initiallizeLoadDefinition();
            loadDefinition( rs );
            terminateLoadDefinition();
            _definitionLoaded = true;
        }
        catch(...)
        {
            _defstream = 0;
            throw;
        }
    }
}

void SnapCsvBase::loadDefinition( RecordStream &rs )
{
    while( rs.readRecord())
    {
        string command;
        rs.record() >> command;
        boost::to_lower( command );

        if( command == "format_name" )
        {
            string name;
            std::getline( rs.record(), name );
            boost::trim(name);
            _name = name;
        }
        else if( command == "format" )
        {
            string formatStr;
            std::getline( rs.record(), formatStr );
            loadFormatDefinition( formatStr );
        }
        else if( command == "columns" )
        {
            loadColumnsDefinition( rs, _columns );
        }
        else if( command == "required_columns" )
        {
            loadColumnsDefinition( rs, _requiredColumns );
            disableColumnCheck();
        }
        else if( command == "required_configuration" )
        {
            std::string item;
            while( rs.record() >> item )
            {
                if( ! _config.defines(item) )
                {
                    definitionError(string("Require CSV configuration item ") + item );
                }
            }
        }
        else if( command == "skip_lines" )
        {
            int nskip;
            if( rs.record() >> nskip )
            {
                setNSkipLines( nskip );
            }
        }
        else if( command == "lookup")
        {
            string lookupName;
            rs.record() >> lookupName;
            loadLookupDefinition( rs, lookupName );
        }
        else
        {
            loadDefinitionCommand( command, rs );
        }
    }
}

bool SnapCsvBase::load( const std::string &filename )
{
    IstreamInput is(filename);
    return load( is );
}

void SnapCsvBase::attachReader( CalcReader *reader )
{
    _reader = reader;

    readerAttached();

    if( _reader ) checkColumns();
}

bool SnapCsvBase::checkColumns()
{
    bool ok = true;
    if( ! _reader ) return ok;
    std::vector<std::string> *cols = _checkColumnEnabled ? &_usedColumns : &_requiredColumns;

    std::vector< std::string > checkedCols;
    for( auto col = cols->begin(); col < cols->end(); col++ )
    {
        std::string ucol = *col;
        boost::to_upper(ucol);
        if( std::find(checkedCols.begin(),checkedCols.end(),ucol) != checkedCols.end())
        {
            continue;
        }
        if( ! _reader->findColumn(*col))
        {
            _reader->runtimeError(std::string("Column ") + *col + " is not defined");
            ok = false;
        }
    }
    return ok;
}

void SnapCsvBase::normaliseColumnNames( CalcReader *reader )
{
    std::vector<std::string> columnNames;
    RGX::regex re("[^A-Za-z0-9]+");
    for( int i = 0; i < reader->columnCount(); i++ )
    {
        std::string colName = reader->column(i).name();
        boost::trim(colName);
        columnNames.push_back( RGX::regex_replace( colName, re, string("_")));

    }
    reader->setColumnNames(columnNames);
}

bool SnapCsvBase::load( InputBase &input )
{
    try
    {
        loadDefinition();
        CalcReader reader(input, *_format, _options );
        normaliseColumnNames( &reader );
        attachReader( &reader );
        
        if( ! checkColumns() )
        {
            attachReader(0);
            return false;
        }

        if( ! _options.useHeader() && _columns.size() > 0 )
        {
            reader.setColumnNames( _columns );
        }
        initiallizeLoadData();

        while( reader.readRecord())
        {
            loadRecord();
        }
        terminateLoadData();

        attachReader(0);
    }
    catch( Error &error )
    {
        attachReader(0);
        runtimeError( error.message() );
        return false;
    }
    catch(...)
    {
        attachReader(0);
        throw;
    }
    return true;
}

void SnapCsvBase::initiallizeLoadData()
{
}

void SnapCsvBase::terminateLoadData()
{
}

void SnapCsvBase::definitionError( const std::string &message )
{
    std::string filename = "";
    int lineno = -1;
    if( _defstream )
    {
        filename = _defstream->filename();
        lineno = _defstream->lineNo();
    }
    throw Error( message, filename, lineno );
}

void SnapCsvBase::runtimeError( const string &message )
{

    // May want to route this through data file handler to get line number etc
    if( _reader != 0 )
    {
        _reader->runtimeError( message );
    }
    else
    {
        throw Error(message);
    }
}

void SnapCsvBase::loadValueDefinition( RecordStream &rs, CsvValue &value )
{
    string valuestr;
    std::getline(rs.record(), valuestr );
    loadValueDefinition( valuestr, value );
}

void SnapCsvBase::loadValueDefinition( const std::string &valuestr, CsvValue &value )
{
    ConcatValue *cvalue = &(value.cvalue());
    cvalue->clear();
    RGX::regex re(
        "\\s*(?:"
        "(default)|"                            // Start of default (1)
        "(\\-?\\d+\\.?\\d*)|"                   // A number (2)
        "\\\"((?:[^\\\"]|\\\"\\\")*)\\\"|"      // Literal string (3)
        "(?:\\$(\\w+))|"                        // Config item (4)
        "(?:\\@(\\w+)|([a-z_]\\w*))|"           // Column name (5)(6)
        "([a-z_]\\w*)\\(\\s*"                   // Start of lookup (7)
        "(?:\\$(\\w+)|\\@(\\w+)|([a-z_]\\w*))"  // Lookup config/column (8)(9)(10)
        "\\s*\\)|"                              // End of lookup
        "(\\S)"                                 // Invalid content (11)
        ")(?:\\s|$)",                           // Terminating space or end
        RGX::regex_constants::icase
    );
    RGX::sregex_iterator imatch(valuestr.begin(), valuestr.end(), re);
    RGX::sregex_iterator end;
    for( ; imatch != end; imatch++ )
    {
        auto match = *imatch;
        // default ...
        if( match[1].length() > 0 )
        {
            ConcatValue *owner = cvalue;
            cvalue = new ConcatValue();
            owner->setDefault( cvalue );
        }
        // Numeric value
        else if( match[2].length() > 0 )
        {
            cvalue->add( StringValue(match[2].str()));
        }
        // Literal text
        else if( match[3].length() > 0 )
        {
            string text = RGX::regex_replace( match[3].str(),RGX::regex("\\\"\\\""),string("\""));
            cvalue->add( text );
        }
        // Config item
        else if( match[4].length() > 0 )
        {
            std::string value = _config.valueOf(match[4].str());
            cvalue->add(value);
        }

        // Column based value.
        else if( match[5].length() > 0 || match[6].length() > 0 )
        {
            std::string colname = match[5].str() + match[6].str();
            NamedValue col( colname , "" );
            _usedColumns.push_back(colname);
            cvalue->add(col);
        }
        // Lookup value ..
        else if( match[7].length() > 0 ) 
        {
            LookupMap &map = lookup( match[7].str());
            LookupValue *lv = new LookupValue();
            lv->setMap( map );
            if( match[8].length() > 0 )
            {
                lv->setSource(new StringValue(_config.valueOf(match[8].str())));
            }
            else
            {
                lv->setSource(new NamedValue(match[9].str()+match[10].str(),""));
            }
            cvalue->add(lv);
        }
        // Invalid value
        else if( match[11].length() > 0 )
        {
            string bad(match[11].first, valuestr.end() );
            definitionError(string("Invalid value for ")+value.name()+": "+bad);
            break;
        }

    }
    if( ! value.cvalue().hasValue())
    {
        definitionError(string("Value not defined for ")+value.name());
    }
}

void SnapCsvBase::loadLookupDefinition( RecordStream &rs, const std::string &lookupName )
{
    LookupMap &map = lookup(lookupName);
    string key;
    string value;
    bool finished = false;
    while( ! finished && rs.readRecord())
    {
        rs.record() >> key;
        boost::to_lower( key );
        if( key == "end_lookup" )
        {
            finished = true;
        }
        else
        {
            std::getline( rs.record(), value );
            boost::trim( value );
            if( key == "default" )
            {
                map.setDefault( value );
            }
            else
            {
                map.add( key, value );
            }
        }
    }
    if( ! finished )
    {
        definitionError("Lookup definition not terminated");
    }
}

void SnapCsvBase::loadColumnsDefinition( RecordStream &rs, std::vector<std::string> &cols )
{

    std::string colname;
    while( rs.record() >> colname )
    {
        cols.push_back(colname);
    }
}

static char getChar( const std::string &chardef )
{
    char c = 0;
    if( boost::iequals(chardef,string("tab"))) c = '\t';
    else if( boost::iequals(chardef,string("space"))) c = ' ';
    else if( chardef.size() == 1 ) c = chardef[0];
    return c;
}

void SnapCsvBase::loadFormatDefinition( const std::string &formatstr )
{
    RGX::regex re(
        "^\\s*"
        "(?:"
        "(csv)|"                    // CSV format
        "(delimiter\\=whitespace)|"     // Whitespace delimited
        "delimiter\\=(\\S|tab|space)"             // Custom delimited parser
        "(?:\\s+quote=(\\S|tab|space))?"
        "(?:\\s+escape=(\\S|tab|space))?"
        ")"
        "(?:\\s+header=(?:y|(n)))?$", // Read headers? Default is Y,
        RGX::regex_constants::icase
    );
    RGX::smatch match;
    if( RGX::regex_match( formatstr.begin(), formatstr.end(), match, re ))
    {
        if( match[1].length() > 0 ) { _format.reset(new CsvFormat());}
        else if( match[2].length() > 0 ) { _format.reset(new WhitespaceFormat());}
        else
        {
            char delim = getChar( match[3].str());
            char quote = getChar( match[4].str());
            char escape = getChar( match[5].str());
            _format.reset(new CsvFormat(delim,escape,quote));
        }
        if( match[6].length() > 0) _options.setUseHeader( false );
    }
    else
    {
        definitionError(string("Invalid format specification: ") + formatstr );
    }

}

LookupMap &SnapCsvBase::lookup( const std::string &lookupName )
{
    std::string name(lookupName);
    boost::to_lower(name);
    if( ! _lookups.count(name))
    {
        _lookups.insert( pair<string,LookupMap>( name, LookupMap(false) ));
    }
    return _lookups[name];
}

/////////////////////////////////////////////////////////////////////////////
//
// DatafileInput

DatafileInput::DatafileInput( const std::string &filename, const std::string &description ) :
    InputBase( filename),
    _df(df_open_data_file( filename.c_str(), description.c_str() )),
    _check_progress(0),
    _aborted(false)
{
    if( ! _df )
    {
        throw Error(string("Cannot open ") + description + " " + filename );
    }
    _owner = true;
    df_set_data_file_comment( _df, 0 );
    df_set_data_file_quote( _df, 0 );
    df_set_data_file_continuation( _df, 0 );
}

DatafileInput::DatafileInput( DATAFILE *df, int (*check_progress)( DATAFILE *df ) ) :
    InputBase( df_file_name( df )),
    _df(df),
    _check_progress(check_progress),
    _aborted(false)
{
    _owner = false;
    df_set_data_file_comment( df, 0 );
    df_set_data_file_quote( df, 0 );
    df_set_data_file_continuation( df, 0 );
}

DatafileInput::~DatafileInput()
{
    if( _owner && _df )
    {
        df_close_data_file( _df );
        _df = 0;
    }
}

bool DatafileInput::getNextLine( std::string &line )
{
    if( df_read_data_file(_df ) != OK )
    {
        return false;
    }
    line = df_rest_of_line( _df );
    return true;
}

bool DatafileInput::handleError( const Error &error )
{
    int status = INVALID_DATA;
    if( error.type() == Warning )
    {
        status = WARNING_ERROR;
    }
    df_data_file_error( _df, status, error.message().c_str());
    return true;
}

int DatafileInput::errorCount()
{
    return df_data_file_errcount( _df );
}
