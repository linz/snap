#include "snapconfig.h"
#include "wx_includes.hpp"
#include "wxsimpledialog.hpp"

#include "scriptimp.hpp"

using namespace Scripter;

class ScriptDialogControl
{
public:
    DialogControlToken *token;
    wxWindow *control;
    Value initValue;
    bool boolVal;
    int intVal;
    wxString stringVal;
    wxString selector;
    ScriptDialogControl() :
        token(0),
        control(0),
        boolVal(false),
        intVal(0),
        stringVal(""),
        selector("")
    {}
};

class ScriptDialog : public wxSimpleDialog
{
public:
    ScriptDialog( wxString label, Token *controls, long buttons );
    ~ScriptDialog();
    void ResetInitialValues();

protected:
    virtual void ReadData();
    virtual void Apply();

protected:
    bool Validate();

private:
    void AddControls();
    wxString GetSelectorString( wxString selectorDefe );
    int GetSelectedIndex( wxString selectorDef, wxString selected );
    wxString GetSelectedValue( wxString selectorDef, int selected );
    void HandleButton( wxEvent &event );
    bool RunDialogProgram( Token *program, bool isValidation );
    int nControls;
    ScriptDialogControl *controls;
    Value *initValues;

};

ScriptDialog::ScriptDialog( wxString label, Token *controllist, long buttons )
    : wxSimpleDialog( label, buttons )
{
    controls = 0;
    nControls = controllist->Count();
    if( nControls > 0 )
    {
        controls = new ScriptDialogControl[nControls];

        DialogControlToken *ctk = static_cast<DialogControlToken *>(controllist);
        for( int i = 0; i < nControls; i++ )
        {
            ScriptDialogControl &ctrl = controls[i];
            ctrl.token = ctk;
            if( ctk->Variable() )
            {
                ctk->Variable()->GetValue( ctrl.initValue );
            }
            ctk = static_cast<DialogControlToken *>(ctk->Next());
        }
    }

    AddControls();
    AddButtonsAndSize();
}

ScriptDialog::~ScriptDialog()
{
    if( nControls > 0 )
    {
        delete [] controls;
    }
}

void ScriptDialog::ResetInitialValues()
{
    for( int i = 0; i < nControls; i++ )
    {
        if( controls[i].token->Variable() )
        {
            controls[i].token->Variable()->SetOwnerValue( controls[i].initValue );
        }
    }
}

void ScriptDialog::AddControls()
{
    wxString label;
    wxString info;
    wxString selector;
    wxBoxSizer *rows = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *columns = new wxBoxSizer( wxHORIZONTAL );
    wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
    rows->Add(columns);
    columns->Add(sizer);

    wxSizerFlags sizerFlags;

    sizerFlags.Left();
    // sizerFlags.Expand();
    sizerFlags.Border( wxLEFT | wxRIGHT );

    for( int i = 0; i < nControls; i++ )
    {

        ScriptDialogControl &control = controls[i];
        DialogControlToken &ctk = *(control.token);

        wxString selectorString;

        selector.Clear();
        info.Clear();
        label.Clear();

        if( ctk.Label() )
        {
            label = wxString(  ctk.Label()->GetValue().AsString() );
        }

        if( ctk.Selector() )
        {
            control.selector = ctk.Selector()->GetValue().AsString();
            selector = control.selector;
        }

        if( ctk.Info() )
        {
            info = wxString(  ctk.Info()->GetValue().AsString()  );
        }

        Value v( false );
        if( ctk.Variable() ) { ctk.Variable()->GetValue( v ); }

        wxControl *wxctrl = 0;
        switch( ctk.Type() )
        {
        case ctLabel:
            break;

        case ctTextBox:
            // For text box the info string defines the number of columns, the
            // number of rows, and the validation string.  These are entered as
            //   n~v~d where ~ is a non numeric delimiter.
        {
            int nch = 32;
            int nline = 1;
            long number;
            if( ! info.IsEmpty() )
            {
                wxString numbers("0123456789");
                wxString delim('~');
                for( size_t i = 0; i < info.Len(); i++ )
                {
                    if( ! numbers.Contains( info[i] ) )
                    {
                        delim = info.Mid( i, 1 );
                        break;
                    }
                }
                wxStringTokenizer tok( info, delim, wxTOKEN_RET_EMPTY );
                if( tok.GetNextToken().ToLong( &number ) && number > 0 ) nch = number;
                if( tok.GetNextToken().ToLong( &number ) && number > 0 ) nline = number;
                info = tok.GetString();
            }
            wxctrl = TextBox( control.stringVal, nch, nline, info );
        }
        break;

        case ctListBox:
        {
            wxRegEx extended("\\yextended\\y",wxRE_ADVANCED | wxRE_ICASE);
            wxRegEx multiple("\\ymultiple\\y",wxRE_ADVANCED | wxRE_ICASE);
            wxRegEx size("\\y(\\d+)x(\\d+)\\y",wxRE_ADVANCED | wxRE_ICASE);
            int type = wxLB_SINGLE;
            long width = -1;
            long height = -1;
            if( extended.Matches(info) ) type = wxLB_EXTENDED;
            else if( multiple.Matches(info) ) type = wxLB_MULTIPLE;
            if( size.Matches(info) )
            {
                size.GetMatch(info,1).ToLong(&width);
                size.GetMatch(info,2).ToLong(&height);
                width = (long) (width*GetCharWidth());
                height = (long) (1.1*height*GetCharHeight());
            }
            wxctrl = ListBox( control.stringVal, selector, type, (int) width, (int) height );
        }
        break;

        case ctCheckBox:
            wxctrl = CheckBox( label, control.boolVal );
            label.Clear();
            break;

        case ctRadioSelector:
            selectorString = GetSelectorString( selector );
            wxctrl = RadioBox( control.intVal, selectorString, label );
            label.Clear();
            break;

        case ctDropDownSelector:
            selectorString = GetSelectorString( selector );
            wxctrl = DropDownBox( control.intVal, selectorString );
            break;

        case ctOpenFileSelector:
            wxctrl = OpenFileBox( control.stringVal, label, selector, info  );
            break;

        case ctSaveFileSelector:
            wxctrl = SaveFileBox( control.stringVal, label, selector, info );
            break;

        case ctButton:
            wxctrl = Button( label, (wxObjectEventFunction) &ScriptDialog::HandleButton );
            control.intVal = wxctrl->GetId();
            label.Clear();
            break;

        case ctSpacer:
            sizer->AddSpacer( GetCharHeight() );
            break;

        case ctNewRow:
            columns = new wxBoxSizer( wxHORIZONTAL );
            rows->Add(columns);
            sizer = new wxBoxSizer( wxVERTICAL );
            columns->AddSpacer( GetCharWidth()*2 );
            columns->Add( sizer );
            break;

        case ctNewColumn:
            sizer = new wxBoxSizer( wxVERTICAL );
            columns->AddSpacer( GetCharWidth()*2 );
            columns->Add( sizer );
            break;

        case ctValidator:
            continue;
        }
        if( ! label.IsEmpty() ) { sizer->Add( Label( label ), sizerFlags ); }
        if( wxctrl ) { sizer->Add( wxctrl, sizerFlags ); control.control = wxctrl;  }
    }
    AddSizer( rows );
}

void ScriptDialog::HandleButton( wxEvent &event )
{
    int id = event.GetId();
    for( int i = 0; i < nControls; i++ )
    {
        ScriptDialogControl &control = controls[i];
        DialogControlToken &ctk = *(control.token);
        if( ctk.Type() == ctButton && control.intVal == id )
        {
            Token *actions = ctk.Actions();
            if( actions )
            {
                RunDialogProgram( actions, false );
            }
        }
    }
}

bool ScriptDialog::Validate()
{
    bool ok = wxSimpleDialog::Validate();
    for( int i = 0;  ok && i < nControls; i++ )
    {
        DialogControlToken &ctk = *(controls[i].token);
        if( ctk.Type() == ctValidator )
        {
            Token *validation = ctk.Actions();
            if( validation )
            {
                ok = RunDialogProgram( validation, true );
            }
        }
    }
    return ok;
}

bool ScriptDialog::RunDialogProgram( Token *program, bool isValidation )
{
    RunPermission rp( program->Owner(), elExit );
    if( ! rp.CanRun() ) return true; // Must return true otherwise may never exit validator

    TransferDataFromWindow();
    Apply();

    Value v = program->GetValue();
    bool result = v.AsBool();

    if( ! isValidation )
    {
        ExitLevel el = program->Owner()->GetExitLevel();
        if( el == elExit || el == elReturn ) EndModal( result ? wxID_OK : wxID_CANCEL );
    }
    ReadData();
    TransferDataToWindow();
    return result;
}

void ScriptDialog::ReadData()
{

    Value v;
    for( int i = 0; i < nControls; i++ )
    {
        ScriptDialogControl &control = controls[i];
        DialogControlToken &ctk = *(control.token);

        switch( ctk.Type() )
        {
        case ctTextBox:
        case ctListBox:
        case ctOpenFileSelector:
        case ctSaveFileSelector:
            ctk.Variable()->GetValue( v );
            control.stringVal = v.AsString();
            break;

        case ctCheckBox:
            ctk.Variable()->GetValue( v );
            control.boolVal = v.AsBool();
            break;

        case ctRadioSelector:
        case ctDropDownSelector:
            ctk.Variable()->GetValue( v );
            control.intVal = GetSelectedIndex( control.selector, v.AsString() );
            break;

        default:
            break;
        }

        // List may have their contents changed by a dialog program, so and
        // reset them if necessary ..

        if( ctk.Type() == ctListBox || ctk.Type() == ctDropDownSelector )
        {
            if( ctk.Selector() )
            {
                wxString selector = ctk.Selector()->GetValue().AsString();
                if( selector != control.selector )
                {
                    if( ctk.Type() == ctListBox )
                    {
                        SetListBoxOptions( (wxListBox *) control.control, control.stringVal, selector );
                    }
                    else if( ctk.Type() == ctDropDownSelector )
                    {
                        SetControlWithItemsOptions( (wxControlWithItems *) control.control, control.intVal, GetSelectorString(selector) );
                    }
                    control.selector = selector;
                }
            }
        }

    }
}

void ScriptDialog::Apply()
{

    for( int i = 0; i < nControls; i++ )
    {
        ScriptDialogControl &control = controls[i];
        DialogControlToken &ctk = *(control.token);

        switch( ctk.Type() )
        {
        case ctTextBox:
        case ctListBox:
        case ctOpenFileSelector:
        case ctSaveFileSelector:
            ctk.Variable()->SetOwnerValue( Value(control.stringVal) );
            break;

        case ctCheckBox:
            ctk.Variable()->SetOwnerValue( Value( control.boolVal ));
            break;

        case ctRadioSelector:
        case ctDropDownSelector:
            {
            wxString selectorValue = GetSelectedValue( control.selector, control.intVal );
            ctk.Variable()->SetOwnerValue( Value(selectorValue) );
            }
            break;

        default:
            break;
        }
    }
}

// Selector definitions are supplied as:
//   ~value~description~value~description
// where ~ may be any delimiter character

wxString ScriptDialog::GetSelectorString( wxString selectorDef )
{
    wxString delim = selectorDef.Mid(0,1);
    wxStringTokenizer tok( selectorDef, delim, wxTOKEN_RET_EMPTY_ALL );
    wxString selectorString;

    if( tok.CountTokens() > 2 )
    {
        tok.GetNextToken();  // Skip initial empty wxString
        while( tok.CountTokens() > 1 )
        {
            wxString indexValue = tok.GetNextToken();
            selectorString.Append(delim);
            selectorString.Append( tok.GetNextToken() );
        }
    }
    return selectorString;
}

int ScriptDialog::GetSelectedIndex( wxString selectorDef, wxString selected )
{
    wxString delim = selectorDef.Mid(0,1);
    int index;
    int result = 0;
    wxStringTokenizer tok( selectorDef, delim, wxTOKEN_RET_EMPTY_ALL );

    index = 0;
    while( tok.CountTokens() > 1 )
    {
        tok.GetNextToken();
        if( tok.GetNextToken().IsSameAs( selected ) ) { result = index; break; }
        index++;
    }
    return result;
}

wxString ScriptDialog::GetSelectedValue( wxString selectorDef, int selected )
{
    wxString delim = selectorDef.Mid(0,1);
    wxString value;
    wxStringTokenizer tok( selectorDef, delim, wxTOKEN_RET_EMPTY_ALL );
    if( (int) tok.CountTokens() >= selected )
    {
        // Skip to required token
        for( int i = selected*2+1; i > 0; i-- )
        {
            tok.GetNextToken();
        }
        value = tok.GetNextToken();
    }
    return value;
}

/////////////////////////////////////////////////////////////////////////////


DialogToken::~DialogToken()
{
    if( label ) delete label;
    if( variable ) delete variable;
    if( controls ) delete controls;
}

Value DialogToken::evaluate()
{
    wxString strOptions;
    long buttons = wxOK | wxCANCEL;
    if( options )
    {
        strOptions = options->GetValue().AsString();
        if( strOptions.IsSameAs("ok",false)) buttons=wxOK;
        if( strOptions.IsSameAs("cancel",false)) buttons=wxCANCEL;
        if( strOptions.IsSameAs("none",false)) buttons=0;
    }

    ScriptDialog dialog( label->GetValue().AsString(), controls, buttons );
    bool ok = dialog.RunDialog();

    // Inital values may have been altered when running dialog programs

    if( ! ok ) dialog.ResetInitialValues();
    if( variable )
    {
        variable->SetOwnerValue(Value(ok));
    }
    else if ( ! ok )
    {
        Owner()->SetExitLevel( elExit );
    }
    return ok;
}

void DialogToken::print( const wxString &prefix, ostream &str )
{
    str << prefix << "DialogControl" << endl;
    PrintSubtoken( label, prefix, str );
    PrintSubtoken( controls, prefix, str );
}

// Evaluating Dialog token does nothing .. evaluation done by owning dialog

DialogControlToken::~DialogControlToken()
{
    if( variable ) delete variable;
    if( label ) delete label;
    if( selector ) delete selector;
    if( info ) delete info;
}

Value DialogControlToken::evaluate() { return Value(true); }

void DialogControlToken::print( const wxString &prefix, ostream &str )
{
    const char *typeString = "unknown";
    switch( type )
    {
        case ctLabel: typeString = "label"; break;
        case ctTextBox: typeString = "text_box"; break;
        case ctCheckBox: typeString = "check_box"; break;
        case ctRadioSelector: typeString = "radio_selector"; break;
        case ctDropDownSelector: typeString = "dropdown_selector"; break;
        case ctOpenFileSelector: typeString = "open_file_selector"; break;
        case ctSaveFileSelector: typeString = "save_file_selector"; break;
        default: break;
    }
    str << prefix << "DialogControlToken: " << typeString << endl;
    PrintSubtoken( variable, prefix, str );
    PrintSubtoken( label, prefix, str );
    PrintSubtoken( selector, prefix, str );
    PrintSubtoken( info, prefix, str );
}
