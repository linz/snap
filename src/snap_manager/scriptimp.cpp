#include "snapconfig.h"
#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include "ctype.h"
#include "scriptimp.hpp"

using namespace Scripter;
using namespace std;

#define DEBUG_SCRIPTIMP 1

#ifdef DEBUG_SCRIPTIMP
#define LOG(x) printf x ;
#else
#define LOG(x)
#endif

const int maxStackDepth = 32;

Token::~Token()
{
    // Delete linked tokens avoiding recursive call down list of tokens

    Token *del = next;
    while( del )
    {
        Token *delnext = del->next;
        del->next = 0;
        delete del;
        del = delnext;
    }
}
ostream &Token::Print( const wxString &prefix, ostream &str )
{
    for( Token *tok = this; tok != 0; tok = tok->Next() )
    {
        tok->print( prefix, str );
    }
    return str;
}

ostream &Token::PrintSubtoken( Token *subtoken, const wxString &prefix, ostream &str )
{
    wxString subprefix("   ");
    subprefix += prefix;

    for( Token *tok = subtoken; tok != 0; tok = tok->Next() )
    {
        tok->print( subprefix, str );
    }
    return str;
}

int Token::Count()
{
    int i = 0;
    for( Token *t = this; t != 0; t = t->Next() )
    {
        i++;
    }
    return i;
}


ostream &operator <<( ostream &str, Token &token ) { return token.Print( wxString(""), str ); }

Value BooleanToken::evaluate()
{
    Value v;
    v = Value(value);
    LOG(("Evaluated %s as %s\n","BooleanToken",v.AsString().c_str()));
    return v;
}


void BooleanToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "BooleanToken: " << value << endl;
}

Value StringToken::evaluate()
{
    Value v(text);
    LOG(("Evaluated %s as %s\n","StringToken",v.AsString().c_str()));
    return v;
}


void StringToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "StringToken: \"" << text << "\"" << endl;
}


Value IdToken::evaluate()
{
    Value v;
    v = Value(id);
    LOG(("Evaluated %s as %s\n","IdToken",v.AsString().c_str()));
    return v;
}

void IdToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "IdToken: \"" << id << "\"" << endl;
}


Value VariableToken::evaluate()
{
    Value v;
    bool result = Owner()->GetValue( name, v );
    if( ! result )
    {
        Owner()->error("No value defined for variable \"%s\"", name.c_str());
    }
    LOG(("Evaluated %s as %s\n","VariableToken",v.AsString().c_str()));
    return v;
}

void VariableToken::SetValue( const Value &value )
{
    Owner()->SetValue( name, value );
}

bool VariableToken::GetValue( Value &value )
{
    return Owner()->GetValue( name, value );
}

void VariableToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "VariableToken: " << name << endl;
}

FunctionToken::~FunctionToken()
{
    delete params;
}

Value FunctionToken::evaluate()
{
    Value v;
    v = Value(true);
    int nparams = params ? params->Count() : 0;
    Value *paramValues = 0;
    if( nparams > 0 )
    {
        paramValues = new Value[ nparams ];
        int i = 0;
        for( Token *p = params; p; p = p->Next() )
        {
            paramValues[i++] = p->evaluate();
        }
    }

    Owner()->EvaluateFunction( name, nparams, paramValues, v );

    if( paramValues ) delete [] paramValues;

    LOG(("Evaluated %s as %s\n","FunctionToken",v.AsString().c_str()));
    return v;
}


void FunctionToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "FunctionToken: " << name << endl;
    PrintSubtoken( params, prefix, str );
}


ConditionalBranch::~ConditionalBranch()
{
    if( condition ) delete condition;
    if( actions ) delete actions;
}

Value ConditionalBranch::evaluate()
{
    Value v;
    Value vMaxIt;
    // Evaluated by ConditionalStatement token for conditional statement ...

    long maxIterations = 100;
    if( Owner()->GetValue( _T("$max_while_loop_iterations"), vMaxIt ))
    {
        vMaxIt.AsString().ToLong( &maxIterations );
    }
    if( iterate )
    {
        RunPermission rp( Owner(), elLoop );
        while( Owner()->CanRun() && maxIterations-- > 0 && condition->evaluate().AsBool() )
        {
            RunPermission rp1( Owner(), elLoopInner );
            v = actions->evaluate();
        }
    }

    LOG(("Evaluated %s as %s\n","ConditionalBranch",v.AsString().c_str()));
    return v;
}


void ConditionalBranch::print( const wxString &prefix, ostream &str )
{
    str << prefix << "ConditionalBranch: " << (iterate ? "iterated" : "" ) << endl;
    str << prefix << "...condition" << endl;
    PrintSubtoken(condition,prefix,str);
    str << prefix << "...actions" << endl;
    PrintSubtoken(actions,prefix,str);
}

ConditionalStatement::~ConditionalStatement()
{
    delete options;
}

Value ConditionalStatement::evaluate()
{
    Value v;
    for( Token *t = options; t; t = t->Next() )
    {
        ConditionalBranch *branch = static_cast<ConditionalBranch *>(t);
        if( ! branch->Condition() || branch->Condition()->evaluate().AsBool() )
        {
            v = branch->Actions()->evaluate();
            break;
        }
    }
    LOG(("Evaluated %s as %s\n","ConditionalStatement",v.AsString().c_str()));
    return v;
}


void ConditionalStatement::print( const wxString &prefix, ostream &str )
{
    str << prefix << "ConditionalStatment: " << endl;
    PrintSubtoken( options, prefix, str );
}


ForeachToken::~ForeachToken()
{
    if( variable ) delete variable;
    if( string ) delete string;
    if( delimiter ) delete delimiter;
    if( actions ) delete actions;
}

Value ForeachToken::evaluate()
{
    Value v;
    VariableToken *var = static_cast<VariableToken *>( variable );
    wxString str = string->evaluate().AsString();
    if( str.IsEmpty() ) return v;

    wxString del(_T("\n"));
    if( delimiter ) del = delimiter->evaluate().AsString();
    wxRegEx re;
    if( ! re.Compile(del,wxRE_ADVANCED) )
    {
        Owner()->error("Invalid regular expression %s specified",(char *) del.c_str() );
        return v;
    }

    RunPermission rp(Owner(),elLoop);

    int start = 0;

    wxArrayString strings;
    while( rp.CanRun() && re.Matches( str.Mid(start) ) )
    {
        size_t mstart;
        size_t mlen;
        int nmatch;
        re.GetMatch( &mstart, &mlen, 0 );
        nmatch = re.GetMatchCount();
        if( ! isMatch )
        {
            var->SetValue( str.Mid(start,mstart) );
            RunLoop();
        }

        // If no bracketed expressions then return the whole match
        else if( nmatch == 1 )
        {
            var->SetValue( str.Mid(start+mstart, mlen) );
            RunLoop();
        }

        // In either case return explicitly selected groups

        if( nmatch > 1 )
        {
            for( int i = 1; rp.CanRun() && i < nmatch; i++ )
            {
                size_t sstart;
                size_t slen;
                re.GetMatch( &sstart, &slen, i );
                var->SetValue( str.Mid(start+sstart, slen ) );
                {
                    RunLoop();
                }
            }
        }
        if( mstart + mlen == 0 ) break;
        start += mstart + mlen;
    }

    if( rp.CanRun() && ! isMatch )
    {
        var->SetValue( str.Mid(start) );
        RunLoop();
    }

    LOG(("Evaluated %s as %s\n","ForeachToken",v.AsString().c_str()));
    return v;
}

void ForeachToken::RunLoop()
{
    RunPermission rp(Owner(),elLoopInner);
    actions->evaluate();
}

void ForeachToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "ForeachToken: " << (isMatch ? "(matches)" : "(delimited)") << endl;
    str << prefix << "... variable" << endl;
    PrintSubtoken( variable, prefix, str );
    str << prefix << "... string" << endl;
    PrintSubtoken( string, prefix, str );
    str << prefix << "... delimiter" << endl;
    if( delimiter ) PrintSubtoken( delimiter, prefix, str );
    str << prefix << "... actions" << endl;
    PrintSubtoken( actions, prefix, str );
}


AssignmentToken::~AssignmentToken()
{
    delete variable;
    delete value;
}

Value AssignmentToken::evaluate()
{
    Value v;
    v = value->evaluate();
    Owner()->SetValue( static_cast<VariableToken *>(variable)->Name(), v );
    LOG(("Evaluated %s as %s\n","AssignmentToken",v.AsString().c_str()));
    return v;
}


void AssignmentToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "AssignmentToken: " << endl;
    str << prefix << "...variable" << endl;
    PrintSubtoken( variable, prefix, str );
    str << prefix << "...value" << endl;
    PrintSubtoken( value, prefix, str );
}


ExitToken::~ExitToken()
{
    if( expression ) delete( expression );
}

Value ExitToken::evaluate()
{
    Value v;
    v = Value(true);
    if( expression )
    {
        v = expression->evaluate();
    }

    Owner()->SetExitLevel(level);

    LOG(("Evaluated %s as %s\n","ExitToken",v.AsString().c_str()));
    return v;
}


void ExitToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "ExitToken: " << ((int) level) << endl;
    if( expression )
    {
        str << prefix << "... expression" << endl;
        PrintSubtoken( expression, prefix, str );
    }
}

MenuItem::~MenuItem()
{
    if( menu_name_expression ) delete menu_name_expression;
    if( description_expression ) delete description_expression;
    if( requirements ) delete requirements;
    if( actions ) delete actions;
}

MenuItem::MenuItem( ScriptImp *owner, Token *menu_name_expression, Token *description_expression )
    : Token(owner), menu_name_expression(menu_name_expression), description_expression(description_expression)
{
    requirements = 0;
    actions = 0;
    installed = false;
}

Value MenuItem::evaluate()
{
    Value v;
    if(   ! installed )
    {
        // Create a copy of this MenuItem to install, transferring ownership of requirements and
        // actions to it...

        installed = true;

        MenuItem *copy = new MenuItem( Owner(), 0, 0 );
        copy->installed = true;

        copy->requirements = requirements; requirements = 0;
        copy->actions = actions; actions = 0;
        copy->menu_name = menu_name_expression->evaluate().AsString();
        copy->description = description_expression->evaluate().AsString();
        Owner()->AddMenuItem( copy );
    }

    v = Value(true);
    LOG(("Evaluated %s as %s\n","MenuItem",v.AsString().c_str()));
    return v;
}


void MenuItem::print( const wxString &prefix, ostream &str )
{
    str << prefix << "MenuItem: \"" << menu_name << "\" \"" << description << "\"" << endl;
    str << prefix << "...requirements" << endl;
    PrintSubtoken( requirements, prefix, str );
    str << prefix << "...actions" << endl;
    PrintSubtoken( actions, prefix, str );
}

bool MenuItem::IsValid()
{
    Value v(true);
    if( requirements ) v = Owner()->Run(requirements);
    return v.AsBool();
}

void MenuItem::Execute()
{
    if( actions )
    {
        Owner()->Run( actions );
    }
}

FunctionDef::~FunctionDef()
{
    if( parameters ) delete parameters;
    if( actions ) delete actions;
}

Value FunctionDef::evaluate()
{
    Value v;
    bool wasinuse = inuse;
    inuse = true;
    // Strictly should put try ... finally round this ...
    v = actions->evaluate();
    inuse = wasinuse;
    LOG(("Evaluated %s as %s\n","FunctionDef",v.AsString().c_str()));
    return v;
}

void FunctionDef::print( const wxString &prefix, ostream &str )
{
    str << prefix << "FunctionDef: " << name << endl;
    str << prefix << "... parameters" << endl;
    PrintSubtoken( parameters, prefix, str );
    str << prefix << "... actions" << endl;
    PrintSubtoken( actions, prefix, str );
}

StatementBlockToken::~StatementBlockToken()
{
    if( statements ) delete statements;
}

Value StatementBlockToken::evaluate()
{
    Value v;
    RunPermission rp( Owner(), elOk );
    for( Token *s = statements; rp.CanRun() && s != 0; s = s->Next() )
    {
        v = s->evaluate();
    }
    LOG(("Evaluated %s as %s\n","StatementBlockToken",v.AsString().c_str()));
    return v;
}


void StatementBlockToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "StatementBlockToken: " << endl;
    PrintSubtoken( statements, prefix, str );
}

Operator::~Operator()
{
    if( operand1 ) delete operand1;
    if( operand2 ) delete operand2;
}

Value Operator::evaluate()
{
    Value result = operand1->evaluate();

    switch( type )
    {
    case opEq:
        result = Value( result.AsString() == operand2->evaluate().AsString() );
        break;

    case opNe:
        result = Value( result.AsString() != operand2->evaluate().AsString() );
        break;

    case opNot:
        result = Value( ! result.AsBool() );
        break;

    case opAnd:
        if( result.AsBool() ) result = operand2->evaluate();
        break;

    case opOr:
        if( ! result.AsBool() ) result = operand2->evaluate();
        break;

    case opConcat:
        wxString concat = result.AsString();
        concat.append( operand2->evaluate().AsString() );
        result = Value( concat );
        break;
    }
    LOG(("Evaluated %s as %s\n","Operator",result.AsString().c_str()));

    return result;
}

void Operator::print( const wxString &prefix, ostream &str )
{
    bool binary = false;
    str << prefix << "Operator: ";
    switch( type )
    {
    case opNot:
        str << "not";
        break;

    case opNe:
        str << "ne";
        binary = true;
        break;

    case opEq:
        str << "eq";
        binary = true;
        break;

    case opAnd:
        str << "and";
        binary = true;
        break;

    case opOr:
        str << "or";
        binary = true;
        break;

    case opConcat:
        str << "concat";
        binary = true;
        break;
    }
    str << endl << prefix << "... operand1" << endl;
    PrintSubtoken( operand1, prefix, str );
    if( binary )
    {
        str << prefix << "... operand2" << endl;
        PrintSubtoken( operand2, prefix, str );
    }
}

// Class instantiated to check that a process has required run level ...

RunPermission::RunPermission( ScriptImp *owner, ExitLevel level ) : owner(owner), level(level) {}

RunPermission::~RunPermission() { if( owner->exitLevel <= level ) owner->exitLevel = elOk; }

bool RunPermission::CanRun() { return owner->exitLevel == elOk; }

// ==============================================================================
// ScriptImp class

ScriptImp::ScriptImp( EnvBase &environment ) :
    environment( environment)
{
    frame = 0;
    stackDepth = 0;
    exitLevel = elOk;
    program = 0;
    source = 0;
    sourceFile = wxString("");
}

ScriptImp::~ScriptImp()
{
    while( frame ) PopStack();

    for( vector<MenuItem *>::iterator it = menuItems.begin();
            it != menuItems.end();
            it++ )
    {
        delete (*it);
        (*it) = 0;
    }

    for( map<wxString,FunctionDef *>::iterator it = functions.begin();
            it != functions.end();
            it++ )
    {
        delete ( it->second);
        it->second = 0;
    }

}

bool ScriptImp::PushStack()
{

    if( stackDepth >= maxStackDepth )
    {
        error("Maximum stack depth exceeded");
        return false;
    }
    StackFrame *newframe = new StackFrame;
    newframe->next = frame;
    frame = newframe;
    variables = &(frame->variables);
    stackDepth++;
    return true;
}

void ScriptImp::PopStack()
{
    if(  frame )
    {
        stackDepth--;
        StackFrame *oldframe = frame;
        frame = oldframe->next;
        delete oldframe;
        variables = frame ? &(frame->variables) : 0;
    }
}

bool ScriptImp::ExecuteScript( const char *filename  )
{
    bool result = true;

    ifstream scriptFile(filename);
    if( scriptFile.good() )
    {
        source = &scriptFile;
        program = 0;
        sourceFile = wxString(filename);
        int parse_result = parse();
        sourceFile = wxString("");
        Token *prog = program;
        program = 0;
        source = 0;
        scriptFile.close();

        result = (parse_result && (prog != 0));
        if( result )
        {
            Run(prog);
        }
        if( prog ) delete prog;
    }
    else
    {
        error("Cannot open file %s",filename);
        result = false;
    }
    return result;
}

Value ScriptImp::Run( Token *program )
{
    Value result;

    if( PushStack() )
    {
        exitLevel = elOk;
        RunPermission rp(this, elExit);
        errorMessage.empty();
        result = program->evaluate();
        PopStack();
    }
    return result;
}

char ScriptImp::getchar()
{
    // Check what the termination condition is.. assuming char 0 at the moment.

    char ch = 0;
    if( source && source->good() ) source->get(ch);
    return ch;
}

void ScriptImp::error( const char *format, ... )
{
    char *buf;
    buf = new char[1024];
    va_list arglist;
    va_start( arglist, format );
    vsprintf( buf, format, arglist );
    errorMessage = buf;
    delete [] buf;

    if( sourceFile != "" ) errorMessage = errorMessage + "\nFile: " + sourceFile;
    environment.ReportError( errorMessage );
    SetExitLevel( elError );
}

MenuItem *ScriptImp::AddMenuItem( MenuItem *item )
{
	// If the menu item is already defined, then replace it ... 
	for( vector<MenuItem *>::iterator it = menuItems.begin();
        it != menuItems.end();
        it++ )
    {
		if( (*it)->MenuName().Lower() == item->MenuName().Lower() )
		{
			delete (*it);
			(*it) = item;
			return item;
		}
    }
    menuItems.push_back( item );
    return item;
}

MenuItem *ScriptImp::GetMenuItem( int i )
{
    return menuItems[i];
}

bool ScriptImp::AddFunction( FunctionDef *func )
{
    bool result = false;
    wxString name = func->Name();

    // Is the function already defined ... if so then either delete and
    // replace the function if it is not currently executing, or delete
    // the new definition if it is.  If it is not defined, then just add
    // it...

    map<wxString,FunctionDef *>::iterator function_ptr = functions.find(name);
    if( function_ptr != functions.end() )
    {
        FunctionDef *f = function_ptr->second;
        if( f->InUse() )
        {
            delete func;
        }
        else
        {
            delete f;
            function_ptr->second = func;
            result = true;
        }
    }
    else
    {
        functions.insert( pair<wxString,FunctionDef *>(name, func ) );
        result = true;
    }
    return result;
}

Token *ScriptImp::GetVariable(const wxString &name )
{
    if( name == wxString("$script_file"))
    {
        return new StringToken(this,sourceFile);
    }
    else
    {
        return new VariableToken(this,name);
    }
}


Token *ScriptImp::InterpolateString(const wxString &dtext )
{
    // Remove intial and final quote marks

    bool interpolate = dtext[0] == '"';
    wxString text;
    if( dtext[0] == '"' || dtext[0] == '\'' )
    {
        text = dtext.substr(1,dtext.size()-2);
    }
    else
    {
        text = dtext;
    }
    if( ! interpolate || text.Len() == 0 )
    {
        return new StringToken(this,text);
    }
    Token *result = 0;
    wxString value;
    wxString name;

        bool escape = false;
        bool invariable = false;
        int last = text.size();
        for( int i = 0; i <= last; i++ )
        {
            char c = i < last ? text.at(i) : 0;
            if( invariable )
            {
                if( isalnum(c) || c == '_' )
                {
                    name.append(c);
                    continue;
                }
                if( name.Len() > 0 )
                {
                    if( value.Len() > 0 )
                    {
                        Token *next = new StringToken( this, value );
                        value.clear();
                        if( result )
                        {
                            result = new Operator(this,opConcat,result,next);
                        }
                        else
                        {
                            result = next;
                        }
                    }
                    Token *var = this->GetVariable( name );
                    if( result )
                    {
                        result = new Operator(this,opConcat,result,var);
                    }
                    else
                    {
                        result = var;
                    }
                }
                else
                {
                    value.append('$');
                }
                invariable = false;
				if( c == '$' ) continue;
            }

            if( escape && c )
            {
                escape = false;
                switch( c )
                {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                }
				value.append(c);
            }
            else if( c == '\\' )
            {
                escape = true;
                continue;
            }
            else if( c == '$' )
            {
                invariable = true;
                name.clear();
                name.append(c);
                continue;
            }
            else if( c )
            {
                value.append( c );
            }
        }

        if( ! result || value.Len() > 0 )
                   {
                        Token *next = new StringToken( this, value );
                        if( result )
                        {
                            result = new Operator(this,opConcat,result,next);
                        }
                        else
                        {
                            result = next;
                        }
                    }
    return result;
}

void ScriptImp::SetValue( const wxString &name, const Value &value )
{
    Value test;
    if( environment.GetValue(name,test))
    {
        error("Cannot assign a value to system variable %s",name.c_str());
    }
    else if( name.substr(0,2) == wxString("$_" ) )
    {
        global.Add( name, value );
    }
    else
    {
        variables->Add( name, value );
    }
}

bool ScriptImp::GetValue( const wxString &name, Value &value )
{
    value.SetValue( false );
    bool result = environment.GetValue(name, value );
    if( ! result ) result = variables->Get(name, value );
    if( ! result ) result = global.Get(name,value);
    return result;
}


void ScriptImp::EvaluateFunction( const wxString &funcname, int nParams, Value params[], Value &result )
{
    wxString name(funcname);

    FunctionStatus status = fsBadFunction;

    // Is this the execute function function..

    while( name.IsSameAs("ExecuteFunction",false) && status == fsBadFunction )
    {
        if( nParams < 1 )
        {
            status = fsBadParameters;
        }
        else
        {
            name = params[0].AsString();
            nParams--;
            params++;
        }
    }

    // Is this the GetValue function

    if( name.IsSameAs("GetValue",false) )
    {
        if( nParams != 1 )
        {
            status = fsBadParameters;
        }
        else
        {
            GetValue( params[0].AsString(), result );
            status = fsOk;
        }
    }


    // Is the function defined by the environment ...

    if( status == fsBadFunction ) status = environment.EvaluateFunction( name, nParams, params, result );

    // Is this function defined by the script ...

    if( status == fsBadFunction )
    {
        map<wxString,FunctionDef *>::iterator function_ptr = functions.find(name);
        if( function_ptr != functions.end() )
        {
            status = fsOk;

            FunctionDef *f = function_ptr->second;
            if( PushStack() )
            {
                // Setup the parameters ..

                int np = 0;
                for( Token *t = f->Parameters(); t; t = t->Next(), np++ )
                {
                    Value v = np < nParams ? params[np] : Value(false);
                    static_cast<VariableToken *>(t)->SetValue( v );
                }

                RunPermission rp( this, elReturn );
                result = f->evaluate();
                PopStack();
            }
        }
    }

    // Report errors

    if( status != fsOk )
    {
        if( status == fsBadFunction )
        {
            error( "Error: Unknown function %s used in script", name.c_str() );
        }
        else if ( status == fsBadParameters )
        {
            error( "Error: Bad parameters passed to function %s in script",name.c_str() );
        }
        else if( status == fsTerminateScript || status == fsReportedError )
        {
            SetExitLevel(elExit);
        }

    }

    return;
}

int ScriptImp::MenuItemCount()
{
    return menuItems.size();
}


void ScriptImp::Print( ostream &str )
{
    str << "Menu items" << endl;
    for( vector<MenuItem *>::iterator it = menuItems.begin();
            it != menuItems.end();
            it++ )
    {
        (*it)->Print( wxString("   "), str );
    }
    str << "Functions items" << endl;
    for( map<wxString,FunctionDef *>::iterator it = functions.begin();
            it != functions.end();
            it++ )
    {
        it->second->Print( wxString("   "), str );
    }
}
