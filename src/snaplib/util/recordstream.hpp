// #pragma once
#ifndef _LINZ_RECORDSTREAM_HPP
#define _LINZ_RECORDSTREAM_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace LINZ
{

/// /////////////////////////////////////////////////////////////////
//  Class RecordStream
//
//  Reads records from a file, ignoring blank records, and optionally
//  accounting for comments and line continuation (line ends with blank
//  plus continuation character)

class RecordStream
{
public:
    RecordStream(const std::string &fname);
    ~RecordStream();

    RecordStream &setComment(char comment = '!')
    {
        _comment = comment;
        return *this;
    }
    RecordStream &setLeadingComment(char leadcomment = '#')
    {
        _leadcomment = leadcomment;
        return *this;
    }
    RecordStream &setContinuation(char contchar = '&')
    {
        _contchar = contchar;
        return *this;
    }

    void setControlChars(char comment = '!', char contchar = '&');

    // Finds the next nonblank record, after removing comments,
    // and joining continued lines
    const std::string &filename() { return _filename; }
    bool good() { return _str.good(); }
    bool readRecord();
    void restart() { _str.seekg(0); };
    int lineNo() { return _lineNo; }

    // Returns the current record as a string
    const std::string &recordString() { return _recordstr; }

    // Returns a stream based on the current record
    std::istringstream &record();

private:
    bool _iscomment(const std::string &str);
    void _trimcomment(std::string &str);
    bool _trimcontinue(std::string &str);
    std::ifstream _str;
    std::string _recordstr;
    std::istringstream _record;
    std::string _filename;
    int _recordLineNo;
    int _lineNo;
    char _leadcomment;
    char _comment;
    char _contchar;
    bool _isloaded;
};

} // End of namespace LINZ

#endif