#include "snapconfig.h"
#include "script.hpp"
#include "scriptimp.hpp"

using namespace Scripter;
using namespace std;

// Script variable class

Value::Value()
{
    SetValue(false);
}

Value::Value( wxString value )
{
    SetValue( value );
}

Value::Value( bool value)
{
    SetValue( value );
}

Value::Value( const char *value )
{
    SetValue( wxString(value) );
}

void Value::SetValue( bool value )
{
    if( value ) { stringValue = wxString("true"); }
    else { stringValue = wxString(""); }
    boolValue = value;
}

void Value::SetValue( const wxString &value )
{
    stringValue = value;
    boolValue = value != "";
}

Value::Value( const Value &value )
{
    stringValue = value.stringValue;
    boolValue = value.boolValue;
}

Value::~Value()
{
}

Value &Value::operator = ( const Value &value )
{
    boolValue = value.boolValue;
    stringValue = value.stringValue;
    return *this;
}

wxString &Value::AsString()
{
    return stringValue;
}

bool Value::AsBool()
{
    return boolValue;
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
