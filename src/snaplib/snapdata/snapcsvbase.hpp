// #pragma once
#ifndef _SNAP_SNAPCSVBASE_HPP
#define _SNAP_SNAPCSVBASE_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <memory.h>
#include <vector>
#include <map>

#include "util/optionstring.hpp"
#include "util/delimitedtextfile.hpp"
#include "util/recordstream.hpp"
#include "util/calcdltfile.hpp"
#include "util/datafile.h"

#define CSVFORMAT_CONFIG "format"

namespace LINZ
{
namespace SNAP
{

using namespace LINZ;
using namespace DelimitedTextFile;

class SnapCsvBase
{
public:
    class CsvValue
    {
    public:
        CsvValue(const std::string &name) : _name(name), _column(0) {}
        ~CsvValue();
        const std::string &name() { return _name; }
        void setName( const std::string name );
        const std::string &value() const { return _column->value(); }
        ConcatValue &cvalue() { return _value; }
        bool hasValue() { return _value.hasValue(); }
        void attach( CalcReader *reader );
        template <typename T> bool operator>>( T &value ) { return (*_column) >> value; }
    private:
        std::string _name;
        ConcatValue _value;
        Column *_column;
    };

public:
    SnapCsvBase( const std::string &name, const OptionString &config = OptionString(""));
    virtual ~SnapCsvBase();
    void disableColumnCheck( bool check = false ) { _checkColumnEnabled = check; }
    bool load( const std::string &filename );
    bool load( RecordInputBase &input );
    const std::string &loadedFilename() const { return _loadedFilename; }
    const std::string &definitionFilename() const { return _definitionFilename; }
protected:
    // Implementation functions
    virtual void initiallizeLoadDefinition();
    virtual void loadDefinitionCommand( const std::string &command, RecordStream &rs ) = 0;
    virtual void terminateLoadDefinition();

    virtual void readerAttached() = 0;
    virtual void initiallizeLoadData();
    virtual void loadRecord() = 0;
    virtual void terminateLoadData();

    // Helper functions
    void definitionError( const string &message );
    void runtimeError( const string &message );
    void loadValueDefinition( RecordStream &rs, CsvValue &value );
    void loadValueDefinition( const std::string &valuestr, CsvValue &value );

    void normaliseColumnNames(CalcReader *reader);
    void attachReader( CalcReader *reader );
    bool checkColumns();

    CalcReader *reader() { return _reader; }

    LookupMap &lookup( const std::string &lookupName );

protected:

private:
    void loadDefinition();
    void loadDefinition( RecordStream &rs );
    void loadLookupDefinition( RecordStream &rs, const std::string &name );
    void loadFormatDefinition( const std::string &formatstr );
    void loadColumnsDefinition( RecordStream &rs, std::vector<std::string> &cols );
    void setNSkipLines( int nskip ) { _options.setNSkipLines( nskip ); }

    bool _definitionLoaded;

    // CSV format options
    std::string _name;
    std::string _description;
    std::string _definitionFilename;
    std::string _loadedFilename;
    std::unique_ptr<Format> _format;
    Options _options;
    std::vector<std::string> _columns;
    OptionString _config;

    bool _checkColumnEnabled;

    // Columns used in the definition
    std::vector<std::string> _usedColumns;
    std::vector<std::string> _requiredColumns;

    // String lookups
    std::map<std::string,LookupMap> _lookups;

    // Currently attached reader
    CalcReader *_reader;

    // Currently used definition input
    RecordStream *_defstream;

};

} // End of namespace SNAP
} // End of namespace LINZ

#endif
