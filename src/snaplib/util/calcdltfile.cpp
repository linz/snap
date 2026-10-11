#include <boost/algorithm/string.hpp>
#include "calcdltfile.hpp"

namespace LINZ
{
namespace DelimitedTextFile
{

///////////////////////////////////////////////////////////////
// CalcValue

void CalcValue::bind( const BindSource * )
{
}

///////////////////////////////////////////////////////////////
// StringValue

StringValue::StringValue( const StringValue &fcol ) :
    _value( fcol._value )
{
}

StringValue *StringValue::clone() const
{
    return new StringValue( *this );
}

void StringValue::calcValue( string &value )
{
    value = _value;
}


///////////////////////////////////////////////////////////////
// NamedValue

NamedValue::NamedValue( const NamedValue &ccol ) :
   _default(nullptr), _binder(nullptr)
{
    _colname = ccol._colname;
    _colvalue = nullptr;
}

NamedValue *NamedValue::clone() const
{
    NamedValue *value = new NamedValue( *this );
    if( _default ) value->setDefault( _default->clone() );
    return value;
}

NamedValue &NamedValue::setDefault(  CalcValue *dfltValue )
{
    if( _default )
    {
        delete _default;
        _default = nullptr;
    }
    if( dfltValue )
    {
        _default = dfltValue;
        if( _binder ) _default->bind( _binder );
    }
    return *this;
}
void NamedValue::bind( const BindSource *binder )
{
    _colvalue = nullptr;
    if( binder )
    {
        _colvalue = binder->value( _colname );
    }
    if( _default ) _default->bind( binder );
    _binder = binder;
}

void NamedValue::calcValue( string &value )
{
    if( _colvalue )
    {
        value = *_colvalue;
    }
    else if( _default )
    {
        _default->calcValue( value );
    }
    else
    {
        std::ostringstream os;
        os << "Value of " << _colname << " is not defined";
        throw CalcException( os.str() );
    }
}

///////////////////////////////////////////////////////////////
// LookupMap

void LookupMapImp::add( const string &key, const string &value )
{
    if( _caseSensitive )
    {
        insert( pair<const string,const string>(key, value ));
    }
    else
    {
        string keyval = key;
        boost::to_lower( keyval );
        insert( pair<const string,const string>(keyval, value ));
    }
}

void LookupMapImp::setDefault( const string &dfltstr )
{
    _default = dfltstr;
}

const string *LookupMapImp::lookup( const string &key ) const
{
    string keyval = key;
    if( ! _caseSensitive ){ boost::to_lower( keyval ); }
    map<const string, const string>::const_iterator match = find(keyval);
    if( match != end() ){ return &(match->second); }
    if( _default.size()) return &(_default);
    return nullptr;
}

///////////////////////////////////////////////////////////////
// LookupValue

LookupValue::LookupValue() :
        _source(nullptr),
        _binder(nullptr)
{
}

LookupValue::LookupValue( const LookupValue &lcol )
{
    _binder = nullptr;
    _map = lcol._map;
    if( lcol._source ) _source = lcol._source->clone();
    else _source = nullptr;
}

LookupValue::~LookupValue()
{
    setSource(nullptr);
}

LookupValue *LookupValue::clone() const
{
    return new LookupValue( *this );
}

LookupValue &LookupValue::setSource( CalcValue *source )
{
    if( _source )
    {
        delete _source;
        _source = nullptr;
    }
    if( source )
    {
        _source = source;
        if( _binder ) _source->bind( _binder );
    }
    return *this;
}


void LookupValue::bind( const BindSource *binder )
{
    _binder = binder;
    if( _source ) _source->bind(binder);
}

void LookupValue::calcValue( string &value )
{
    value.clear();
    string svalue("");
    if( _source ) _source->calcValue(svalue);

    const string *match = _map.lookup( svalue );
    if( match )
    {
        value = *match;
    }
     else
    {
        std::ostringstream os;
        os << "Lookup key \"" << svalue << "\" is not defined";
        throw CalcException( os.str() );
    }
}

///////////////////////////////////////////////////////////////
// ConcatValue

ConcatValue::ConcatValue() : 
    _default(nullptr),
    _binder(nullptr)
{
}

ConcatValue::ConcatValue( const ConcatValue &ccol ) :
    _default(nullptr),
    _binder(nullptr)

{
    for( vector<CalcValue *>::const_iterator c = ccol._components.begin(); c < ccol._components.end(); c++ )
    {
        _components.push_back( (*c)->clone());
    }
    if( ccol._default ) _default = ccol._default->clone();
}

ConcatValue::~ConcatValue()
{
    clear();
}

ConcatValue *ConcatValue::clone() const
{
    ConcatValue *value = new ConcatValue( *this );
    return value;
}

void ConcatValue::clear()
{
    for( vector<CalcValue *>::const_iterator c = _components.begin(); c < _components.end(); c++ )
    {
        delete *c;
    }
    _components.clear();
    setDefault(nullptr);
}

ConcatValue &ConcatValue::setDefault( CalcValue *dfltValue )
{
    if( _default )
    {
        delete _default;
        _default = nullptr;
    }
    if( dfltValue )
    {
        _default = dfltValue;
        if( _binder ) _default->bind( _binder );
    }
    return *this;
}

ConcatValue &ConcatValue::add( CalcValue *column )
{
    if( _binder ) column->bind( _binder );
    _components.push_back(column);
    return *this;
}

void ConcatValue::bind( const BindSource *binder )
{
    _binder = binder;
    for( vector<CalcValue *>::const_iterator c = _components.begin(); c < _components.end(); c++ )
    {
        (*c)->bind(binder);
    }
    if( _default ) _default->bind(binder);
}

void ConcatValue::calcValue( string &value )
{
    value.clear();
    string component;
    for( vector<CalcValue *>::const_iterator c = _components.begin(); c < _components.end(); c++ )
    {
        (*c)->calcValue(component);
        value += component;
    }
    boost::trim_right( value );
    if( _default && value.size() == 0 )
    {
        _default->calcValue(value);
    }
}


///////////////////////////////////////////////////////////////
// CalcReader

std::string *CalcReader::BindSource::value( const std::string &name ) const
{
    Column *c = _reader.findColumn(name);
    if( c ) return &(c->value());
    return nullptr;
}

Column & CalcReader::addColumn( const std::string &name, const CalcValue &column )
{
    CalcValue *ccol = column.clone();
    ccol->bind( BindSource(*this) );
    Column *col = newColumn( name );
    _columns.push_back( shared_ptr<CalcReaderColumn>(new CalcReaderColumn(ccol,col)));
    return *col;
}

bool CalcReader::readRecord()
{
    if( ! Reader::readRecord()) return false;
    for( auto c = _columns.begin(); c < _columns.end(); c++ )
    {
        try
        {
            (*c)->first->calcValue( (*c)->second->value());
        }
        catch( CalcException &error )
        {
            std::ostringstream os;
            os << error.what() << " in column " << (*c)->second->name();
            runtimeError( os.str() );
        }
    }
    return true;
}

Column *CalcReader::findColumn( const std::string &name ) const
{
    for( auto c = _columns.begin(); c < _columns.end(); c++ )
    {
        if( namesMatch(name,(*c)->name())) return (*c)->second;
    }
    return Reader::findColumn( name );
}

} // End of namespace DelimitedTextFile
} // End of namespace LINZ
