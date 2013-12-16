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

Value::Value( const Value &value )
{
    doubleValue = value.doubleValue;
    stringValue = value.stringValue;
    boolValue = value.boolValue;
    SetNext(next);
}

void Value::SetNext( Value *v )
{
    Value *nv = new Value(*v);
    next = nv;
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
    boolValue = value.boolValue;
    doubleValue = value.doubleValue;
    stringValue = value.stringValue;
    return *this;
}

const wxString &Value::AsString() const
{
    return stringValue;
}

bool Value::AsBool() const
{
    return boolValue;
}

double Value::AsDouble() const
{
    return doubleValue;
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

int Script::GetMenuItemCount()
{
    return implementation->MenuItemCount();
}

bool Script::GetMenuDefinition( int i, MenuDef &def )
{
    MenuItem *mi = implementation->GetMenuItem( i );
    bool result = mi != 0;
    if( result )
    {
        def.menu_name = mi->MenuName().c_str();
        def.description = mi->Description().c_str();
    }
    return result;
}

bool Script::MenuIsValid( int i )
{
    MenuItem *mi = implementation->GetMenuItem( i );
    bool result = false;
    if( mi )
    {
        result = mi->IsValid();
    }
    return result;
}

void Script::RunMenuActions( int i )
{
    MenuItem *mi = implementation->GetMenuItem( i );
    if( mi ) mi->Execute();
}
