#include "snapconfig.hpp"

#include "linereader.hpp"

#include <algorithm>
#include <optional>

#include "util/fieldscanner.hpp"

namespace
{
    constexpr std::string_view blanks( " \t" );

    bool is_blank( const char c )
    {
        return blanks.find( c ) != std::string_view::npos;
    }
}

LineReader::LineReader( std::istream &input ) :
    _input( input ),
    _seekable( input.rdbuf() && input.rdbuf()->pubseekoff( 0, std::ios_base::cur, std::ios_base::in ) != std::streampos( std::streamoff( -1 ) ) )
{
}

bool LineReader::endsWithNewline() const
{
    return ! _inputEnded && ! _input.eof();
}

void LineReader::_loadLine()
{
    if( ! _lineDone || _inputEnded ) return;
    _line.clear();
    _rest = std::string_view();
    _recordStart = 0;
    if( ! std::getline( _input, _line ) )
    {
        _inputEnded = true;
        _line.clear();
        return;
    }
    _rest = _line;
    _lineDone = false;
}

void LineReader::_skipBlanks()
{
    _rest.remove_prefix( std::min( _rest.find_first_not_of( blanks ), _rest.size() ) );
}

void LineReader::skipBlanks()
{
    _loadLine();
    _skipBlanks();
}

void LineReader::skipRemaining()
{
    _lineDone = true;
}

void LineReader::markRecordStart()
{
    _recordStart = _line.size() - _rest.size();
}

void LineReader::rewindToRecord()
{
    _rest = std::string_view( _line ).substr( _recordStart );
    _lineDone = false;
}

TokenResult LineReader::readToken( const char separator, bool usespace, bool &isspace, std::string &token, const size_t maxLength )
{
    token.clear();
    isspace = false;
    _loadLine();
    if( _inputEnded ) return TokenResult::EndOfInput;

    _skipBlanks();
    usespace = usespace || separator == 0;
    std::string terminators( "\r" );
    if( separator ) terminators += separator;
    if( usespace ) terminators += blanks;

    FieldScanner scanner( _rest );
    char terminator = 0;
    const std::optional<std::string_view> field = scanner.nextUntil( terminators, terminator );
    _rest = scanner.remainder();

    // Nothing left in the line.  At the end of the input, that is the end of
    // the last line when it had no newline.
    if( ! field )
    {
        _lineDone = true;
        return endsWithNewline() ? TokenResult::EndOfLine : TokenResult::EndOfInput;
    }

    const size_t stored = std::min( field->size(), maxLength );
    token.assign( field->substr( 0, stored ) );
    while( ! token.empty() && is_blank( token.back() ) ) token.pop_back();

    // A field that ended at a blank, when there is a separator, can be followed
    // by blanks and then the separator, which belongs to the same field break.
    isspace = usespace && is_blank( terminator ) && terminator != separator;
    if( separator && isspace )
    {
        _skipBlanks();
        if( _rest.empty() )
        {
            isspace = false;
        }
        else if( _rest.front() == separator || _rest.front() == '\r' )
        {
            _rest.remove_prefix( 1 );
            isspace = false;
        }
    }
    isspace = isspace && separator != 0;

    if( stored > 0 ) return TokenResult::Found;
    if( terminator == 0 && ! endsWithNewline() ) return TokenResult::EndOfInput;
    return TokenResult::EmptyField;
}
