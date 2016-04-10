#include "snapconfig.h"
#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include "ctype.h"
#include "scriptimp.hpp"

using namespace Scripter;
using namespace std;

#define DEBUG_SCRIPTIMP 0

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

Value Token::GetValue()
{
    return evaluate();
}

Value Token::GetValueList()
{
    Value v = GetValue();
    for( Token *pt=this->next; pt; pt=pt->next )
    {
        v.SetNext( pt->GetValue() );
    }
    return v;
}

void Token::SetOwnerValue( const Value &value )
{
    Owner()->error("Cannot set variable for token without variable name");
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

void VariableToken::SetOwnerValue( const Value &value )
{
    VariableToken *pvar = this;
    const Value *pval=&value;

    while( pvar->Next() )
    {
        Value v;
        if( pval ) v=Value(*pval,true);
        Owner()->SetValue( pvar->name, v );
        pvar=static_cast<VariableToken *> (pvar->Next());
        if( pval ) pval=pval->Next();
    }
    Owner()->SetValue( pvar->Name(), pval ? *pval : Value() );
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
    Value v(true);
    Value pv;
    if( params ) pv = params->GetValueList();

    Owner()->EvaluateFunction( name, params ? &pv : 0, v );

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
        while( Owner()->CanRun() && maxIterations-- > 0 && condition->GetValue().AsBool() )
        {
            RunPermission rp1( Owner(), elLoopInner );
            v = actions->GetValue();
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
        if( ! branch->Condition() || branch->Condition()->GetValue().AsBool() )
        {
            v = branch->Actions()->GetValue();
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

    wxString del(_T("\n"));
    wxRegEx re;

    // Use a regular expression if the delimiter is explicitely defined, or if there is just
    // one variable..
    
    bool usere = delimiter || ! (string->Next());
    if( delimiter ) del = delimiter->GetValue().AsString();
    if( usere && ! re.Compile(del,wxRE_ADVANCED) )
    {
        Owner()->error("Invalid regular expression %s specified",(char *) del.c_str() );
        return v;
    }

    RunPermission rp(Owner(),elLoop);

    for( Token *strtok=string; strtok; strtok=strtok->Next())
    {
        if( ! usere )
        {
            if( rp.CanRun() ) 
            {
                var->SetOwnerValue( strtok->GetValue() );
                RunLoop();
            }
            continue;
        }

        int start = 0;
        wxString str = strtok->GetValue().AsString();

        while( rp.CanRun() && re.Matches( str.Mid(start) ) )
        {
            size_t mstart;
            size_t mlen;
            int nmatch;
            re.GetMatch( &mstart, &mlen, 0 );
            nmatch = re.GetMatchCount();
            if( ! isMatch )
            {
                var->SetOwnerValue( str.Mid(start,mstart) );
                RunLoop();
            }

            // If no bracketed expressions then return the whole match
            else if( nmatch == 1 )
            {
                var->SetOwnerValue( str.Mid(start+mstart, mlen) );
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
                    var->SetOwnerValue( str.Mid(start+sstart, slen ) );
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
            var->SetOwnerValue( str.Mid(start) );
            RunLoop();
        }
    }

    LOG(("Evaluated %s as %s\n","ForeachToken",v.AsString().c_str()));
    return v;
}

void ForeachToken::RunLoop()
{
    RunPermission rp(Owner(),elLoopInner);
    actions->GetValue();
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
    Value v = value->GetValueList();
    variable->SetOwnerValue( v );
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
        v = expression->GetValueList();
    }

    Owner()->SetExitLevel(level,v);

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

int MenuItem::nextId=1;

MenuItem::Functions::~Functions()
{
    if( requirements ){ delete requirements; }
    if( actions ){ delete actions; }
}

MenuItem::~MenuItem()
{
    if( menu_name_expression ) delete menu_name_expression;
    if( description_expression ) delete description_expression;
    if( functions )
    {
        functions->refcount--;
        if( functions->refcount <= 0 ) delete functions;
    }
}

MenuItem::MenuItem( ScriptImp *owner, Token *menu_name_expression, Token *description_expression )
    : Token(owner), menu_name_expression(menu_name_expression), description_expression(description_expression)
{
    functions=new Functions();
    id=nextId++;
    installed = false;
}

// Create a copy for installation ...

MenuItem::MenuItem( MenuItem &src ) :
    Token( src.Owner() )
{
    menu_name_expression=0;
    description_expression=0;
    id=src.id;
    menu_name = src.menu_name_expression->GetValue().AsString();
    description = src.description_expression->GetValue().AsString();
    functions=src.functions;
    functions->refcount++;
    }

Value MenuItem::evaluate()
{
    Value v;

    // Create a copy of this menu item to be owned by the owner...
    Owner()->AddMenuItem( new MenuItem(*this) );

    v = Value(true);
    LOG(("Evaluated %s as %s\n","MenuItem",v.AsString().c_str()));
    return v;
}


void MenuItem::print( const wxString &prefix, ostream &str )
{
    str << prefix << "MenuItem: \"" << menu_name << "\" \"" << description << "\"" << endl;
    str << prefix << "...requirements" << endl;
    PrintSubtoken( functions->requirements, prefix, str );
    str << prefix << "...actions" << endl;
    PrintSubtoken( functions->actions, prefix, str );
}

bool MenuItem::IsValid()
{
    Value v(true);
    if( functions->requirements ) v = Owner()->Run(functions->requirements);
    return v.AsBool();
}

void MenuItem::Execute()
{
    if( functions->actions )
    {
        Owner()->Run( functions->actions );
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
    v = actions->GetValue();
    // Set the exit value if not already set by a return statement
    // (Note: SetExitLevel does nothing if the exit level is not increased)
    Owner()->SetExitLevel( elReturn, v );
    inuse = wasinuse;
    LOG(("Evaluated %s as %s\n","FunctionDef",v.AsString().c_str()));
    return Owner()->ExitValue();
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
        v = s->GetValue();
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

static int compValue( const Value &value1, const Value &value2 )
{
    wxString s1=value1.AsString();
    wxString s2=value2.AsString();
    double v1=0.0;
    double v2=0.0;
    wxRegEx reNumber(wxT("^(\\-?[0-9]+(\\.[0-9]+)?)(\\.*)"));
    if( reNumber.Matches(s1) )
    {
        double v;
        if( reNumber.GetMatch(s1,1).ToDouble(&v) ) v1=v;
        s1=reNumber.GetMatch(s1,2);
    }
    if( reNumber.Matches(s2) )
    {
        double v;
        if( reNumber.GetMatch(s2,1).ToDouble(&v) ) v2=v;
        s2=reNumber.GetMatch(s2,2);
    }
    if( v1 < v2 ) return -1;
    if( v1 > v2 ) return 1;
    return s1.Cmp(s2);
}

Value Operator::evaluate()
{
    Value result = operand1->GetValue();

    switch( type )
    {
    case opEq:
        result = Value( result.AsString() == operand2->GetValue().AsString() );
        break;

    case opNe:
        result = Value( result.AsString() != operand2->GetValue().AsString() );
        break;

    case opLt:
        result = Value( compValue( result, operand2->GetValue() ) < 0 );
        break;

    case opLe:
        result = Value( compValue( result, operand2->GetValue() ) <= 0 );
        break;

    case opGe:
        result = Value( compValue( result, operand2->GetValue() ) >= 0 );
        break;

    case opGt:
        result = Value( compValue( result, operand2->GetValue() ) > 0 );
        break;


    case opNot:
        result = Value( ! result.AsBool() );
        break;

    case opAnd:
        if( result.AsBool() ) result = operand2->GetValue();
        break;

    case opOr:
        if( ! result.AsBool() ) result = operand2->GetValue();
        break;

    case opConcat:
        {
            wxString concat = result.AsString();
            concat.append( operand2->GetValue().AsString() );
            result = Value( concat );
        }
        break;

    case opPlus:
        {
            double dval = result.AsDouble() + operand2->GetValue().AsDouble();
            result = Value( dval );
        }
        break;

    case opMinus:
        {
            double dval = result.AsDouble() - operand2->GetValue().AsDouble();
            result = Value( dval );
        }
        break;

    case opMultiply:
        {
            double dval = result.AsDouble() * operand2->GetValue().AsDouble();
            result = Value( dval );
        }
        break;

    case opDivide:
        {
            double dval = result.AsDouble();
            double dval2 =  operand2->GetValue().AsDouble();
            if( dval2 == 0.0 )
            {
                Owner()->error("Cannot divide by 0");
                result=Value(0.0);
            }
            else
            {
                result=Value(dval/dval2);
            }
            break;
        }
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

    case opPlus:
        str << "plus";
        binary = true;
        break;

    case opMinus:
        str << "minus";
        binary = true;
        break;

    case opMultiply:
        str << "multiply";
        binary = true;
        break;

    case opDivide:
        str << "divide";
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

bool ScriptImp::PushStack( const wxString &name )
{

    if( stackDepth >= maxStackDepth )
    {
        error("Maximum stack depth exceeded");
        return false;
    }
    StackFrame *newframe = new StackFrame(name);
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
            PostRunActions();
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

void ScriptImp::RunMenuActions( int id )
{
    for( vector<MenuItem *>::iterator it = menuItems.begin(); 
        it !=menuItems.end();
        it++ )
    {
        MenuItem *mi = (*it);
        if( mi->Id() == id )
        {
        mi->Execute();
        PostRunActions();
        break;
        }
    }
}

Value ScriptImp::Run( Token *program )
{
    Value result;

    if( PushStack() )
    {
        exitLevel = elOk;
        RunPermission rp(this, elExit);
        errorMessage.empty();
        result = program->GetValue();
        PopStack();
    }
    return result;
}

void ScriptImp::PostRunActions()
{
    // Post run clean up...
    // Remove deleted menu items

    for( vector<MenuItem *>::iterator it = deleteMenuItems.begin();
    it != deleteMenuItems.end();
    it++ )
    {

			if(*it) delete (*it);
			(*it) = 0;
    }
    deleteMenuItems.clear();
    EnableMenuItems();
}

void ScriptImp::EnableMenuItems()
{
    // Enable/disable menu items

    for( vector<MenuItem *>::iterator it = menuItems.begin();
    it != menuItems.end();
    it++ )
    {
        MenuItem *mi=(*it);
        bool valid=mi->IsValid();
        environment.EnableMenuItem( mi->MenuName(), valid );
    }
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
    for( StackFrame *f = frame; f; f=f->next )
    {
        if( f->name != _T("") )
        {
            errorMessage = errorMessage + "\nIn function: " + f->name;
            break;
        }
    }
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
        MenuItem *mi = (*it);
		if( mi->MenuName() == item->MenuName() )
		{
            item->SetId( mi->Id());
            deleteMenuItems.push_back(mi);
            (*it)=item;
            return item;
		}
    }
    if( environment.AddMenuItem( item->MenuName(), item->Description(), item->Id()))
    {
        menuItems.push_back( item );
    }
    else
    {
        delete( item );
        item=0;
    }
    return item;
}

void ScriptImp::RemoveMenuItem( const wxString &name )
{
    wxString itemname=name;
    wxString match=name;
    wxString delimiter(_T("|"));
    wxArrayString eraseMenus;
    vector<MenuItem *> eraseItems;

    if( ! match.EndsWith(delimiter.c_str())) 
    {
        eraseMenus.Add(match);
        match.Append(delimiter);
    }

	for( vector<MenuItem *>::iterator it = menuItems.begin();
        it != menuItems.end();
        it++ )
    {
        wxString rest;
        MenuItem *mi = (*it);
        bool matched = mi->MenuName() == itemname;
        if( ! matched && mi->MenuName().StartsWith(match.c_str(),&rest) )
        {
            matched=true;
            while( rest.Contains(delimiter) )
            {
                rest=rest.BeforeLast(delimiter[0]);
                wxString menuname=match+rest;
                if( eraseMenus.Index(menuname.c_str()) == wxNOT_FOUND )
                {
                    eraseMenus.Add(menuname);
                }
            }
        }
        if( matched )
        {
            eraseItems.push_back( mi );
        }
    }

	for( vector<MenuItem *>::iterator it = eraseItems.begin();
        it != eraseItems.end();
        it++ )
    {
        MenuItem *mi =(*it);
        if( environment.RemoveMenuItem( mi->MenuName()) )
        {
            for( vector<MenuItem *>::iterator it2 = menuItems.begin();
                it2 != menuItems.end();
                it2++ )
            {
                if( (*it2) == mi )
                {
                    menuItems.erase(it2);
                    deleteMenuItems.push_back(mi);
                    break;
                }
            }
        }
    }

    eraseMenus.Sort(true);
    for( int i=0; i < eraseMenus.Count(); i++ )
    {
        environment.RemoveMenuItem( eraseMenus[i] + _T("|") );
    }
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


void ScriptImp::EvaluateFunction( const wxString &funcname, const Value *params, Value &result )
{
    wxString name(funcname);

    FunctionStatus status = fsBadFunction;

    // Is this the execute function function..

    while( name.IsSameAs("ExecuteFunction",false) && status == fsBadFunction )
    {
        if( ! params  )
        {
            status = fsBadParameters;
        }
        else
        {
            name = params->AsString();
            params = params->Next();
        }
    }

    // Is this the GetValue function

    if( name.IsSameAs("GetValue",false) )
    {
        if( ! params )
        {
            status = fsBadParameters;
        }
        else
        {
            GetValue( params->AsString(), result );
            status = fsOk;
        }
    }

    // Is this the DeleteMenu function

    if( name.IsSameAs("DeleteMenu",false) )
    {
        if( ! params )
        {
            status = fsBadParameters;
        }
        else
        {
            RemoveMenuItem( params->AsString());
            status = fsOk;
        }
    }
    // Is the function defined by the environment ...

    if( status == fsBadFunction ) status = environment.EvaluateFunction( name, params, result );

    // Is this function defined by the script ...

    if( status == fsBadFunction )
    {
        map<wxString,FunctionDef *>::iterator function_ptr = functions.find(name);
        if( function_ptr != functions.end() )
        {
            status = fsOk;

            FunctionDef *f = function_ptr->second;
            if( PushStack(name) )
            {
                // Setup the parameters ..

                if( f->Parameters()) f->Parameters()->SetOwnerValue( params ? *params : Value() );
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
