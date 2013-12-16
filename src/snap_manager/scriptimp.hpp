#ifndef SCRIPTIMP_HPP
#define SCRIPTIMP_HPP

#include <list>
#include <map>
#include <vector>


#include <iostream>

#include "script.hpp"

namespace Scripter
{

enum ExitLevel
{
    elOk,
    elLoopInner,
    elLoop,
    elReturn,
    elExit,
    elError
};

class ScriptImp;

class Token
{
public:
    Token( ScriptImp *owner) : owner(owner), next(0) {}
    virtual ~Token();

    Value GetValue();

    Token *SetNext( Token *n ) { Token *t = this; while( t->next ) { t = t->next; }; t->next = n; return this; }
    Token *Next() { return next; }
    // Returns a count of tokens in the linked list from this token ..
    int Count();

    ScriptImp *Owner() { return owner; }
    virtual void SetOwnerValue( const Value &value ); 
    ostream &Print( const wxString &prefix, ostream &str );
    ostream &PrintSubtoken( Token *subtoken, const wxString &prefix, ostream &str );

protected:
    virtual Value evaluate() = 0;
    virtual void print( const wxString &prefix, ostream &str ) = 0;

private:
    ScriptImp *owner;
    Token *next;
};

ostream &operator <<( ostream &str, Token &token );
class BooleanToken : public Token
{
public:
    BooleanToken( ScriptImp *owner, bool value ) : Token(owner), value(value) {};
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
    bool value;
};

class StringToken : public Token
{
public:
    StringToken( ScriptImp *owner, const wxString &text ) : Token(owner), text(text) {};
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
    wxString text;
};

class IdToken : public Token
{
public:
    IdToken( ScriptImp *owner, wxString id ) : Token(owner), id(id) {}
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
    wxString id;
};

class VariableToken : public Token
{
public:
    VariableToken( ScriptImp *owner, wxString name ) : Token(owner), name(name) {}
    bool GetValue( Value &v );
    wxString &Name() { return name; }
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
    virtual void SetOwnerValue( const Value &value ); 
private:
    wxString name;
};


class FunctionToken : public Token
{
public:
    FunctionToken( ScriptImp *owner, wxString name, Token *params = 0 ) : Token( owner ), name(name), params(params) {}
    virtual ~FunctionToken();
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
    virtual void SetParameters( Token *params ) { this->params = params; }
private:
    wxString name;
    Token *params;
};

class ConditionalBranch : public Token
{
public:
    ConditionalBranch( ScriptImp *owner, Token *condition, Token *actions, bool iterate = false )
        : Token( owner ), condition(condition), actions(actions), iterate(iterate)
    {}
    virtual ~ConditionalBranch();
    void SetActions( Token *actions ) { this->actions = actions; }
    Token *Condition() { return condition; }
    Token *Actions() { return actions; }
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
private:
    Token *condition;
    Token *actions;
    bool iterate;
};

class ConditionalStatement : public Token
{
public:
    ConditionalStatement( ScriptImp *owner, Token *options ) : Token(owner), options(options) {}
    virtual ~ConditionalStatement();
    void AddOptions( Token *newOptions ) { if( options ) { options->SetNext(newOptions); } else { options = newOptions; };}
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
private:
    Token *options;
};


class ForeachToken : public Token
{
public:
    ForeachToken( ScriptImp *owner, Token *variable, Token *string, Token *delimiter, Token *actions, bool isMatch = false )
        : Token( owner ), variable(variable), string(string), delimiter(delimiter), actions(actions), isMatch(isMatch)
    {}
    virtual ~ForeachToken();
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
private:
    void RunLoop();
    Token *variable;
    Token *string;
    Token *delimiter;
    Token *actions;
    bool isMatch;
};


class AssignmentToken : public Token
{
public:
    AssignmentToken( ScriptImp *owner, Token *variable, Token *value ) : Token(owner), variable(variable), value(value) {};
    virtual ~AssignmentToken();
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
private:
    Token *variable;
    Token *value;
};

class ExitToken : public Token
{
public:
    ExitToken( ScriptImp *owner, ExitLevel level, Token *expression = 0 )
        : Token(owner), level(level), expression(expression) {}
    virtual ~ExitToken();
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
private:
    ExitLevel level;
    Token *expression;
};

class StatementBlockToken : public Token
{
public:
    StatementBlockToken( ScriptImp *owner, Token *statements = 0 ) : Token(owner), statements(statements) {};
    virtual ~StatementBlockToken();
    void AddStatements( Token *newStatements )
    {
        if( statements ) { statements->SetNext( newStatements ); }
        else { statements = newStatements; }
    }
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
private:
    Token *statements;
};

enum OperatorType
{
    opEq,
    opNe,
    opNot,
    opAnd,
    opOr,
    opConcat,
    opPlus,
    opMinus,
    opMultiply,
    opDivide
};

class Operator : public Token
{
public:
    Operator( ScriptImp *owner, OperatorType type, Token *operand1, Token *operand2 = 0 )
        : Token( owner ), type(type), operand1(operand1), operand2(operand2) {}
    virtual ~Operator();
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
private:
    OperatorType type;
    Token *operand1;
    Token *operand2;
};

class DialogToken : public Token
{
public:
    DialogToken( ScriptImp *owner, Token *label, Token *controls, Token *variable, Token *options )
        : Token(owner), label(label), controls(controls), variable( static_cast<VariableToken *>(variable)), options(options) {};
    virtual ~DialogToken();
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
private:
    Token *label;
    Token *controls;
    Token *options;
    VariableToken *variable;
};

enum DialogControlTokenType
{
    ctLabel,
    ctTextBox,
    ctListBox,
    ctCheckBox,
    ctRadioSelector,
    ctDropDownSelector,
    ctOpenFileSelector,
    ctSaveFileSelector,
    ctButton,
    ctValidator,
    ctSpacer,
    ctNewColumn
};

class DialogControlToken : public Token
{
public:
    DialogControlToken( ScriptImp *owner, DialogControlTokenType type,
                        Token *label = 0, Token *variable = 0, Token *selector = 0, Token *info = 0 )
        : Token(owner),
          type(type),
          label(label),
          variable( static_cast<VariableToken *>(variable)),
          selector(selector),
          info(info)
    {};
    virtual ~DialogControlToken();
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
    DialogControlTokenType Type() { return type; }
    VariableToken *Variable() { return variable; }
    Token *Label() { return label; }
    Token *Selector() { return selector; }
    Token *Info() { return (type == ctButton || type == ctValidator) ? 0 : info; }
    Token *Actions() { return (type == ctButton || type == ctValidator) ? info : 0; }
private:
    DialogControlTokenType type;
    VariableToken *variable;
    Token *label;
    Token *selector;
    Token *info;
};

class MenuItem : public Token
{
public:
    MenuItem( ScriptImp *owner, Token *menu_name_expression, Token *description_expression );
    virtual ~MenuItem();
    MenuItem *AddRequirements( Token *requirements ) { this->requirements = requirements; return this; }
    MenuItem *AddActions( Token *actions ) { this->actions = actions; return this; }
    bool IsValid();
    void Execute();
    const wxString &MenuName() { return menu_name; }
    const wxString &Description() { return description; }
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
private:
    Token *menu_name_expression;
    Token *description_expression;
    Token *requirements;
    Token *actions;
    wxString menu_name;
    wxString description;
    bool installed;
};


class FunctionDef : public Token
{
public:
    FunctionDef( ScriptImp *owner, wxString name, Token *parameters, Token *actions ) :
        Token(owner), name(name), parameters(parameters), actions(actions), inuse(false) {}
    virtual ~FunctionDef();
    virtual Value evaluate();
    virtual void print( const wxString &prefix, ostream &str );
    Token *Parameters() { return parameters; }
    wxString &Name() { return name; }
    bool InUse() { return inuse; }
private:
    wxString name;
    Token *parameters;
    Token *actions;
    bool inuse;
};

class StackFrame
{
public:
    StackFrame( const wxString &name) :  name(name), variables(), next(0) {}
    StackFrame( ) :  name(_T("")), variables(), next(0) {}
    wxString name;
    VariableList variables;
    StackFrame *next;
};

class RunPermission
{
public:
    RunPermission( ScriptImp *owner, ExitLevel level );
    ~RunPermission();
    bool CanRun();
private:
    ScriptImp *owner;
    ExitLevel level;
};

class ScriptImp
{
public:
    friend class RunPermission;

    ScriptImp( EnvBase &environment );
    ~ScriptImp();

    // Functions for loading menu items and programs
    bool ExecuteScript( const char *filename );
    Value Run(Token *program);

    // Functions for accessing menu items

    int MenuItemCount();
    MenuItem *GetMenuItem( int i );

    // Functions used by Tokens

    MenuItem *AddMenuItem( MenuItem *item );
    bool AddFunction( FunctionDef *func );

    Token *GetVariable( const wxString &name );
    Token *InterpolateString( const wxString &text );
    void SetValue( const wxString &name, const Value &value );
    bool GetValue( const wxString &name, Value &value );
    void EvaluateFunction( const wxString &name, const Value *params, Value &result );
    ExitLevel GetExitLevel() { return exitLevel; }
    void SetExitLevel( ExitLevel level ) { if( level > exitLevel ) exitLevel = level; }
    bool CanRun() { return exitLevel == elOk; }

    // Program generated and used by LAPG grammar processor ..
    int parse();
    void error( const char *format, ... );
    char getchar();

    // Debug print of contents...
    void Print( ostream &os );

private:
    bool PushStack( const wxString &name = _T("") );
    void PopStack();

    StackFrame *frame;
    int stackDepth;
    VariableList *variables;
    VariableList global;

    bool inConfig;
    ExitLevel exitLevel;
    wxString errorMessage;

    EnvBase &environment;

    // Keep a list of allocated tokens to
    vector<MenuItem *> menuItems;
    map< wxString, FunctionDef *> functions;

    // Stuff for parsing
    wxString sourceFile;
    istream *source;
    Token *program;

};

}

#endif
