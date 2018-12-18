#include "snapconfig.h"
#include "wxsimpledialog.hpp"
#include "wxregexvalidator.hpp"
#include "wxnumbervalidator.hpp"
#include <wx/tokenzr.h>

#include <stdarg.h>


wxOptionValidatorBase::wxOptionValidatorBase( ListControlOption *options, int *value ) :
    wxValidator(),
    value(value),
    nOptions(0),
    optValues(0)
{
    for( int i=0; options[i].name; i++ )
    {
        nOptions++;
    }
    optValues = new int[nOptions];
    for( int i=0; i < nOptions; i++ )
    {
        optValues[i]=options[i].value;
    }
}

wxOptionValidatorBase::wxOptionValidatorBase( const wxOptionValidatorBase &validator ) :
    wxValidator(),
    value(validator.value),
    nOptions( validator.nOptions ),
    optValues( 0 )
{
    optValues = new int[nOptions];
    for( int i=0; i < nOptions; i++ )
    {
        optValues[i]=validator.optValues[i];
    }
}

wxOptionValidatorBase::~wxOptionValidatorBase()
{
    delete [] optValues;
}

bool wxOptionValidatorBase::TransferToWindow()
{
    int optVal = value ? *value : optValues[0];
    SetSelectedIndex(0);
    for( int i = 0; i < nOptions; i++ )
    {
        if( optValues[i] == optVal )
        {
            SetSelectedIndex(i);
            break;
        }
    }
    return true;
}

bool wxOptionValidatorBase::TransferFromWindow()
{
    if( value )
    {
        int optval=GetSelectedIndex();
        if( optval >= 0 && optval < nOptions )
        {
            *value = optValues[optval];
        }
        else
        {
            *value = 0;
        }
    }
    return true;
}

bool wxOptionValidatorBase::Validate( wxWindow * WXUNUSED(parent) )
{
    return true;
}

//===========================================================================

int wxRadioBoxOptionValidator::GetSelectedIndex()
{
    wxRadioBox *rb = static_cast<wxRadioBox *>( GetWindow() );
    return rb->GetSelection();
}

void wxRadioBoxOptionValidator::SetSelectedIndex( int i )
{
    wxRadioBox *rb = static_cast<wxRadioBox *>( GetWindow() );
    rb->SetSelection(i);
}

//===========================================================================

class wxControlWithItemsValidator : public wxOptionValidatorBase
{
public:
    wxControlWithItemsValidator( wxControlWithItems *ctrl, ListControlOption *options, int *value = NULL):
        wxOptionValidatorBase(options,value), ctrl(ctrl) {};
    wxControlWithItemsValidator( const wxControlWithItemsValidator &validator ) : wxOptionValidatorBase(validator), ctrl(validator.ctrl) {};
    virtual wxValidator *Clone() const { return new wxControlWithItemsValidator( *this ); }
protected:
    virtual int GetSelectedIndex();
    virtual void SetSelectedIndex( int i );
private:
    wxControlWithItems *ctrl;
};

int wxControlWithItemsValidator::GetSelectedIndex()
{
    return ctrl->GetSelection();
}

void wxControlWithItemsValidator::SetSelectedIndex( int i )
{
    ctrl->SetSelection(i);
}

//===========================================================================

wxSimpleListBoxValidator::wxSimpleListBoxValidator( char delimiter, wxString *value ) :
    value(value), delimiter(delimiter)
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
        ::wxMessageBox( message,"Missing filename",wxICON_EXCLAMATION | wxOK );
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

wxSimpleDialog::wxSimpleDialog( wxString title, long buttonFlags )
    : wxDialog(
        wxTheApp->GetTopWindow(),
        wxID_ANY,
        title,
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

wxStaticText *wxSimpleDialog::Label( wxString text, wxWindow *window )
{
    wxStaticText *stLabel = new wxStaticText( this, wxID_ANY, text );
    if( window ) stLabel->MoveBeforeInTabOrder( window );
    return stLabel;
}

wxTextCtrl *wxSimpleDialog::TextBox( wxString &string, int nch, int nlines, wxString validChar )
{
    long style = wxTE_LEFT;

    if( validChar.IsSameAs("readonly", false ) )
    {
        style |= wxTE_READONLY;
        validChar = wxEmptyString;
    }

    if( nlines > 1 )
    {
        style |= wxTE_MULTILINE;
    }
    else
    {
        nlines = 1;
    }

    wxRegexpStringValidator v( &string, validChar );

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


wxListBox *wxSimpleDialog::ListBox( wxString &value, wxString options, int type, int width, int height )
{
    wxArrayString arrLabels;

    wxListBox *ctrl = new wxListBox( this,
                                     wxID_ANY,
                                     wxDefaultPosition,
                                     wxSize(width,height),
                                     arrLabels,
                                     type);

    SetListBoxOptions( ctrl, value, options );

    return ctrl;
}

wxRadioBox *wxSimpleDialog::RadioBox( int &value, wxString options, wxString label, bool horizontal )
{
    wxArrayString arrLabels;
    wxString delim=options[0];
    wxString strLabels=options.Mid(1);
    wxStringTokenizer tok( strLabels, delim );

    while( tok.HasMoreTokens() )
    {
        arrLabels.Add( tok.GetNextToken() );
    }


    wxRadioBox *ctrl = new wxRadioBox(this,
                                      wxID_ANY,
                                      label,
                                      wxDefaultPosition,
                                      wxDefaultSize,
                                      arrLabels,
                                      1,
                                      horizontal ? wxRA_SPECIFY_ROWS : wxRA_SPECIFY_COLS,
                                      wxGenericValidator( &value )
                                     );

    return ctrl;
}

wxRadioBox *wxSimpleDialog::RadioBox( int &value, ListControlOption *options, wxString label, bool horizontal )
{
    wxArrayString arrLabels;
    for( int i = 0; options[i].name; i++ )
    {
        arrLabels.Add( wxString(options[i].name));
    }
    wxRadioBox *ctrl = new wxRadioBox(this,
                                      wxID_ANY,
                                      label,
                                      wxDefaultPosition,
                                      wxDefaultSize,
                                      arrLabels,
                                      1,
                                      horizontal ? wxRA_SPECIFY_ROWS : wxRA_SPECIFY_COLS,
                                      wxRadioBoxOptionValidator( options, &value )
                                     );

    return ctrl;
}

wxChoice *wxSimpleDialog::DropDownBox( int &value, wxString options )
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

wxChoice *wxSimpleDialog::DropDownBox( int &value, ListControlOption *options )
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



wxCheckBox *wxSimpleDialog::CheckBox( wxString label, bool &value )
{
    return new wxCheckBox(
               this,
               wxID_ANY,
               label,
               wxDefaultPosition,
               wxDefaultSize,
               0,
               wxGenericValidator( &value )
           );
}

wxFilePickerCtrl *wxSimpleDialog::OpenFileBox( wxString &filename, wxString label, wxString wildcard, wxString message )
{
    wxFilePickerCtrl *fp = new wxFilePickerCtrl(
        this,
        wxID_ANY,
        wxEmptyString,
        label,
        wildcard,
        wxDefaultPosition,
        wxDefaultSize,
        wxFLP_USE_TEXTCTRL | wxFLP_OPEN,
        wxFilePickerValidator( &filename, message )
    );
    fp->SetMinSize( wxSize( GetCharWidth()*(50), (GetCharHeight()*5)/3 ));
    return fp;
}

wxFilePickerCtrl *wxSimpleDialog::SaveFileBox( wxString &filename, wxString label, wxString wildcard, wxString message )
{
    wxFilePickerCtrl *fp =  new wxFilePickerCtrl(
        this,
        wxID_ANY,
        wxEmptyString,
        label,
        wildcard,
        wxDefaultPosition,
        wxDefaultSize,
        wxFLP_USE_TEXTCTRL | wxFLP_SAVE,
        wxFilePickerValidator( &filename, message )
    );
    fp->SetMinSize( wxSize( GetCharWidth()*(50), (GetCharHeight()*5)/3 ));
    return fp;

}

wxButton *wxSimpleDialog::Button( wxString label, wxObjectEventFunction function )
{
    int id = nextId++;
    wxButton *button = new wxButton(
        this,
        id,
        label
    );
    Connect( id, wxEVT_COMMAND_BUTTON_CLICKED, function );
    return button;
}

void wxSimpleDialog::AddSpacer()
{
    sizer->AddSpacer( GetCharHeight());
}

wxStaticText *wxSimpleDialog::AddLabel(wxString label, wxWindow *window )
{
    wxStaticText *lb = Label(label, window);
    AddControl( wxEmptyString, lb );
    return lb;
}

wxTextCtrl *wxSimpleDialog::AddTextBox(wxString label, wxString &string, int nch, int nlines, wxString validChar )
{
    wxTextCtrl *txtctrl = TextBox( string, nch, nlines, validChar );
    AddControl( label, txtctrl );
    return txtctrl;
}

wxTextCtrl *wxSimpleDialog::AddNumberBox(wxString label, int &value, bool positive, int nch )
{
    wxTextCtrl *txtctrl = NumberBox( value, positive, nch );
    AddControl( label, txtctrl );
    return txtctrl;;
}

wxTextCtrl *wxSimpleDialog::AddNumberBox(wxString label, double &value, bool positive, int ndp, int nch )
{
    wxTextCtrl *txtctrl =  NumberBox( value, positive, ndp, nch );
    AddControl( label, txtctrl );
    return txtctrl;
}

wxListBox *wxSimpleDialog::AddListBox(wxString label, wxString &string, wxString options, int type, int width, int height )
{
    wxListBox *listbox = ListBox( string, options, type, width, height );
    AddControl( label, listbox );
    return listbox;
}

wxRadioBox *wxSimpleDialog::AddRadioBox(wxString label, int &value, wxString options, bool horizontal )
{
    wxRadioBox *rb = RadioBox( value, options, label, horizontal );
    AddControl( wxEmptyString, rb );
    return rb;
}

wxRadioBox *wxSimpleDialog::AddRadioBox(wxString label, int &value, ListControlOption *options, bool horizontal )
{
    wxRadioBox *rb = RadioBox( value, options, label, horizontal );
    AddControl( wxEmptyString, rb );
    return rb;
}

wxChoice *wxSimpleDialog::AddDropDownBox(wxString label, int &value, wxString options )
{
    wxChoice *ddb = DropDownBox( value, options );
    AddControl( label, ddb );
    return ddb;
}

wxChoice *wxSimpleDialog::AddDropDownBox(wxString label, int &value, ListControlOption *options )
{
    wxChoice *ddb = DropDownBox( value, options );
    AddControl( label, ddb );
    return ddb;
}

wxCheckBox *wxSimpleDialog::AddCheckBox(wxString label, bool &value )
{
    wxCheckBox *cb = CheckBox( label, value );
    AddControl( wxEmptyString, cb );
    return cb;
}

wxFilePickerCtrl *wxSimpleDialog::AddOpenFileBox( wxString label, wxString &string, wxString wildcard, wxString message )
{
    wxFilePickerCtrl *fp = OpenFileBox( string, label, wildcard, message );
    AddControl( label, fp );
    return fp;
}

wxFilePickerCtrl *wxSimpleDialog::AddSaveFileBox( wxString label, wxString &string, wxString wildcard, wxString message )
{
    wxFilePickerCtrl *fp = SaveFileBox( string, label, wildcard, message );
    AddControl( label, fp );
    return fp;
}


wxButton *wxSimpleDialog::AddButton( wxString label, wxObjectEventFunction function )
{
    wxButton *button = Button(label,function);
    AddControl( wxEmptyString, button );
    return button;
}

void wxSimpleDialog::AddControl(wxString label, wxWindow *window )
{
    if( ! label.IsEmpty() )
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


void wxSimpleDialog::SetListBoxOptions( wxListBox *lb, wxString &string, wxString options )
{
    wxString delim=options[0];
    wxString strLabels=options.Mid(1);
    wxStringTokenizer tok( strLabels, delim );

    lb->Clear();
    while( tok.HasMoreTokens() )
    {
        lb->Append( tok.GetNextToken() );
    }

    lb->SetValidator( wxSimpleListBoxValidator( options[0], &string ));
}

void wxSimpleDialog::SetControlWithItemsOptions( wxControlWithItems *ctrl, int &value, wxString options )
{
    wxString delim=options[0];
    wxString strLabels=options.Mid(1);
    wxStringTokenizer tok( strLabels, delim );

    ctrl->Clear();
    while( tok.HasMoreTokens() )
    {
        ctrl->Append( tok.GetNextToken() );
    }

    ctrl->SetValidator( wxGenericValidator( &value ) );
}

void wxSimpleDialog::SetControlWithItemsOptions( wxControlWithItems *ctrl, int &value, ListControlOption *options )
{
    ctrl->Clear();
    for( int i = 0; options[i].name; i++ )
    {
        ctrl->Append( wxString(options[i].name));
    }

    ctrl->SetValidator(wxControlWithItemsValidator( ctrl, options, &value ));
}
