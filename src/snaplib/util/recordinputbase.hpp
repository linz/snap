// #pragma once
#ifndef _LINZ_INPUTBASE_HPP
#define _LINZ_INPUTBASE_HPP

#include <iostream>
#include <string>
#include <stdexcept>

namespace LINZ
{

enum RecordErrorType { FatalError, RuntimeError, Warning };

class RecordError : public std::runtime_error
{
public:
    RecordError( const std::string &message, const std::string &filename="", int lineno=-1, int recordno=-1 ) :
        runtime_error( message ),
        _message(message),
        _filename(filename),
        _location(""),
        _lineno(lineno),
        _recordno(recordno),
        _type( RuntimeError )
    { _setLocation(); }
    RecordError( RecordErrorType type, const std::string &message, const std::string &filename="", int lineno=-1, int recordno=-1 ) :
        runtime_error( message ),
        _message(message),
        _filename(filename),
        _location(""),
        _lineno(lineno),
        _recordno(recordno),
        _type( type )
    { _setLocation(); }
    ~RecordError() throw () {};
    const std::string &message() const { return _message; }
    const std::string &location() const { return _location; }
    const std::string &filename() const { return _filename; }
    const int lineno() const { return _lineno; }
    const int recordno() const { return _recordno; }
    RecordErrorType type() const { return _type; }
private:
    void _setLocation();
    std::string _message;
    std::string _filename;
    std::string _location;
    int _lineno;
    int _recordno;
    RecordErrorType _type;
};


// Abstract Base Input class

class RecordInputBase
{
public:
    RecordInputBase( const std::string &name ) : _name(name) {}
    virtual ~RecordInputBase();
    const std::string &name() { return _name;}
    virtual bool getNextLine( std::string &line ) = 0;
    virtual int lineNumber();
    virtual int recordNumber();
    // Return true if the error is handled
    void raiseError( const std::string message );
    virtual bool handleError( const RecordError &error );
protected:
    void setName( const std::string &name ){ _name=name; }
private:
    std::string _name;
};

// Istream input implementation

class IstreamRecordInput : public RecordInputBase
{
public:
    IstreamRecordInput( std::istream &is );
    IstreamRecordInput( const std::string &filename );
    virtual ~IstreamRecordInput();
    virtual bool getNextLine( std::string &line );
    virtual int lineNumber();
private:
    std::istream *_is;
    int _lineno;
    bool _ownInput;
};

} // end of LINZ namespace

#endif // End of #ifdef _LINZ_INPUTBASE_HPP
