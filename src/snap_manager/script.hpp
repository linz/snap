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
    Value( const char *values );
    Value( double value );
    Value( const Value &variable, bool valueOnly=false );
    ~Value();
    Value & operator= (const Value &value );
    Value & operator= (const wxString &value ) { SetValue(value); return *this; }
    Value & operator= (const char *value ) { SetValue( wxString(value)); return *this; }
    Value & operator= (bool value) { SetValue(value); return *this; }
    Value & operator= (double value) { SetValue(value); return *this; }
    void SetValue( const wxString &value );
    void SetValue( double );
    void SetValue( bool value );
    wxString AsString(int index=0) const;
    double AsDouble(int index=0) const;
    bool AsBool(int index=0) const;
    int Count() const;
    void SetNext( const Value &value );
    void SetNext( Value *pvalue );
    Value *Next() const;

private:
    bool boolValue;
    double doubleValue;
    wxString stringValue;
    Value *next;
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
    virtual bool AddMenuItem( const wxString &name, const wxString &description, int id ) = 0;
    virtual bool RemoveMenuItem( const wxString &name ) = 0;
    virtual void EnableMenuItem( const wxString &name, bool enabled ) = 0;
    virtual FunctionStatus EvaluateFunction( const wxString &name, const Value *params, Value &result ) = 0;
    virtual void ReportError( const wxString &error ) = 0;
private:
};

class ScriptImp;

class Script
{
public:
    Script( EnvBase &environment );
    ~Script();

    bool ExecuteScript( const char *filename );
    void RunMenuActions( int id );
    void EnableMenuItems();
private:
    ScriptImp *implementation;
};

};



#endif
