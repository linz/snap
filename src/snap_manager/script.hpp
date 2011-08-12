#ifndef SCRIPT_HPP
#define SCRIPT_HPP

#include "wx_includes.hpp"
#include <map>

namespace Scripter
{

using namespace std;

class Value
{
public:
    Value();
    Value( wxString value );
    Value( bool value);
    Value( const char *value );
    Value( const Value &variable );
    ~Value();
    Value & operator= (const Value &value );
    Value & operator= (const wxString &value ) { SetValue(value); }
    Value & operator= (const char *value ) { SetValue( wxString(_T(value))); }
    Value & operator = (bool value) { SetValue(value); }
    void SetValue( const wxString &value );
    void SetValue( bool value );
    wxString &AsString();
    bool AsBool();

private:
    bool boolValue;
    wxString stringValue;
};

class VariableList : public map<wxString,Value>
{
public:
    VariableList();
    ~VariableList();
    void Add( wxString name, Value var );
    bool Get( wxString name, Value &var );
private:
};


// Abstract base class for a script running environment..

enum FunctionStatus
{
    fsOk,                  // All ok
    fsTerminateScript,     // All ok but exit script
    fsReportedError,       // Error already reported - exit script
    fsBadFunction,         // Unreported error ... function not known
    fsBadParameters        // Unreported error ... bad parameters in function call
};

class EnvBase
{
public:
    EnvBase() {}
    virtual ~EnvBase() {}
    virtual bool GetValue( const wxString &name, Value &value ) = 0;
    virtual FunctionStatus EvaluateFunction( const wxString &name, int nParams, Value params[], Value &result ) = 0;
    virtual void ReportError( const wxString &error ) = 0;
private:
};

class ScriptImp;

struct MenuDef
{
    const char *menu_name;
    const char *description;
};

class Script
{
public:
    Script( EnvBase &environment );
    ~Script();

    bool ExecuteScript( const char *filename );
    int GetMenuItemCount();
    bool GetMenuDefinition( int i, MenuDef &def );
    bool MenuIsValid( int i );
    void RunMenuActions( int i );
private:
    ScriptImp *implementation;
};

};



#endif