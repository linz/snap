// #pragma once
#ifndef _LINZ_CALCDLTFILE_HPP
#define _LINZ_CALCDLTFILE_HPP

#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>

#include "delimitedtextfile.hpp"


namespace LINZ
{
namespace DelimitedTextFile
{

using namespace LINZ;

using std::istream;
using std::string;
using std::vector;
using std::map;
using std::pair;
using std::shared_ptr;

class CalcException : public std::runtime_error
{
public:
    CalcException( const string &message ) : runtime_error( message ) {}
};

class BindSource
{
public:
    virtual ~BindSource() = default;
    [[nodiscard]] virtual string *value( const std::string &name ) const = 0;
};

class CalcValue
{
public:
    virtual ~CalcValue() = default;
    [[nodiscard]] virtual CalcValue *clone() const = 0;

    virtual void bind( const BindSource *binder );
    void bind( const BindSource &binder ) { bind( &binder ); }
    virtual void calcValue( string &value ) = 0;
private:
};

class StringValue : public CalcValue
{
public:
    StringValue( const string &value ) : _value(value) {}
    StringValue( const StringValue &fcol );

    [[nodiscard]] StringValue *clone() const override;
    void calcValue( string &value ) override;
private:
    const string _value;
};

class NamedValue : public CalcValue
{
public:
    NamedValue( const string &colname, CalcValue *dfltValue = nullptr ) :
        _colname(colname), _colvalue(nullptr), _default( dfltValue ? dfltValue->clone() : nullptr ) {}
    NamedValue( const string &colname, const std::string &dfltValue ) :
        _colname(colname), _colvalue(nullptr), _default( new StringValue(dfltValue) ) {}
    NamedValue( const NamedValue &ccol );
    [[nodiscard]] NamedValue *clone() const override;

    NamedValue &setDefault( CalcValue *dfltValue );
    NamedValue &setDefault( const CalcValue &dfltValue ) { return setDefault( dfltValue.clone() ); }
    NamedValue &setDefault( const std::string& dfltValue ) { return setDefault( new StringValue(dfltValue)); }
    void bind( const BindSource *binder ) override;
    void calcValue( string &value ) override;
private:
    string _colname;
    const string *_colvalue;
    CalcValue *_default;
    const BindSource *_binder;
};

class LookupMap;

class LookupMapImp : public map<const string, const string>
{
    friend class LookupMap;
private:
    LookupMapImp(bool caseSensitive) :
        _caseSensitive( caseSensitive )
    {}
    void add( const string &key, const string &value );
    void setDefault( const string &dfltstr );
    [[nodiscard]] const string *lookup( const string &key ) const;
    string _default;
    bool _caseSensitive;
};

class LookupMap : public shared_ptr<LookupMapImp>
{

public:
    LookupMap( bool caseSensitive = true ) :
        shared_ptr<LookupMapImp>( new LookupMapImp(caseSensitive) ) {}
    void add( const string &key, const string &value ) { (*this)->add(key,value); }
    void setDefault( const string &dfltstr ) { (*this)->setDefault( dfltstr ); }
    [[nodiscard]] const string *lookup( const string &key ) const { return (*this)->lookup(key); }
};

class LookupValue : public CalcValue
{
public:
    LookupValue();
    LookupValue( const LookupValue &lookup );
    ~LookupValue() override;
    [[nodiscard]] LookupValue *clone() const override;
    LookupMap map() { return _map; }
    LookupValue &setMap( LookupMap map ) { _map = std::move(map); return *this; }
    // Assigning pointer transfers ownership
    LookupValue &setSource( CalcValue *source );
    LookupValue &setSource( const CalcValue &source ) { return setSource( source.clone() ); }
    LookupValue &setSource( const std::string& source ) { return setSource( new NamedValue(source)); }

    void bind( const BindSource *binder ) override;
    void calcValue( string &value ) override;
private:
    CalcValue *_source;
    LookupMap _map;
    const BindSource *_binder;
};

class ConcatValue : public CalcValue
{
public:
    ConcatValue();
    ConcatValue( const ConcatValue &ccol );
    ~ConcatValue() override;
    [[nodiscard]] ConcatValue *clone() const override;
    void clear();
    // Adding a pointer transfers ownership
    ConcatValue &add( CalcValue *column );
    ConcatValue &add( const CalcValue &column ) { return add( column.clone()); }
    ConcatValue &add( const std::string &value ) { return add(new StringValue(value)); }
    ConcatValue &operator<< ( CalcValue &column ) { return add(column); }
    ConcatValue &operator<< ( const std::string &value ) { return add(new StringValue(value)); }

    // Using a pointer transfers ownership
    ConcatValue &setDefault( CalcValue *dfltValue );
    ConcatValue &setDefault( const CalcValue &dfltValue ) { return setDefault( dfltValue.clone() ); }
    ConcatValue &setDefault( const std::string& dfltValue ) { return setDefault( new StringValue(dfltValue)); }

    bool hasValue() { return !_components.empty(); }

    void bind( const BindSource *binder ) override;
    void calcValue( string &value ) override;
private:
    vector<CalcValue *> _components;
    CalcValue *_default;
    const BindSource *_binder;
};

class CalcReader : public Reader
{
    class CalcReaderColumn : public pair<CalcValue *, Column *>
    {
        friend class CalcReader;
    public:
        CalcReaderColumn(CalcValue *ccol, Column *col) : pair<CalcValue *, Column *>( ccol, col ) {}
        ~CalcReaderColumn() { delete first; delete second; }
        [[nodiscard]] const string &name() const { return second->name(); }
        [[nodiscard]] string &value() const { return second->value(); }
        template<typename T> void operator>>( T &value ) { (*second) >> value; }
    };

    class BindSource : public DelimitedTextFile::BindSource
    {
    public:
        BindSource( const Reader &reader ) : _reader(reader) {}
        [[nodiscard]] string *value( const std::string &name ) const override;
    private:
        const Reader &_reader;
    };

public:
    CalcReader( const string& filename, const Format &parser = Format::Csv, const Options &options = Options() )
        : Reader( filename, parser, options ) {}
    CalcReader( istream &is, const Format &parser = Format::Csv, const Options &options = Options() )
        : Reader( is, parser, options ) {}
    CalcReader( RecordInputBase &input, const Format &parser = Format::Csv, const Options &options = Options() )
        : Reader( input, parser, options ) {}

    Column &addColumn( const std::string &name, const CalcValue &column );
    bool readRecord() override;
    [[nodiscard]] Column *findColumn( const std::string &name ) const override;
private:
    vector<shared_ptr<CalcReaderColumn> > _columns;
};

} // End of DelimitedTextFile namespace
} // End of LINZ namespace

#endif
