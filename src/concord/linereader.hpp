// #pragma once
#ifndef _LINEREADER_HPP
#define _LINEREADER_HPP

#include <cstddef>
#include <istream>
#include <string>
#include <string_view>

/// What readToken() found.
enum class TokenResult
{
    Found,       ///< a field with one or more characters
    EmptyField,  ///< a field with no characters, before a separator or a carriage return
    EndOfLine,   ///< the end of the line, so there was no field. The line is now finished.
    EndOfInput   ///< the end of the input, so there was no field
};

/// Reads input a line at a time and splits the line into fields, for programs
/// that prompt for or read free format values, such as concord.
///
/// The reader loads a line when it needs one: at the start, and after the
/// current line has been finished. A line is finished when readToken() reports
/// its end, or when the caller gives up on the rest of it with skipRemaining().
/// Reading again then loads the next line, so a caller that sees EndOfLine can
/// treat it as an error and stop, or carry on reading from the next line.
class LineReader
{
public:
    /// \param input must outlive the reader.
    explicit LineReader( std::istream &input );

    LineReader( const LineReader & ) = delete;
    LineReader &operator=( const LineReader & ) = delete;

    /// Whether the input is a file, so that a record can be read again from
    /// its start with rewindToRecord(), as opposed to a pipe or the keyboard.
    bool seekable() const { return _seekable; }

    /// Reads the next field of the current line. Spaces and tabs before the
    /// field are skipped, and those after it are removed. The field ends at the
    /// separator, at a carriage return, at the end of the line, and at a space
    /// or tab if usespace is set or there is no separator. The character that
    /// ended the field is consumed, except at the end of the line.
    ///
    /// If there is a separator and the field ended at a space or tab, then
    /// the spaces and tabs after it are skipped as well, as are a separator or
    /// carriage return that follow them. So "a  , b" is two fields, not three.
    ///
    /// \return what was found. The token is empty unless the result is Found.
    TokenResult readToken(
        char separator,        ///< the character that ends a field, or 0 for none
        bool usespace,         ///< true if a space or tab also ends a field
        bool &isspace,         ///< set true if the field ended at a space or tab, rather than at the separator
        std::string &token,    ///< set to the field, cut to maxLength characters
        size_t maxLength );    ///< the most characters to keep, though all of the field is read

    /// Skips spaces and tabs in the current line without reading a field, and
    /// without finishing the line. The next line is loaded first if the current
    /// one is finished. Afterwards remainder() starts at the next character,
    /// and is empty at the end of the line or the input.
    void skipBlanks();

    /// Gives up on the rest of the current line, so that the next read loads
    /// the next line. Does nothing if the line is already finished. remainder()
    /// still gives the unread text until the next line is loaded.
    void skipRemaining();

    /// The unconsumed text of the current line, as for FieldScanner::remainder(),
    /// with any carriage returns in it and without its newline. It is empty once
    /// readToken() has reported the end of the line, or if the input has ended,
    /// and is valid until the next line is loaded.
    std::string_view remainder() const { return _rest; }

    /// Whether the current line ended with a newline, rather than at the end
    /// of the input. False if the input has ended. An empty remainder() is the
    /// end of a line if this is true, and the end of the input if it is false.
    bool endsWithNewline() const;

    /// Remembers the current position in the line as the start of a record.
    void markRecordStart();

    /// Returns to the position remembered by markRecordStart(), and makes the
    /// line current again if it had been finished.
    void rewindToRecord();

private:
    /// Reads the next line if the current one is finished. Does nothing if the
    /// input has ended.
    void _loadLine();
    void _skipBlanks();

    std::istream &_input;         ///< where the lines come from
    const bool _seekable;         ///< whether the input is a file
    bool _lineDone=true;          ///< whether the current line is finished, so the next read loads another
    bool _inputEnded=false;       ///< whether there are no more lines
    std::string _line;            ///< the current line, without its newline
    std::string_view _rest;       ///< the unread part of _line
    size_t _recordStart=0;        ///< offset in _line remembered by markRecordStart()
};

#endif
