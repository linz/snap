#include "snapconfig.h"
#include "script.hpp"
#include "scriptimp.hpp"

using namespace Scripter;
using namespace std;

// Script variable class

Value::Value() : next(0)
{
    SetValue(false);
}

Value::Value( wxString value ) : next(0)
{
    SetValue( value );
}

Value::Value( bool value) : next(0)
{
    SetValue( value );
}

Value::Value( double value) : next(0)
{
    SetValue( value );
}

Value::Value( const char *value ) : next(0)
{
    SetValue( wxString(value) );
}

void Value::SetValue( bool value )
{
    if( value ) { stringValue = wxString("true"); }
    else { stringValue = wxString(""); }
    doubleValue = value ? 1.0 : 0.0;
    boolValue = value;
}

void Value::SetValue( const wxString &value )
{
    stringValue = value;
    if( ! value.ToDouble( &doubleValue ) ) doubleValue=0.0;
    boolValue = value != "";
}

void Value::SetValue( double value )
{
    doubleValue = value;
    stringValue = "";
    stringValue << value;
    boolValue = value != 0.0;
}

Value::Value( const Value &value, bool valueOnly )
{
    doubleValue = value.doubleValue;
    stringValue = value.stringValue;
    boolValue = value.boolValue;
    next = 0;
    if( ! valueOnly && value.Next() ) SetNext(value.Next());
}

void Value::SetNext( Value *v )
{
    SetNext( *v );
}

void Value::SetNext( const Value &v )
{
    Value *np=this;
    while( np->next ) np=np->next;
    Value *nv = new Value(v);
    np->next = nv;
}

Value *Value::Next() const
{
    return next;
}

Value::~Value()
{
    if( next )
    {
        Value *v=next;
        next=0;
        delete v;
    }
}

Value &Value::operator = ( const Value &value )
{
    if( &value != this )
    {
        boolValue = value.boolValue;
        doubleValue = value.doubleValue;
        stringValue = value.stringValue;
        if( next ) { Value *v=next; next=0; delete( v ); }
        if( value.Next() ) SetNext(value.Next());
    }
    return *this;
}

wxString Value::AsString(int index) const
{
    const Value *vp=this;
    while( index-- && vp ) vp=vp->next;
    return vp ? vp->stringValue : wxString();
}

bool Value::AsBool(int index) const
{
    const Value *vp=this;
    while( index-- && vp ) vp=vp->next;
    return vp ? vp->boolValue : false;
}

double Value::AsDouble(int index) const
{
    const Value *vp=this;
    while( index-- && vp ) vp=vp->next;
    return vp ? vp->doubleValue :  0.0;
}

int Value::Count() const
{
    const Value *vp=this;
    int count=1;
    while( vp->next ){ vp=vp->next; count++; }
    return count;
}

//////////////////////////////////////////////////////////////////


VariableList::VariableList()
{
}

VariableList::~VariableList()
{
}

void VariableList::Add( wxString name, Value var )
{
    wxString lname = name.Lower();
    iterator it = find(lname);
    if( it != end() )
    {
        (*it).second = var;
    }
    else
    {
        insert( pair<wxString,Value>(lname,var) );
    }
}

bool VariableList::Get( wxString name, Value &var )
{
    bool result = false;
    iterator it = find(name.Lower());
    if( it != end() )
    {
        result = true;
        var = (*it).second;
    }
    return result;
}

//////////////////////////////////////////////////////////////////
// Script class .. interface to ScriptImp .. implementation class


Script::Script( EnvBase &environment )
{
    implementation = new ScriptImp( environment );
}

Script::~Script()
{
    delete implementation;
}

bool Script::ExecuteScript( const char *filename )
{
    return implementation->ExecuteScript( filename );
}

void Script::RunMenuActions( int id )
{
    implementation->RunMenuActions( id );
}

void Script::EnableMenuItems()
{
    implementation->EnableMenuItems();
}
