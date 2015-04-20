// #pragma once
#ifndef _LINZ_DELIMITEDTEXTFILE_HPP
#define _LINZ_DELIMITEDTEXTFILE_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

#include "util/recordinputbase.hpp"


namespace LINZ
{
namespace DelimitedTextFile
{

using namespace LINZ;

//! DelimitedTextFile: reading delimited text files (eg CSV files) into fields
/*!
 *  Delimited text files are files delimited by specific characters or character sequences.
 *  The DelimitedTextFile class reads these files, parsing into the component fields and
 *  writing the fields to typed values.
 *
 *  Warning: Column objects read from a Reader cannot be used once the Reader is destroyed.
 *  Currently there is no check on this, it is the programmers reponsibility to limit the
 *  scope of the Column to match the corresponding Reader.
 *
 *  Synopsis:
 *
 *  using namespace LINZ;
 *
 *  DelimitedTextFile::Reader dlt( "test.csv", DelimitedTextFile::Format::CsvFormat )
 *  const DelimitedTextFile::Column &codeCol = dlt['code'];
 *  while( dlt.readRecord )
 *  {
 *      string code;
 *      double value;
 *      try
 *      {
 *          dlt >> code >> value;
 *          // or ..
 *          codeCol >> code;
 *          // or ..
 *          dlt['code'] >> code;
 *          // or ..
 *          dlt[0] >> code;
 *
 *          ...
 *      }
 *      catch( DelimitedTextFile::RecordError *error )
 *      {
 *          cout << error.message() << endl;
 *      }
 *  }
 *
 *  Constructor takes as parameters a filename or input stream, a parser (available
 *  prebuilt parsers are ::CsvFormat, ::TabDelimitedFormat, ::PipeDelimitedFormat, and
 *  ::WhitespaceDelimited), and an Options object
 */


class Reader;


class Column
{
    friend class Reader;
public:
    const std::string &name() const { return _name; }
    const std::string &value() const { return _value; }
    std::string &value() { return _value; }
    int index() const { return _index; }
    template<typename T> bool operator>>( T &tvalue ) const;
    bool operator>>( std::string &strvalue ) const;
private:
    Column( const Reader &dlt, std::string name, int index ) :
        _dlt(dlt), _name(name), _value(""), _index(index) {}
    void setName( const std::string &name ) { _name = name; }
    void clear() { _value.clear(); }
    void handleReadError() const;
    const Reader &_dlt;
    std::string _name;
    std::string _value;
    int _index;
};

// To support the dlt >> value >> value >> .. notation
class IncrementingColumn
{
    friend class Reader;
public:
    template<typename T> IncrementingColumn operator>>( T &tvalue );
private:
    IncrementingColumn( Reader &dlt ) : _dlt(dlt), _index(0) {}
    Reader &_dlt;
    int _index;
};

class CsvFormat;
class WhitespaceFormat;

// Format is a base class for formats, which define how the file is parsed into
// fields.  Derived classes override the clone and parseRecord function.
// The parseRecord function should read one or more lines with the getNextLine
// function.  Each input line is written into the std::string buffer(). The
// values read from the line are written to std::string fields returned by
// successive calls getNextValue().  Errors can be reported with runtimeError,
// which by default throws an exception.

class Format
{
    friend class Reader;
public:
    Format() : _dlt(0) {}
    virtual ~Format() {};
    virtual Format *clone() const = 0;
protected:
    virtual bool parseRecord() = 0;
    bool getNextLine();
    std::string &getNextValue();
    void runtimeError( const std::string message ) const;
    std::string &buffer() { return _buffer; }
private:
    void setSource( Reader *dlt ) { _dlt = dlt; }
    Reader *_dlt;
    std::string _buffer;
public:
    static const CsvFormat Csv;
    static const CsvFormat TabDelimited;
    static const CsvFormat PipeDelimited;
    static const WhitespaceFormat WhitespaceDelimited;
};

class WhitespaceFormat : public Format
{
public:
    WhitespaceFormat() {}
    WhitespaceFormat *clone() const;
protected:
    virtual bool parseRecord();
};

class CsvFormat : public Format
{
public:
    CsvFormat( char delimiter=',', char escape='"', char quote='"') :
        _delimiter(delimiter), _escape(escape), _quote(quote) {}
    CsvFormat *clone() const;
    void setDelimiter( char delimiter ) { _delimiter = delimiter; }
    void setQuote( char quote ) { _quote = quote; }
    void setEscape( char escape ) { _escape = escape; }
protected:
    virtual bool parseRecord();
private:
    char _delimiter;
    char _escape;
    char _quote;
};

class Options
{
    friend class Reader;
public:
    Options() :
        _useHeader(true),
        _fillMissingColumns(true),
        _addExtraColumns(true),
        _ignoreExtraColumns(true),
        _nSkipLines(0)
    {}
    bool useHeader() { return _useHeader; }
    bool fillMissingColumns() { return _fillMissingColumns; }
    bool addExtraColumns() { return _addExtraColumns; }
    bool ignoreExtraColumns() { return _ignoreExtraColumns; }
    int nSkipLines() { return _nSkipLines; }
    Options &setUseHeader( bool useHeader = true ) { _useHeader = useHeader; return *this; }
    Options &setFillMissingColumns( bool fillMissingColumns = true ) { _fillMissingColumns = fillMissingColumns; return *this; }
    Options &setAddExtraColumns( bool addExtraColumns = true ) { _addExtraColumns = addExtraColumns; return *this; }
    Options &setIgnoreExtraColumns( bool ignoreExtraColumns = true ) { _ignoreExtraColumns = ignoreExtraColumns; return *this; }
    Options &setNSkipLines( int nSkipLines ) { _nSkipLines = nSkipLines; return *this; }
private:
    bool _useHeader;
    bool _fillMissingColumns;
    bool _addExtraColumns;
    bool _ignoreExtraColumns;
    int _nSkipLines;
};

// Reader class
class Reader
{
    friend class Format;
    friend class Column;
public:
    Reader( RecordInputBase &input, const Format &parser=Format::Csv, const Options &options = Options() );
    Reader( const std::string filename, const Format &parser = Format::Csv, const Options &options = Options() );
    Reader( std::istream &is, const Format &parser = Format::Csv, const Options &options = Options() );
    virtual ~Reader();

    const std::string &streamName() { return _input->name(); }

    // Configuration

    void setNamesCaseSensitive( bool set = true ) { _namesCaseSensitive = set; }
    void setColumnNames( const std::vector<std::string> &columnNames );
    void setColumnNames( const std::string &columnNames );

    // Getting column information

    int columnCount() const { return _columns.size(); }
    const Column &column( int i ) const { if( i >= (int) _columns.size()) columnIndexError(i); return *(_columns[i]); }
    const Column &column( const std::string &name ) const { return columnNamed( name ); }
    const Column &operator[] ( int i ) const { return column(i); }
    const Column &operator[] ( const std::string &name ) const { return column(name); }
    virtual Column *findColumn( const std::string &name ) const;

    // Reading records

    virtual bool readRecord();

    // Current information

    int lineNo() const { return _lineno; }
    int recordNo() const { return _recordno; }
    void runtimeError( const std::string &message ) const;
    void warning( const std::string &message ) const;

    // Reader >> value >> value ... syntax

    template<typename T> IncrementingColumn operator>>( T &tvalue );

protected:
    Column *newColumn( const std::string &name );
    bool namesMatch( const std::string &name1, const std::string &name2 ) const;

private:
    void setup( const Format &parser );
    void readHeader();
    void columnIndexError( int string ) const;
    void handleError( const RecordError &error ) const;
    Column &columnNamed( const std::string &name ) const;
    // Field parser functions
    bool getNextLine( std::string &buffer );
    std::string &getNextValue();

    // Data members
    std::vector<Column *> _columns;
    RecordInputBase *_input;
    bool _ownInput;
    std::string _dummyValue;
    bool _columnsSet;
    bool _namesCaseSensitive;
    Format *_parser;
    Options _options;
    int _recordno;
    int _lineno;
    int _colno;
    Column *_badColumn;
};

// Inline functions and non-trivial template functions

inline bool Format::getNextLine() { return _dlt ? _dlt->getNextLine(_buffer) : false; }
inline std::string &Format::getNextValue() { return _dlt->getNextValue(); }


template<typename T> bool Column::operator>>( T &tvalue ) const
{
    std::istringstream is(value());
    is >> tvalue;
    if( is.fail() || ! is.eof() ) { handleReadError(); return false; }
    return true;
}
inline bool Column::operator>>( std::string &strvalue ) const
{
    strvalue = value();
    return true;
}

template<typename T> IncrementingColumn IncrementingColumn::operator>>( T &tvalue )
{
    _dlt.column(_index) >> tvalue;
    _index++;
    return *this;
}

template<typename T> IncrementingColumn Reader::operator>>( T &tvalue )
{
    return IncrementingColumn(*this) >> tvalue;
}

} // end of DelimitedTextFile namespace
} // end of LINZ namespace

#endif // End of #ifdef _LINZ_DELIMITEDTEXTFILE_HPP
