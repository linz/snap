#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <locale>
#include <cctype>

#include <boost/algorithm/string.hpp>

#include "recordstream.hpp"

using namespace LINZ;

using std::string;

RecordStream::RecordStream( const string &fname ) :
    _filename(fname),
    _str(fname.c_str())
{
    _recordLineNo = 0;
    _lineNo = 0;
    _comment = 0;
    _leadcomment = 0;
    _contchar = 0;
    _isloaded = true;
}

RecordStream::~RecordStream()
{
}

void RecordStream::setControlChars( char comment, char contchar)
{
    _comment = comment;
    _contchar = contchar;
}

bool RecordStream::_iscomment( const std::string &str )
{
    if( ! _leadcomment ) return false;
    auto first = std::find_if( str.begin(), str.end(),
                            std::not1(std::ptr_fun<int, int>(std::isspace)));
    return first < str.end() && (*first) == _leadcomment;
}

void RecordStream::_trimcomment( std::string &str )
{
    if( ! _comment ) return;
    string::size_type pos = str.find_first_of(_comment);
    if( pos == string::npos ) str.erase(pos);
}

bool RecordStream::_trimcontinue( std::string &str )
{
    boost::trim_right( str );
    if( ! _contchar ) return false;
    string::size_type pos = str.size();
    if( pos > 1 && str[pos-1] == _contchar && ::isspace(str[pos-1]))
    {
        str.resize( pos-1 );
        return true;
    }
    return false;
}


bool RecordStream::readRecord()
{
    if( ! _str ) return false;
    _recordstr.clear();
    _isloaded = false;
    while( true )
    {
        if( ! std::getline( _str, _recordstr )) return false;
        _lineNo++;
        _recordLineNo = _lineNo;
        // If respecting a comment character, then trim everything after it
        if( _iscomment( _recordstr ) ) continue;
        _trimcomment( _recordstr );
        while ( _trimcontinue( _recordstr ))
        {
            string extra;
            if( ! std::getline( _str, extra )) break;
            _lineNo++;
            if( _iscomment( extra ) ) break;
            _trimcomment( extra );
            _recordstr += extra;
        }
        // If string is not empty
        if( _recordstr.size() > 0 ) break;
    }
    return true;
}

std::istringstream &RecordStream::record()
{
    if( ! _isloaded )
    {
        _record.str(_recordstr);
        _record.clear();
        _record.seekg(0);
        _isloaded = true;
    }
    return _record;
}
