#include <fstream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include "delimitedtextfile.hpp"

namespace LINZ
{
namespace DelimitedTextFile
{

//////////////////////////////////////////////////////////////////////
// Error class

void Error::_setLocation()
{
    if( _filename != "" )
    {
        std::ostringstream location;
        location << "File: " << _filename;
        if( _lineno >= 0 ) location << "  line: " << _lineno;
        if( _recordno >= 0 ) location << "  record: " << _recordno;
        _location = location.str();
    }
}

//////////////////////////////////////////////////////////////////////
// Format implementations used for delimited text files

const CsvFormat Format::Csv;
const CsvFormat Format::PipeDelimited( '|', '\\', 0 );
const CsvFormat Format::TabDelimited( '\t' );
const WhitespaceFormat Format::WhitespaceDelimited;

void Format::runtimeError( const std::string message ) const
{
    if( _dlt )
    {
        _dlt->runtimeError( message );
    }
    else
    {
        throw Error("Source text file not specified in field parser");
    }
}

WhitespaceFormat *WhitespaceFormat::clone() const
{
    return new WhitespaceFormat();
}

bool WhitespaceFormat::parseRecord()
{
    if( ! getNextLine()) return false;
    std::istringstream sstream(buffer());
    while( sstream.good())
    {
        std::string &value = getNextValue();
        sstream >> value;
    }
    return true;
}

CsvFormat *CsvFormat::clone() const
{
    CsvFormat *csv =  new CsvFormat( _delimiter, _escape, _quote );
    return csv;
}

bool CsvFormat::parseRecord()
{

    if( ! getNextLine()) return false;
    std::string &record = buffer();
    std::string::iterator start = record.begin();
    std::string::iterator end = record.end();

    while( true )
    {
        std::string &value = getNextValue();

        bool inQuote = start < end && *start == _quote;
        if( inQuote ) start++;

        // The field in sections up to escape characters
        while( true )
        {
            std::string::iterator endptr = start;
            while(
                endptr < end &&
                *endptr != _quote &&
                *endptr != _delimiter &&
                *endptr != _escape )
                endptr++;
            value.insert(value.end(), start, endptr );
            start = endptr;
            bool escaped = inQuote;
            if( start < end )
            {
                start++;
                if( *endptr == _quote )
                {
                    if(! inQuote )
                    {
                        runtimeError("Misplaced quote character in field");
                        return true;
                    }
                    if( start < end )
                    {
                        if( *start == _delimiter )
                        {
                            endptr++;
                            start++;
                            escaped = false;
                        }
                        else if( _quote != _escape || *start != _quote )
                        {
                            runtimeError("Incorrectly terminated quoted field");
                        }
                    }
                    else
                    {
                        escaped = false;
                    }

                }
                else if( *endptr == _escape )
                {
                    escaped = true;
                }
            }
            if( ! escaped ) break;
            if( start < end )
            {
                value.push_back( *start );
                start++;
            }
            else
            {
                value.push_back('\n');
                if( ! getNextLine())
                {
                    runtimeError("Unterminated field at end of file");
                    return true;
                }
                start = record.begin();
                end = record.end();
            }
        }
        // Trim the end of records for whitespace to ensure CsvColumn >> xx
        // finds eof after reading field
        if( ! inQuote ) boost::trim_right( value );

        if( start >= end ) break;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////
// Column code

void Column::handleReadError() const
{
    std::ostringstream os;
    os << "Error reading field " << _name << " (" << _value << ")";
    _dlt.runtimeError(os.str());
}

//////////////////////////////////////////////////////////////////////
// InputBase code

InputBase::~InputBase() {}
bool InputBase::handleError( const Error &error ) { return false; }

//////////////////////////////////////////////////////////////////////
// IstreamInput code

IstreamInput::IstreamInput( std::istream &is ) : InputBase("unnamed source"), _is(&is), _ownInput(false) {}

IstreamInput::IstreamInput( const std::string &filename ) :
    InputBase(filename),
    _ownInput( false )
{
    _is = new std::ifstream(filename);
    if( ! _is->good())
    {
        delete _is;
        throw Error(std::string("Unable to open file ") + filename);
    }
    _ownInput = true;
}
IstreamInput::~IstreamInput() { if( _ownInput ) delete _is; }

bool IstreamInput::getNextLine( std::string &line )
{

    if( _is->eof()) return false;
    std::getline( *_is, line );
    if( _is->fail()) return false; \
    return true;
}

//////////////////////////////////////////////////////////////////////
// Reader code

Reader::Reader( InputBase &input, const Format &parser, const Options &options ) :
    _input(&input),
    _ownInput( false ),
    _options( options )
{
    _badColumn = 0;
    setup( parser );
}

Reader::Reader( const std::string filename, const Format &parser, const Options &options ) :
    _input(0),
    _ownInput( false ),
    _options( options )
{
    _badColumn = 0;
    _input = new IstreamInput( filename );
    _ownInput = true;
    setup( parser );
}

Reader::Reader( std::istream &is, const Format &parser, const Options &options ) :
    _input( new IstreamInput(is)),
    _ownInput(true),
    _options( options )
{
    setup( parser );
}

Reader::~Reader()
{
    if( _parser ) delete _parser;
    if( _ownInput && _input ) delete _input;
    for( std::vector<Column *>::iterator c = _columns.begin(); c < _columns.end(); c++ )
    {
        delete (*c);
        (*c) = 0;
    }
    if( _badColumn ) delete _badColumn;
}

void Reader::setup( const Format &parser )
{
    _badColumn = new Column(*this,"Undefined",-1);
    _parser = parser.clone();
    _parser->setSource( this );
    _recordno = 0;
    _lineno = 0;
    _columnsSet = false;
    _namesCaseSensitive = false;
    readHeader();
}

void Reader::setColumnNames( const std::vector<std::string> &columnNames )
{
    for( int i = 0; i < columnNames.size(); i++ )
    {
        if( i >= _columns.size())
        {
            _columns.push_back( new Column(*this,columnNames[i],i));
        }
        else
        {
            _columns[i]->setName(columnNames[i]);
        }
    }
    _columnsSet = true;
}

Column *Reader::newColumn( const std::string &name )
{
    Column *c= new Column((*this),name,-1);
    return c;
}

void Reader::setColumnNames( const std::string &columnNames )
{
    std::istringstream sstream(columnNames);
    // Crude tokenizer!
    std::vector<std::string> columns;
    std::string name;
    while( sstream.good())
    {
        sstream >> name;
        if( name != "" )
        {
            columns.push_back(name);
        }
    }
    setColumnNames( columns );
}

void Reader::readHeader()
{
    if( _options._nSkipLines > 0 )
    {
        std::string buffer;
        for( int i = 0; i < _options._nSkipLines; i++ )
        {
            if( ! getNextLine(buffer)) return;
        }
    }
    if( ! _options._useHeader ) return;
    readRecord();
    for( std::vector<Column *>::iterator c = _columns.begin(); c < _columns.end(); c++ )
    {
        if( (*c)->value() != "" )
        {
            (*c)->setName( (*c)->value() );
        }
    }
    _recordno = 0;
}

// Function used by parser to obtain value to load

std::string &Reader::getNextValue()
{
    int index = _colno;
    _colno++;
    if( index > _columns.size())
    {
        runtimeError("Invalid call to getValue from parser");
    }
    else if( index == _columns.size())
    {
        if( ! _columnsSet || _options._addExtraColumns )
        {
            std::ostringstream os;
            os << "col" << (index+1);
            _columns.push_back( new Column(*this, os.str(), index ) );
        }
        else if( _options._ignoreExtraColumns )
        {
            _dummyValue.clear();
            return _dummyValue;
        }
        else
        {
            runtimeError("Extra columns");
        }

    }

    std::string &value = _columns[index]->_value;
    value.clear();
    return value;
}

bool Reader::readRecord()
{
    _colno = 0;
    for( std::vector<Column *>::iterator c = _columns.begin(); c < _columns.end(); c++ )
    {
        (*c)->clear();
    }
    if( ! _parser->parseRecord()) return false;
    _recordno++;
    _columnsSet = true;
    if( _colno < _columns.size() && ! _options._fillMissingColumns)
    {
        runtimeError("Missing columns");
    }
    return _colno >= 0;
}

bool Reader::getNextLine( std::string &buffer )
{
    if( ! _input->getNextLine(buffer) ) return false;
    _lineno++;
    return true;
}

bool Reader::namesMatch( const std::string &name1, const std::string &name2 ) const
{
    if( name1.size() != name2.size()) return false;
    if( _namesCaseSensitive ) return name1 == name2;

    std::string::const_iterator c1 = name1.begin();
    std::string::const_iterator c2 = name2.begin();
    for( ; c1 < name1.end(); c1++, c2++ )
    {
        if( tolower(*c1) != tolower(*c2)) return false;
    }
    return true;
}

Column *Reader::findColumn( const std::string &name ) const
{
    for( int i = 0; i < _columns.size(); i++ )
    {
        if( namesMatch( name, _columns[i]->name() ) ) return _columns[i];
    }
    return 0;
}

Column &Reader::columnNamed( const std::string &name ) const
{
    Column *c = findColumn( name );
    if( c ) return *c;
    std::ostringstream error;
    error << "Invalid column " << name << " requested";
    runtimeError(error.str());
    return *_badColumn;
}

void Reader::columnIndexError( int i ) const
{
    std::ostringstream os;
    os << "Invalid column number " << i << " requested";
    runtimeError(os.str());
}

void Reader::runtimeError( const std::string &message ) const
{
    handleError( Error(message,_input ? _input->name() : "", _lineno, _recordno ));
}

void Reader::warning( const std::string &message ) const
{
    handleError( Error(Warning, message,_input ? _input->name() : "", _lineno, _recordno ));
}
void Reader::handleError( const Error &error ) const
{
    ErrorType type = error.type();
    if( type != FatalError && _input && _input->handleError(error)) return;
    if( type != Warning ) throw error;
}

}  // End of DelimitedTextFile namespace
}  // End of LINZ namespace
