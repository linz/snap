#include "snapconfig.hpp"

// Standalone unit test for the LineReader class that concord uses to read its
// input (concord/linereader.hpp). Failures are reported to stdout and the
// process exit code is the pass/fail signal (0 = all passed), as for the other
// test-only tools in src/test/.

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

#include "linereader.hpp"

namespace
{

int failures=0;

void check( bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

/// A stream buffer that serves a string but cannot seek, like a pipe.
class PipeBuffer : public std::streambuf
{
public:
    explicit PipeBuffer( const std::string &text ) : _text( text ) { setg( _text.data(), _text.data(), _text.data() + _text.size() ); }
private:
    std::string _text;
};

/// Reads a field from the reader, returning what was found.
TokenResult read( LineReader &reader, std::string &token, const char separator=0, const bool usespace=false, const size_t maxLength=100 )
{
    bool isspace=false;
    return reader.readToken( separator, usespace, isspace, token, maxLength );
}

void check_whitespace_fields()
{
    std::istringstream input( "  abc  def\tghi \n" );
    LineReader reader( input );
    std::string token;
    check( read( reader, token ) == TokenResult::Found && token == "abc", "whitespace: first field, skipping leading blanks" );
    check( read( reader, token ) == TokenResult::Found && token == "def", "whitespace: second field across a run of blanks" );
    check( read( reader, token ) == TokenResult::Found && token == "ghi", "whitespace: third field across a tab" );
    check( read( reader, token ) == TokenResult::EndOfLine && token.empty(), "whitespace: end of line after the last field" );
    check( read( reader, token ) == TokenResult::EndOfInput, "whitespace: the next read finds the input has ended" );
    check( read( reader, token ) == TokenResult::EndOfInput, "whitespace: end of input is repeated" );
}

void check_separator_fields()
{
    std::istringstream input( "a , b,,c \n" );
    LineReader reader( input );
    std::string token;
    check( read( reader, token, ',' ) == TokenResult::Found && token == "a", "separator: blanks before the separator are removed" );
    check( read( reader, token, ',' ) == TokenResult::Found && token == "b", "separator: blanks after the separator are skipped" );
    check( read( reader, token, ',' ) == TokenResult::EmptyField && token.empty(), "separator: adjacent separators give an empty field" );
    check( read( reader, token, ',' ) == TokenResult::Found && token == "c", "separator: the last field is trimmed" );
    check( read( reader, token, ',' ) == TokenResult::EndOfLine, "separator: end of line" );
}

void check_blank_or_separator()
{
    // A field ending at a blank is reported as such, unless the separator follows the blanks.
    std::istringstream input( "45 17,15 S\n45 , 17\n" );
    LineReader reader( input );
    std::string token;
    bool isspace=false;
    check( reader.readToken( ',', true, isspace, token, 100 ) == TokenResult::Found && token == "45" && isspace, "blank or separator: 45 ends at a blank" );
    check( reader.readToken( ',', true, isspace, token, 100 ) == TokenResult::Found && token == "17" && ! isspace, "blank or separator: 17 ends at the separator" );
    check( reader.readToken( ',', true, isspace, token, 100 ) == TokenResult::Found && token == "15" && isspace, "blank or separator: 15 ends at a blank" );
    check( reader.readToken( ',', true, isspace, token, 100 ) == TokenResult::Found && token == "S" && ! isspace, "blank or separator: S ends at the end of the line" );
    check( reader.readToken( ',', true, isspace, token, 100 ) == TokenResult::EndOfLine, "blank or separator: end of the first line" );
    check( reader.readToken( ',', true, isspace, token, 100 ) == TokenResult::Found && token == "45" && ! isspace, "blank or separator: a separator after the blanks ends the field" );
    check( reader.readToken( ',', true, isspace, token, 100 ) == TokenResult::Found && token == "17", "blank or separator: the field after the separator" );
}

void check_no_separator_never_reports_isspace()
{
    std::istringstream input( "a b\n" );
    LineReader reader( input );
    std::string token;
    bool isspace=true;
    check( reader.readToken( 0, true, isspace, token, 100 ) == TokenResult::Found && ! isspace, "no separator: a field ending at a blank is not reported as isspace" );
}

void check_maximum_length()
{
    std::istringstream input( "abcdefgh ij\n" );
    LineReader reader( input );
    std::string token;
    check( read( reader, token, 0, false, 3 ) == TokenResult::Found && token == "abc", "maximum length: the field is cut short" );
    check( read( reader, token, 0, false, 3 ) == TokenResult::Found && token == "ij", "maximum length: all of the long field was consumed" );
}

void check_carriage_returns()
{
    std::istringstream input( "a b\r\nc\r\n" );
    LineReader reader( input );
    std::string token;
    check( read( reader, token ) == TokenResult::Found && token == "a", "crlf: first field" );
    check( read( reader, token ) == TokenResult::Found && token == "b", "crlf: a carriage return ends a field" );
    check( read( reader, token ) == TokenResult::EndOfLine, "crlf: end of line after the carriage return" );
    check( read( reader, token ) == TokenResult::Found && token == "c", "crlf: second line" );
}

void check_last_line_without_newline()
{
    std::istringstream input( "a\nb" );
    LineReader reader( input );
    std::string token;
    check( read( reader, token ) == TokenResult::Found && token == "a", "no final newline: first line" );
    check( reader.endsWithNewline(), "no final newline: the first line ends with a newline" );
    check( read( reader, token ) == TokenResult::EndOfLine, "no final newline: end of the first line" );
    check( read( reader, token ) == TokenResult::Found && token == "b", "no final newline: the last line" );
    check( ! reader.endsWithNewline(), "no final newline: the last line has none" );
    check( read( reader, token ) == TokenResult::EndOfInput, "no final newline: the end of that line is the end of the input" );

    std::istringstream blank( "a\n   " );
    LineReader blankReader( blank );
    read( blankReader, token );
    read( blankReader, token );
    check( read( blankReader, token ) == TokenResult::EndOfInput, "no final newline: a blank last line is the end of the input" );
}

void check_blank_lines()
{
    std::istringstream input( "\n\n x\n" );
    LineReader reader( input );
    std::string token;
    check( read( reader, token ) == TokenResult::EndOfLine, "blank lines: first" );
    check( read( reader, token ) == TokenResult::EndOfLine, "blank lines: second" );
    check( read( reader, token ) == TokenResult::Found && token == "x", "blank lines: then a field" );
}

void check_empty_input()
{
    std::istringstream input( "" );
    LineReader reader( input );
    std::string token;
    check( read( reader, token ) == TokenResult::EndOfInput, "empty input: end of input" );
    check( reader.remainder().empty() && ! reader.endsWithNewline(), "empty input: nothing left and no newline" );
}

void check_skip_remaining()
{
    std::istringstream input( "a b c\nd\n\nx\n" );
    LineReader reader( input );
    std::string token;
    read( reader, token );
    reader.skipRemaining();
    check( read( reader, token ) == TokenResult::Found && token == "d", "skipRemaining: the next read is on the next line" );
    reader.skipRemaining();
    reader.skipRemaining();
    check( read( reader, token ) == TokenResult::EndOfLine, "skipRemaining: calling it twice skips only one line" );
    reader.skipRemaining();
    check( read( reader, token ) == TokenResult::Found && token == "x", "skipRemaining: after the end of a line it does not skip the next line" );
}

void check_remainder_and_skip_blanks()
{
    std::istringstream input( "  ! a comment\n\t\nlast" );
    LineReader reader( input );
    reader.skipBlanks();
    check( reader.remainder() == "! a comment", "skipBlanks: remainder starts at the first character" );
    check( reader.endsWithNewline(), "skipBlanks: the line ends with a newline" );
    reader.skipRemaining();
    reader.skipBlanks();
    check( reader.remainder().empty() && reader.endsWithNewline(), "skipBlanks: a blank line has an empty remainder and a newline" );
    reader.skipRemaining();
    reader.skipBlanks();
    check( reader.remainder() == "last" && ! reader.endsWithNewline(), "skipBlanks: the last line has no newline" );
    reader.skipRemaining();
    reader.skipBlanks();
    check( reader.remainder().empty() && ! reader.endsWithNewline(), "skipBlanks: after the last line the input has ended" );
}

void check_record_start()
{
    std::istringstream input( "  a b c\n" );
    LineReader reader( input );
    std::string token;
    reader.skipBlanks();
    reader.markRecordStart();
    read( reader, token );
    read( reader, token );
    check( reader.remainder() == "c", "record: the remainder after two fields" );
    reader.rewindToRecord();
    check( reader.remainder() == "a b c", "record: rewound to the start of the record" );
    check( read( reader, token ) == TokenResult::Found && token == "a", "record: reading again starts with the first field" );

    // Rewinding a line that has been read to its end makes it current again.
    std::istringstream second( "a\nb\n" );
    LineReader finished( second );
    finished.markRecordStart();
    read( finished, token );
    check( read( finished, token ) == TokenResult::EndOfLine, "record: the line is finished" );
    finished.rewindToRecord();
    check( finished.remainder() == "a" && read( finished, token ) == TokenResult::Found && token == "a", "record: a finished line can be read again" );
}

void check_seekable()
{
    std::istringstream file( "a\n" );
    LineReader fileReader( file );
    check( fileReader.seekable(), "seekable: a stream that can seek" );

    PipeBuffer pipeBuffer( "a\n" );
    std::istream pipe( &pipeBuffer );
    LineReader pipeReader( pipe );
    check( ! pipeReader.seekable(), "seekable: a stream that cannot seek is not" );
    std::string token;
    check( read( pipeReader, token ) == TokenResult::Found && token == "a", "seekable: the reader still reads a stream that cannot seek" );
}

}

int main()
{
    check_whitespace_fields();
    check_separator_fields();
    check_blank_or_separator();
    check_no_separator_never_reports_isspace();
    check_maximum_length();
    check_carriage_returns();
    check_last_line_without_newline();
    check_blank_lines();
    check_empty_input();
    check_skip_remaining();
    check_remainder_and_skip_blanks();
    check_record_start();
    check_seekable();

    if( failures == 0 )
    {
        std::cout << "All LineReader tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " LineReader test(s) failed\n";
    return EXIT_FAILURE;
}
