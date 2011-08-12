#include "snapconfig.h"
#include "wxsimpledialog.hpp"
#include "wxregexvalidator.hpp"
#include "wxnumbervalidator.hpp"
#include <wx/tokenzr.h>

#include <stdarg.h>


wxRadioBoxOptionValidator::wxRadioBoxOptionValidator( wxRadioBoxOption *options, int *value ) :
    wxValidator(),
    value(value),
    options(options)
{
}

wxRadioBoxOptionValidator::wxRadioBoxOptionValidator( const wxRadioBoxOptionValidator &validator ) :
    wxValidator(),
    value(validator.value),
    options( validator.options )
{
}

bool wxRadioBoxOptionValidator::TransferToWindow()
{
    wxControlWithItems *rb = static_cast<wxControlWithItems *>( GetWindow() );
    int optVal = value ? *value : options[0].value;
    rb->SetSelection(0);
    for( int i = 0; options[i].name; i++ )
    {
        if( options[i].value == optVal )
        {
            rb->SetSelection(i);
            break;
        }
    }
    return true;
}

bool wxRadioBoxOptionValidator::TransferFromWindow()
{
    wxControlWithItems *rb = static_cast<wxControlWithItems *>( GetWindow() );
    if( value )
    {
        *value = options[rb->GetSelection()].value;
    }
    return true;
}

bool wxRadioBoxOptionValidator::Validate( wxWindow * WXUNUSED(parent) )
{
    return true;
}

//===========================================================================

wxSimpleListBoxValidator::wxSimpleListBoxValidator( char delimiter, wxString *value ) :
    delimiter(delimiter), value(value)
{
}

wxSimpleListBoxValidator::wxSimpleListBoxValidator( const wxSimpleListBoxValidator &validator )
{
    delimiter = validator.delimiter;
    value = validator.value;
}

bool wxSimpleListBoxValidator::TransferToWindow()
{
    if( ! value ) return true;
    wxListBox *lb = static_cast<wxListBox *>( GetWindow() );
    wxStringTokenizer tok( *value, wxString(delimiter) );
    while( tok.HasMoreTokens() )
    {
        wxString selValue = tok.GetNextToken();
        lb->SetStringSelection( selValue );
    }
    return true;
}

bool wxSimpleListBoxValidator::TransferFromWindow()
{
    if( ! value ) return true;
    wxListBox *lb = static_cast<wxListBox *>( GetWindow() );
    wxString result;

    wxArrayInt selections;
    lb->GetSelections( selections );

    for( int i = 0; i < (int) selections.Count(); i++ )
    {
        int is = selections.Item(i);
        wxString selValue = lb->GetString( is );
        if( result.Length() > 0 ) result.Append(delimiter);
        result.Append(selValue);
    }

    *value = result;
    return true;
}

bool wxSimpleListBoxValidator::Validate( wxWindow * WXUNUSED(parent) )
{
    return true;
}

//===========================================================================

wxFilePickerValidator::wxFilePickerValidator( wxString *filename, const wxString &message )
    : filename(filename), message(message)
{
}

wxFilePickerValidator::wxFilePickerValidator( const wxFilePickerValidator &validator )
{
    filename = validator.filename;
    message = validator.message;
}

bool wxFilePickerValidator::TransferToWindow()
{
    wxFilePickerCtrl *fp = static_cast<wxFilePickerCtrl *>( GetWindow() );
    if( filename ) fp->SetPath( *filename );
    return true;
}

bool wxFilePickerValidator::TransferFromWindow()
{
    wxFilePickerCtrl *fp = static_cast<wxFilePickerCtrl *>( GetWindow() );
    if( filename ) *filename = fp->GetPath();
    return true;
}

bool wxFilePickerValidator::Validate( wxWindow * WXUNUSED(parent) )
{
    wxFilePickerCtrl *fp = static_cast<wxFilePickerCtrl *>( GetWindow() );
    if( ! message.IsEmpty() && fp->GetPath().IsEmpty() )
    {
        ::wxMessageBox( message,_T("Missing filename"),wxICON_EXCLAMATION | wxOK );
        return false;
    }
    return true;
}


//===========================================================================


BEGIN_EVENT_TABLE( wxSimpleDialog, wxDialog)
    EVT_BUTTON( wxID_HELP, wxSimpleDialog::ShowHelp )
    EVT_HELP(wxID_ANY, wxSimpleDialog::ShowHelp )
    EVT_IDLE( wxSimpleDialog::OnIdle )
END_EVENT_TABLE()

wxSimpleDialog::wxSimpleDialog( const char *title, long buttonFlags )
    : wxDialog(
        wxTheApp->GetTopWindow(),
        wxID_ANY,
        _T(title),
        wxDefaultPosition,
        wxDefaultSize
    )
{
    help = 0;
    helpButton = 0;
    lastWindow = 0;
    nextId = wxID_HIGHEST+1;
    this->buttonFlags = buttonFlags;
    sizer = new wxBoxSizer( wxVERTICAL );
    sizerFlags.Left();
    sizerFlags.Expand();
    sizerFlags.Border( wxLEFT | wxRIGHT );
    AddSpacer();
}


wxSimpleDialog::~wxSimpleDialog()
{
}

wxStaticText *wxSimpleDialog::Label( const char *text, wxWindow *window )
{
    wxStaticText *stLabel = new wxStaticText( this, wxID_ANY, _T(text) );
    if( window ) stLabel->MoveBeforeInTabOrder( window );
    return stLabel;
}

wxTextCtrl *wxSimpleDialog::TextBox( wxString &string, int nch, int nlines, const char *validChar )
{
    wxString rules = wxEmptyString;
    if( validChar ) rules = wxString(_T(validChar));

    long style = wxTE_LEFT;

    if( rules.IsSameAs(_T("readonly"), false ) )
    {
        style |= wxTE_READONLY;
        rules = wxEmptyString;
    }

    if( nlines > 1 )
    {
        style |= wxTE_MULTILINE;
    }
    else
    {
        nlines = 1;
    }

    wxRegexpStringValidator v( &string, rules );

    wxTextCtrl *ctrl = new wxTextCtrl(
        this,
        wxID_ANY,
        "",
        wxDefaultPosition,
        wxSize( GetCharWidth()*(nch+1), (GetCharHeight()*(nlines*3+2))/3 ),
        style,
        v
    );

    return ctrl;
}

wxTextCtrl *wxSimpleDialog::NumberBox( int &value, bool positive, int nch )
{
    wxTextCtrl *ctrl = new wxTextCtrl( this,
                                       wxID_ANY,
                                       "",
                                       wxDefaultPosition,
                                       wxSize( GetCharWidth()*(nch+1), (GetCharHeight()*5)/3 ),
                                       wxTE_RIGHT,
                                       wxNumberValidator( &value, positive )
                                     );
    return ctrl;
}

wxTextCtrl *wxSimpleDialog::NumberBox( double &value, bool positive, int ndp, int nch )
{
    wxTextCtrl *ctrl = new wxTextCtrl( this,
                                       wxID_ANY,
                                       "",
                                       wxDefaultPosition,
                                       wxDefaultSize,
                                       wxTE_RIGHT,
                                       wxNumberValidator( &value, ndp, positive )
                                     );
    ctrl->SetSize( wxSize(ctrl->GetCharWidth()*nch,ctrl->GetSize().GetHeight()));
    return ctrl;
}


wxListBox *wxSimpleDialog::ListBox( wxString &value, const char *options, int type, int width, int height )
{
    wxArrayString arrLabels;
    wxSize lbSize = wxDefaultSize;

    wxListBox *ctrl = new wxListBox( this,
                                     wxID_ANY,
                                     wxDefaultPosition,
                                     wxSize(width,height),
                                     arrLabels,
                                     type);

    SetListBoxOptions( ctrl, value, options );

    return ctrl;
}

wxRadioBox *wxSimpleDialog::RadioBox( int &value, const char *options, const char *label, bool horizontal )
{
    wxArrayString arrLabels;
    wxString strLabels( _T(options+1) );
    wxString delim( options[0] );
    wxStringTokenizer tok( strLabels, delim );

    while( tok.HasMoreTokens() )
    {
        arrLabels.Add( tok.GetNextToken() );
    }


    wxString strLabel( label ? _T(label) : _T(""));

    wxRadioBox *ctrl = new wxRadioBox(this,
                                      wxID_ANY,
                                      strLabel,
                                      wxDefaultPosition,
                                      wxDefaultSize,
                                      arrLabels,
                                      1,
                                      horizontal ? wxRA_SPECIFY_ROWS : wxRA_SPECIFY_COLS,
                                      wxGenericValidator( &value )
                                     );

    return ctrl;
}

wxRadioBox *wxSimpleDialog::RadioBox( int &value, wxRadioBoxOption *options, const char *label, bool horizontal )
{
    wxArrayString arrLabels;
    for( int i = 0; options[i].name; i++ )
    {
        arrLabels.Add( wxString(_T(options[i].name)));
    }

    wxString strLabel( label ? _T(label) : _T(""));

    wxRadioBox *ctrl = new wxRadioBox(this,
                                      wxID_ANY,
                                      strLabel,
                                      wxDefaultPosition,
                                      wxDefaultSize,
                                      arrLabels,
                                      1,
                                      horizontal ? wxRA_SPECIFY_ROWS : wxRA_SPECIFY_COLS,
                                      wxRadioBoxOptionValidator( options, &value )
                                     );

    return ctrl;
}

wxChoice *wxSimpleDialog::DropDownBox( int &value, const char *options )
{
    wxArrayString arrLabels;

    wxChoice *ctrl = new wxChoice(this,
                                  wxID_ANY,
                                  wxDefaultPosition,
                                  wxDefaultSize,
                                  arrLabels,
                                  0
                                 );

    SetControlWithItemsOptions( ctrl, value, options );
    return ctrl;
}

wxChoice *wxSimpleDialog::DropDownBox( int &value, wxRadioBoxOption *options )
{
    wxArrayString arrLabels;

    wxChoice *ctrl = new wxChoice(this,
                                  wxID_ANY,
                                  wxDefaultPosition,
                                  wxDefaultSize,
                                  arrLabels,
                                  0
                                 );

    SetControlWithItemsOptions( ctrl, value, options );
    return ctrl;
}



wxCheckBox *wxSimpleDialog::CheckBox( const char *label, bool &value )
{
    return new wxCheckBox(
               this,
               wxID_ANY,
               _T(label),
               wxDefaultPosition,
               wxDefaultSize,
               0,
               wxGenericValidator( &value )
           );
}

wxFilePickerCtrl *wxSimpleDialog::OpenFileBox( wxString &filename, const char *label, const char *wildcard, const char *message )
{
    wxFilePickerCtrl *fp = new wxFilePickerCtrl(
        this,
        wxID_ANY,
        wxEmptyString,
        _T(label),
        _T(wildcard),
        wxDefaultPosition,
        wxDefaultSize,
        wxFLP_USE_TEXTCTRL | wxFLP_OPEN,
        wxFilePickerValidator( &filename, message )
    );
    fp->SetMinSize( wxSize( GetCharWidth()*(50), (GetCharHeight()*5)/3 ));
    return fp;
}

wxFilePickerCtrl *wxSimpleDialog::SaveFileBox( wxString &filename, const char *label, const char *wildcard, const char *message )
{
    wxFilePickerCtrl *fp =  new wxFilePickerCtrl(
        this,
        wxID_ANY,
        wxEmptyString,
        _T(label),
        _T(wildcard),
        wxDefaultPosition,
        wxDefaultSize,
        wxFLP_USE_TEXTCTRL | wxFLP_SAVE,
        wxFilePickerValidator( &filename, message )
    );
    fp->SetMinSize( wxSize( GetCharWidth()*(50), (GetCharHeight()*5)/3 ));
    return fp;

}

wxButton *wxSimpleDialog::Button( const char *label, wxObjectEventFunction function )
{
    int id = nextId++;
    wxButton *button = new wxButton(
        this,
        id,
        _T(label)
    );
    Connect( id, wxEVT_COMMAND_BUTTON_CLICKED, function );
    return button;
}

void wxSimpleDialog::AddSpacer()
{
    sizer->AddSpacer( GetCharHeight());
}

wxStaticText *wxSimpleDialog::AddLabel(const char *label, wxWindow *window )
{
    wxStaticText *lb = Label(label, window);
    AddControl( 0, lb );
    return lb;
}

wxTextCtrl *wxSimpleDialog::AddTextBox(const char *label, wxString &string, int nch, int nlines, const char *validChar )
{
    wxTextCtrl *txtctrl = TextBox( string, nch, nlines, validChar );
    AddControl( label, txtctrl );
    return txtctrl;
}

wxTextCtrl *wxSimpleDialog::AddNumberBox(const char *label, int &value, bool positive, int nch )
{
    wxTextCtrl *txtctrl = NumberBox( value, positive, nch );
    AddControl( label, txtctrl );
    return txtctrl;;
}

wxTextCtrl *wxSimpleDialog::AddNumberBox(const char *label, double &value, bool positive, int ndp, int nch )
{
    wxTextCtrl *txtctrl =  NumberBox( value, positive, ndp, nch );
    AddControl( label, txtctrl );
    return txtctrl;
}

wxListBox *wxSimpleDialog::AddListBox(const char *label, wxString &string, const char *options, int type, int width, int height )
{
    wxListBox *listbox = ListBox( string, options, type, width, height );
    AddControl( label, listbox );
    return listbox;
}

wxRadioBox *wxSimpleDialog::AddRadioBox(const char *label, int &value, const char *options, bool horizontal )
{
    wxRadioBox *rb = RadioBox( value, options, label, horizontal );
    AddControl( 0, rb );
    return rb;
}

wxRadioBox *wxSimpleDialog::AddRadioBox(const char *label, int &value, wxRadioBoxOption *options, bool horizontal )
{
    wxRadioBox *rb = RadioBox( value, options, label, horizontal );
    AddControl( 0, rb );
    return rb;
}

wxChoice *wxSimpleDialog::AddDropDownBox(const char *label, int &value, const char *options )
{
    wxChoice *ddb = DropDownBox( value, options );
    AddControl( label, ddb );
    return ddb;
}

wxChoice *wxSimpleDialog::AddDropDownBox(const char *label, int &value, wxRadioBoxOption *options )
{
    wxChoice *ddb = DropDownBox( value, options );
    AddControl( label, ddb );
    return ddb;
}

wxCheckBox *wxSimpleDialog::AddCheckBox(const char *label, bool &value )
{
    wxCheckBox *cb = CheckBox( label, value );
    AddControl( 0, cb );
    return cb;
}

wxFilePickerCtrl *wxSimpleDialog::AddOpenFileBox( const char *label, wxString &string, const char *wildcard, const char *message )
{
    wxFilePickerCtrl *fp = OpenFileBox( string, label, wildcard, message );
    AddControl( label, fp );
    return fp;
}

wxFilePickerCtrl *wxSimpleDialog::AddSaveFileBox( const char *label, wxString &string, const char *wildcard, const char *message )
{
    wxFilePickerCtrl *fp = SaveFileBox( string, label, wildcard, message );
    AddControl( label, fp );
    return fp;
}


wxButton *wxSimpleDialog::AddButton( const char *label, wxObjectEventFunction function )
{
    wxButton *button = Button(label,function);
    AddControl( 0, button );
    return button;
}

void wxSimpleDialog::AddControl(const char *label, wxWindow *window )
{
    if( label && label[0])
    {
        AddControls( 2, Label(label,window), window, 0 );
    }
    else
    {
        sizer->Add( window, sizerFlags );
    }
}

void wxSimpleDialog::AddControl(wxWindow *window, const wxSizerFlags &flags  )
{
    sizer->Add( window, flags );
}

void wxSimpleDialog::AddControls( int nColumns, wxWindow *control, ... )
{
    if( ! control ) return;

    wxFlexGridSizer *sizer = new wxFlexGridSizer(nColumns,0,GetCharWidth());

    va_list argptr;
    for( va_start( argptr, control ); control; control = va_arg( argptr, wxWindow *) )
    {
        sizer->Add(control);
    }
    AddSizer( sizer );
}


void wxSimpleDialog::AddSizer(wxSizer *subSizer )
{
    sizer->Add( subSizer, sizerFlags );
}

void wxSimpleDialog::AddSizer( wxSizer *subSizer, const wxSizerFlags &flags )
{
    sizer->Add( subSizer, flags );
}

wxWindow *wxSimpleDialog::ResetTabOrder( wxSizer *sizer, wxWindow *last )
{
    wxSizerItemList &children = sizer->GetChildren();
    for( size_t i = 0; i < children.GetCount(); i++ )
    {
        wxSizerItem *item = children.Item( i )->GetData();
        if( item->IsWindow() )
        {
            wxWindow *window = item->GetWindow();
            if( last ) window->MoveAfterInTabOrder( last );
            last = window;
        }
        else if( item->IsSizer() )
        {
            wxSizer *s = item->GetSizer();
            last = ResetTabOrder( s, last );
        }

    }
    return last;
}

void wxSimpleDialog::AddButtonsAndSize()
{
    AddSpacer();
    if( buttonFlags )
    {
        wxSizer *buttonSizer = CreateButtonSizer( buttonFlags );

        // See if we have a help button

        wxSizerItemList &buttons = buttonSizer->GetChildren();
        for( size_t i = 0; i < buttons.GetCount(); i++ )
        {
            wxWindow *window = buttons.Item(i)->GetData()->GetWindow();
            if( window && window->GetId() == wxID_HELP )
            {
                helpButton = window;
            }
        }

        AddSizer( buttonSizer );
        AddSpacer();
    }
    SetSizerAndFit( sizer );
    lastWindow = ResetTabOrder( sizer, 0 );
}

// Crude work-around to try and get first focusable window focussed when dialog
// displays ... really need an on create or similar event.

void wxSimpleDialog::OnIdle( wxIdleEvent & WXUNUSED(event) )
{
    if( lastWindow )
    {
        // Set focus on the last window, then tab forwards
        // to the first window which can actually use focus ...
        // Reset lastWindow to 0 so that this only happens on the first idle
        // event received by the window..

        lastWindow->SetFocus();
        lastWindow->Navigate();
        lastWindow = 0;
    }
}

void wxSimpleDialog::MakeSizeable()
{
    SetWindowStyle( GetWindowStyle() | wxRESIZE_BORDER );
}

void wxSimpleDialog::ReadData()
{
}

void wxSimpleDialog::Apply()
{
}


void wxSimpleDialog::SetupHelp( wxHelpController *controller, wxString topic )
{
    help = controller;
    helpTopic = topic;
}

void wxSimpleDialog::ShowHelp( wxCommandEvent & WXUNUSED(event) )
{
    if( help ) help->DisplaySection( helpTopic );
}

void wxSimpleDialog::ShowHelp( wxHelpEvent & WXUNUSED(event) )
{
    if( help ) help->DisplaySection( helpTopic );
}

bool wxSimpleDialog::RunDialog()
{
    if( helpButton )
    {
        helpButton->Show( help != 0 );
    }
    ReadData();
    int result = ShowModal();
    bool apply = result == wxID_OK;
    if( apply ) Apply();
    return apply;
}


void wxSimpleDialog::SetListBoxOptions( wxListBox *lb, wxString &string, const char *options )
{
    wxString strLabels( _T(options+1) );
    wxString delim( options[0] );
    wxStringTokenizer tok( strLabels, delim );

    lb->Clear();
    while( tok.HasMoreTokens() )
    {
        lb->Append( tok.GetNextToken() );
    }

    lb->SetValidator( wxSimpleListBoxValidator( options[0], &string ));
}

void wxSimpleDialog::SetControlWithItemsOptions( wxControlWithItems *ctrl, int &value, const char *options )
{
    wxString strLabels( _T(options+1) );
    wxString delim( options[0] );
    wxStringTokenizer tok( strLabels, delim );

    ctrl->Clear();
    while( tok.HasMoreTokens() )
    {
        ctrl->Append( tok.GetNextToken() );
    }

    ctrl->SetValidator( wxGenericValidator( &value ) );
}

void wxSimpleDialog::SetControlWithItemsOptions( wxControlWithItems *ctrl, int &value, wxRadioBoxOption *options )
{
    ctrl->Clear();
    for( int i = 0; options[i].name; i++ )
    {
        ctrl->Append( wxString(_T(options[i].name)));
    }

    ctrl->SetValidator(wxRadioBoxOptionValidator( options, &value ));
}
