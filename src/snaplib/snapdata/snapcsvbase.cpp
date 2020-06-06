#include "snapconfig.hpp"

#include <iostream>
#include <sstream>
#include <memory>
#include <vector>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include "util/snapregex.hpp"

#include "snapdata/snapcsvbase.hpp"

#include "util/calcdltfile.hpp"
#include "util/recordstream.hpp"
#include "util/errdef.h"

using namespace LINZ;
using namespace DelimitedTextFile;
using namespace SNAP;

/////////////////////////////////////////////////////////////////
// CsvValue

SnapCsvBase::CsvValue::~CsvValue()
{
}

void SnapCsvBase::CsvValue::setName(const std::string name)
{
    _name = name;
    if (_column != 0)
        _column->setName(name);
}

void SnapCsvBase::CsvValue::attach(CalcReader *reader)
{
    _column = reader ? &reader->addColumn(_name, _value) : 0;
}

/////////////////////////////////////////////////////////////////
// SnapCsvBase

SnapCsvBase::SnapCsvBase(const std::string &name, const OptionString &config) : _definitionLoaded(false),
                                                                                _name(name),
                                                                                _format(new CsvFormat()),
                                                                                _config(config),
                                                                                _checkColumnEnabled(false),
                                                                                _reader(0)
{
}

SnapCsvBase::~SnapCsvBase()
{
}

void SnapCsvBase::initiallizeLoadDefinition()
{
}

void SnapCsvBase::terminateLoadDefinition()
{
}

void SnapCsvBase::loadDefinition()
{
    if (!_definitionLoaded)
    {
        RecordStream rs(_name);
        try
        {
            _defstream = &rs;
            rs.setLeadingComment('!');
            initiallizeLoadDefinition();
            loadDefinition(rs);
            terminateLoadDefinition();
            _definitionLoaded = true;
            _definitionFilename = rs.filename();
        }
        catch (...)
        {
            _defstream = 0;
            throw;
        }
    }
}

void SnapCsvBase::loadDefinition(RecordStream &rs)
{
    while (rs.readRecord())
    {
        string command;
        rs.record() >> command;
        boost::to_lower(command);

        if (command == "format_name")
        {
            string name;
            std::getline(rs.record(), name);
            boost::trim(name);
            _name = name;
        }
        else if (command == "format")
        {
            string formatStr;
            std::getline(rs.record(), formatStr);
            loadFormatDefinition(formatStr);
        }
        else if (command == "columns")
        {
            loadColumnsDefinition(rs, _columns);
        }
        else if (command == "required_columns")
        {
            loadColumnsDefinition(rs, _requiredColumns);
            disableColumnCheck();
        }
        else if (command == "required_configuration")
        {
            std::string item;
            while (rs.record() >> item)
            {
                if (!_config.defines(item))
                {
                    definitionError(string("Require CSV configuration item ") + item);
                }
            }
        }
        else if (command == "skip_lines")
        {
            int nskip;
            if (rs.record() >> nskip)
            {
                setNSkipLines(nskip);
            }
        }
        else if (command == "lookup")
        {
            string lookupName;
            rs.record() >> lookupName;
            loadLookupDefinition(rs, lookupName);
        }
        else
        {
            continue;
        }
    }
    rs.restart();
    while (rs.readRecord())
    {
        string command;
        rs.record() >> command;
        boost::to_lower(command);

        if (command == "format_name" || command == "format" || command == "columns" || command == "required_columns" || command == "required_configuration" || command == "skip_lines")
        {
            continue;
        }
        else if (command == "lookup")
        {
            string lookupName;
            rs.record() >> lookupName;
            loadLookupDefinition(rs, lookupName, true);
        }
        else
        {
            loadDefinitionCommand(command, rs);
        }
    }
}

bool SnapCsvBase::load(const std::string &filename)
{
    IstreamRecordInput is(filename);
    return load(is);
}

void SnapCsvBase::attachReader(CalcReader *reader)
{
    _reader = reader;

    readerAttached();

    if (_reader)
        checkColumns();
}

bool SnapCsvBase::checkColumns()
{
    bool ok = true;
    if (!_reader)
        return ok;
    std::vector<std::string> *cols = _checkColumnEnabled ? &_usedColumns : &_requiredColumns;

    std::vector<std::string> checkedCols;
    for (auto col = cols->begin(); col < cols->end(); col++)
    {
        std::string ucol = *col;
        boost::to_upper(ucol);
        if (std::find(checkedCols.begin(), checkedCols.end(), ucol) != checkedCols.end())
        {
            continue;
        }
        if (!_reader->findColumn(*col))
        {
            _reader->runtimeError(std::string("Column ") + *col + " is not defined");
            ok = false;
        }
    }
    return ok;
}

void SnapCsvBase::normaliseColumnNames(CalcReader *reader)
{
    std::vector<std::string> columnNames;
    RGX::regex re("[^A-Za-z0-9]+");
    for (int i = 0; i < reader->columnCount(); i++)
    {
        std::string colName = reader->column(i).name();
        boost::trim(colName);
        columnNames.push_back(RGX::regex_replace(colName, re, string("_")));
    }
    reader->setColumnNames(columnNames);
}

bool SnapCsvBase::load(RecordInputBase &input)
{
    try
    {
        loadDefinition();
        CalcReader reader(input, *_format, _options);
        normaliseColumnNames(&reader);
        attachReader(&reader);

        if (!checkColumns())
        {
            attachReader(0);
            return false;
        }

        if (!_options.useHeader() && _columns.size() > 0)
        {
            reader.setColumnNames(_columns);
        }
        initiallizeLoadData();

        while (reader.readRecord())
        {
            loadRecord();
        }
        terminateLoadData();
        _loadedFilename = input.name();

        attachReader(0);
    }
    catch (RecordError &error)
    {
        attachReader(0);
        runtimeError(error.message());
        return false;
    }
    catch (...)
    {
        attachReader(0);
        throw;
    }
    return true;
}

void SnapCsvBase::initiallizeLoadData()
{
}

void SnapCsvBase::terminateLoadData()
{
}

void SnapCsvBase::definitionError(const std::string &message)
{
    std::string filename = "";
    int lineno = -1;
    if (_defstream)
    {
        filename = _defstream->filename();
        lineno = _defstream->lineNo();
    }
    throw RecordError(message, filename, lineno);
}

void SnapCsvBase::runtimeError(const string &message)
{

    // May want to route this through data file handler to get line number etc
    if (_reader != 0)
    {
        _reader->runtimeError(message);
    }
    else
    {
        throw RecordError(message);
    }
}

void SnapCsvBase::loadValueDefinition(RecordStream &rs, CsvValue &value)
{
    string valuestr;
    std::getline(rs.record(), valuestr);
    loadValueDefinition(valuestr, value);
}

void SnapCsvBase::recordUsedColumn(const std::string &colname)
{
    _usedColumns.push_back(colname);
}

void SnapCsvBase::loadValueDefinition(const std::string &valuestr, CsvValue &value, bool configonly)
{
    ConcatValue *cvalue = &(value.cvalue());
    cvalue->clear();
    RGX::regex re(
        "\\s*(?:"
        "(default)|"                       // Start of default (1)
        "(\\-?\\d+\\.?\\d*)|"              // A number (2)
        "\\\"((?:[^\\\"]|\\\"\\\")*)\\\"|" // Literal string (3)
        "(?:\\$(\\w+))|"                   // Config item (4)
        "(?:\\@(\\w+))|"                   // Column name (5)
        "([a-z_]\\w*)\\(\\s*"              // Start of lookup (6)
        "(?:\\$(\\w+)|\\@(\\w+))"          // Lookup config/column (7)(9)
        "\\s*\\)|"                         // End of lookup
        "(\\S+)"                           // Other random string (9)
        ")(?:\\s|$)",                      // Terminating space or end
        RGX::regex_constants::icase);
    RGX::sregex_iterator imatch(valuestr.begin(), valuestr.end(), re);
    RGX::sregex_iterator end;
    for (; imatch != end; imatch++)
    {
        auto match = *imatch;
        // default ...
        if (match[1].length() > 0)
        {
            ConcatValue *owner = cvalue;
            cvalue = new ConcatValue();
            owner->setDefault(cvalue);
        }
        // Numeric value
        else if (match[2].length() > 0)
        {
            cvalue->add(StringValue(match[2].str()));
        }
        // Literal text
        else if (match[3].length() > 0)
        {
            string text = RGX::regex_replace(match[3].str(), RGX::regex("\\\"\\\""), string("\""));
            cvalue->add(text);
        }
        // Config item
        else if (match[4].length() > 0)
        {
            std::string value = _config.valueOf(match[4].str());
            cvalue->add(value);
        }

        // Column based value.
        else if (match[5].length() > 0)
        {
            std::string colname = match[5].str();
            recordUsedColumn(colname);
            NamedValue col(colname, "");
            cvalue->add(col);
        }
        // Lookup value ..
        else if (match[6].length() > 0)
        {
            std::string lookupname = match[6].str();
            LookupMap &map = lookup(lookupname);
            if (match[7].length() > 0)
            {
                std::string key = match[7].str();
                const string *kval = map.lookup(_config.valueOf(key));
                if (!kval)
                {
                    definitionError(std::string("Value ") + lookupname + "(" + key + ") is not defined for " + value.name());
                }
                else
                {
                    cvalue->add(*kval);
                }
            }
            else
            {
                LookupValue *lv = new LookupValue();
                lv->setMap(map);
                std::string colname = match[8].str();
                recordUsedColumn(colname);
                lv->setSource(new NamedValue(colname, ""));
                cvalue->add(lv);
            }
        }
        // Any other value is treated as literal text (previously it
        // was considered invalid.  Change for forward compatibility of non data value
        // definitions
        else if (match[9].length() > 0)
        {
            cvalue->add(StringValue(match[9]));
        }
    }
    if (!value.cvalue().hasValue())
    {
        definitionError(string("Value not defined for ") + value.name());
    }
}

const string SnapCsvBase::getConfigValue(const std::string &name, RecordStream &rs)
{
    string valuestr;
    getline(rs.record(), valuestr);
    return getConfigValue(name, valuestr);
}

const string SnapCsvBase::getConfigValue(const std::string &name, const string &valuestr)
{
    string value("");
    RGX::regex re(
        "\\s*(?:"
        "(default)|"                       // Start of default (1)
        "(\\-?\\d+\\.?\\d*)|"              // A number (2)
        "\\\"((?:[^\\\"]|\\\"\\\")*)\\\"|" // Literal string (3)
        "(?:\\$(\\w+))|"                   // Config item (4)
        "(?:\\@(\\w+))|"                   // Column name (5)
        "([a-z_]\\w*)\\(\\s*"              // Start of lookup (6)
        "(?:\\$(\\w+)|\\@(\\w+))"          // Lookup config/column (7)(8)
        "\\s*\\)|"                         // End of lookup
        "(\\S+)"                           // Other random string (9)
        ")(?:\\s|$)",                      // Terminating space or end
        RGX::regex_constants::icase);
    RGX::sregex_iterator imatch(valuestr.begin(), valuestr.end(), re);
    RGX::sregex_iterator end;
    for (; imatch != end; imatch++)
    {
        auto match = *imatch;
        string nextpart("");
        // default ...
        if (match[1].length() > 0)
        {
            if (value.length() > 0)
                break;
        }
        // Numeric value
        else if (match[2].length() > 0)
        {
            nextpart = match[2].str();
        }
        // Literal text
        else if (match[3].length() > 0)
        {
            nextpart = RGX::regex_replace(match[3].str(), RGX::regex("\\\"\\\""), string("\""));
        }
        // Config item
        else if (match[4].length() > 0)
        {
            nextpart = _config.valueOf(match[4].str());
        }

        // Column based value.
        else if (match[5].length() > 0)
        {
            string colname = match[5].str();
            definitionError(string("Column value ") + colname + " is not valid in definition of " + colname);
            break;
        }
        // Lookup value ..
        else if (match[6].length() > 0)
        {
            std::string lookupname = match[6].str();
            LookupMap &map = lookup(lookupname);
            if (match[7].length() > 0)
            {
                std::string key = _config.valueOf(match[7].str());
                std::string *keyval = map.lookup(key);
                if (keyval)
                {
                    nextpart = *keyval;
                }
                else
                {
                    definitionError(std::string("Value ") + lookupname + "(" + key + ") is not defined for " + name);
                }
            }
            else
            {
                string colname = match[8].str();
                definitionError(string("Column value ") + colname + " is not valid in definition of " + name);
            }
        }
        // Any other value is treated as literal text
        else if (match[9].length() > 0)
        {
            nextpart = match[9];
        }
        if (nextpart.length() > 0)
        {
            if (value.length() > 0)
                value += " ";
            value += nextpart;
        }
    }
    return value;
}

void SnapCsvBase::loadLookupDefinition(RecordStream &rs, const string &lookupName, bool skip)
{
    LookupMap &map = lookup(lookupName);
    string key;
    string value;
    bool finished = false;
    while (!finished && rs.readRecord())
    {
        rs.record() >> key;
        boost::to_lower(key);
        if (key == "end_lookup")
        {
            finished = true;
        }
        else if (!skip)
        {
            std::getline(rs.record(), value);
            boost::trim(value);
            if (key == "default")
            {
                map.setDefault(value);
            }
            else
            {
                map.add(key, value);
            }
        }
    }
    if (!finished)
    {
        definitionError("Lookup definition not terminated");
    }
}

void SnapCsvBase::loadColumnsDefinition(RecordStream &rs, std::vector<std::string> &cols)
{

    std::string colname;
    while (rs.record() >> colname)
    {
        cols.push_back(colname);
    }
}

static char getChar(const std::string &chardef)
{
    char c = 0;
    if (boost::iequals(chardef, string("tab")))
        c = '\t';
    else if (boost::iequals(chardef, string("space")))
        c = ' ';
    else if (chardef.size() == 1)
        c = chardef[0];
    return c;
}

void SnapCsvBase::loadFormatDefinition(const std::string &formatstr)
{
    RGX::regex re(
        "^\\s*"
        "(?:"
        "(csv)|"                      // CSV format
        "(delimiter\\=whitespace)|"   // Whitespace delimited
        "delimiter\\=(\\S|tab|space)" // Custom delimited parser
        "(?:\\s+quote=(\\S|tab|space))?"
        "(?:\\s+escape=(\\S|tab|space))?"
        ")"
        "(?:\\s+header=(?:y|(n)))?$", // Read headers? Default is Y,
        RGX::regex_constants::icase);
    RGX::smatch match;
    if (RGX::regex_match(formatstr.begin(), formatstr.end(), match, re))
    {
        if (match[1].length() > 0)
        {
            _format.reset(new CsvFormat());
        }
        else if (match[2].length() > 0)
        {
            _format.reset(new WhitespaceFormat());
        }
        else
        {
            char delim = getChar(match[3].str());
            char quote = getChar(match[4].str());
            char escape = getChar(match[5].str());
            _format.reset(new CsvFormat(delim, escape, quote));
        }
        if (match[6].length() > 0)
            _options.setUseHeader(false);
    }
    else
    {
        definitionError(string("Invalid format specification: ") + formatstr);
    }
}

LookupMap &SnapCsvBase::lookup(const std::string &lookupName)
{
    std::string name(lookupName);
    boost::to_lower(name);
    if (!_lookups.count(name))
    {
        _lookups.insert(pair<string, LookupMap>(name, LookupMap(false)));
    }
    return _lookups[name];
}
