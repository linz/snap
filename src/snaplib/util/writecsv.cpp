#include "snapconfig.h"
#include "util/dateutil.h"
#include "util/snapctype.h"
#include "util/textformat.hpp"
#include "util/writecsv.h"

namespace
{
    constexpr char QUOTE = '"';
    constexpr int DEFAULT_DECIMAL_PLACES = 6;
    constexpr std::size_t MAX_HEADER_LENGTH = 32;
}

std::unique_ptr<output_csv> output_csv::open( const std::string &filename, const bool tab_delimited )
{
    std::unique_ptr<output_csv> csv( new output_csv(filename,tab_delimited) );
    if( ! csv->_f.is_open() ) return nullptr;
    return csv;
}

output_csv::output_csv( const std::string &filename, const bool tab_delimited )
    : _f(filename),
      _delim(tab_delimited ? '\t' : ','),
      _quoted(! tab_delimited),
      _delimrep(tab_delimited ? " " : ","),
      _newlinerep(tab_delimited ? " " : "\n")
{
}

void output_csv::endRecord()
{
    _f << '\n';
    _delimit = false;
}

void output_csv::_writeDelimiter()
{
    if( _delimit ) _f << _delim;
    _delimit = true;
}

void output_csv::writeHeader( const std::string_view fieldname )
{
    std::string header;
    for( char ch : fieldname )
    {
        if( ! ISALNUM(ch) ) ch = '_';
        header += ch;
        if( header.size() >= MAX_HEADER_LENGTH ) break;
    }
    writeString( header );
}

void output_csv::writeString( const std::string_view value )
{
    _writeDelimiter();
    if( _quoted ) _f << QUOTE;
    for( const char c : value )
    {
        if( _quoted && c == QUOTE ) _f << QUOTE << QUOTE;
        else if( c == _delim ) _f << _delimrep;
        else if( c == '\n' ) _f << _newlinerep;
        else _f << c;
    }
    if( _quoted ) _f << QUOTE;
}

void output_csv::writeInt( const long value )
{
    _writeDelimiter();
    _f << value;
}

void output_csv::writeDouble( const double value, const int ndp )
{
    _writeDelimiter();
    _f << format_fixed( value, ndp >= 0 ? ndp : DEFAULT_DECIMAL_PLACES );
}

void output_csv::writeNullField()
{
    _writeDelimiter();
}

void output_csv::writeNullFields( const int count )
{
    for( int i = 0; i < count; i++ ) writeNullField();
}

void output_csv::writeDate( const double date )
{
    if( date == UNDEFINED_DATE )
    {
        writeNullField();
        return;
    }
    writeString( date_as_string(date) );
}
