#ifndef WXDIALOGBASE_HPP
#define WXDIALOGBASE_HPP

#include "wx_includes.hpp"

// ListControlOption Really simple structure for defining a set of radio button labels and
// integer values.  This is supplied as array RadioOption *options to the .. or
// Add.. function.  In each case it is assumed that the lifetime of the array exceeds that
// of the objects using it...

struct ListControlOption
{
    const char *name;
    int value;
};

class wxOptionValidatorBase : public wxValidator
{
public:
    wxOptionValidatorBase( ListControlOption *options, int *value = NULL);
    wxOptionValidatorBase( const wxOptionValidatorBase &validator );
    virtual ~wxOptionValidatorBase();
    virtual bool TransferToWindow();
    virtual bool TransferFromWindow();
    virtual bool Validate( wxWindow *parent );
protected:
    virtual int GetSelectedIndex()=0;
    virtual void SetSelectedIndex( int i )=0;
private:
    int *value;
    int nOptions;
    int *optValues;
    ListControlOption *options;
};

class wxRadioBoxOptionValidator : public wxOptionValidatorBase
{
public:
    wxRadioBoxOptionValidator( ListControlOption *options, int *value = NULL) : wxOptionValidatorBase(options,value) {};
    wxRadioBoxOptionValidator( const wxRadioBoxOptionValidator &validator ) : wxOptionValidatorBase(validator) {};
    virtual wxValidator *Clone() const { return new wxRadioBoxOptionValidator( *this ); }
protected:
    virtual int GetSelectedIndex();
    virtual void SetSelectedIndex( int i );
};


class wxSimpleListBoxValidator : public wxValidator
{
public:
    wxSimpleListBoxValidator( char delimiter, wxString *value );
    wxSimpleListBoxValidator( const wxSimpleListBoxValidator &validator );

    virtual wxValidator *Clone() const { return new wxSimpleListBoxValidator( *this ); }
    virtual bool TransferToWindow();
    virtual bool TransferFromWindow();
    virtual bool Validate( wxWindow *parent );

private:
    wxString *value=0;
    char delimiter=0;
};

class wxFilePickerValidator : public wxValidator
{
public:
    wxFilePickerValidator( wxString *filename, const wxString &message = wxEmptyString );
    wxFilePickerValidator( const wxFilePickerValidator &validator );

    virtual wxValidator *Clone() const { return new wxFilePickerValidator( *this ); }
    virtual bool TransferToWindow();
    virtual bool TransferFromWindow();
    virtual bool Validate( wxWindow *parent );

private:
    wxString *filename;
    wxString message;
};

class wxSimpleDialog : public wxDialog
{
public:
    wxSimpleDialog( wxString title, long buttonFlags  = wxOK | wxCANCEL | wxHELP );
    virtual ~wxSimpleDialog();

    // Stuff for creating controls

    wxStaticText *Label( wxString text, wxWindow *window = 0 );
    wxTextCtrl *TextBox( wxString &string, int nch = 20, int nlines = 1, wxString validChar = wxEmptyString );
    wxTextCtrl *NumberBox( int &value, bool positive = true, int nch = 10 );
    wxTextCtrl *NumberBox( double &value, bool positive = true, int ndp = 2, int nch = 12 );

    // Options defined as ~value1~value2~... corresponding to 0,1,...
    wxRadioBox *RadioBox( int &value, wxString options, wxString label = wxEmptyString, bool horizontal = false );
    wxRadioBox *RadioBox( int &value, ListControlOption *options, wxString label = wxEmptyString, bool horizontal = false );
    wxListBox *ListBox( wxString &value, wxString options, int type, int width=-1, int height=-1 );
    wxChoice *DropDownBox( int &value, wxString options );
    wxChoice *DropDownBox( int &value, ListControlOption *options );
    wxCheckBox *CheckBox( wxString label, bool &value );
    wxFilePickerCtrl *OpenFileBox( wxString &filename, wxString label, wxString wildcard, wxString message = wxEmptyString );
    wxFilePickerCtrl *SaveFileBox( wxString &filename, wxString label, wxString wildcard, wxString message = wxEmptyString );
    wxButton *Button( wxString label, wxObjectEventFunction function );

    // Declare button function in dialog class as..
    // void MyFunc( wxCommandEvent &event );
    //
    // Build button with ..
    // Button("MyButton",wxCommandEventHandler( MyDialog::MyFunc ))

    // Stuff for adding controls in the constructor

    void AddSpacer();
    wxStaticText *AddLabel(wxString label, wxWindow *window = 0 );
    wxTextCtrl *AddTextBox(wxString label, wxString &string, int nch = 20, int nlines = 1, wxString validChar = wxEmptyString );
    wxListBox *AddListBox( wxString label, wxString &string, wxString options, int type, int width=-1, int height=-1 );
    wxTextCtrl *AddNumberBox(wxString label, int &value, bool positive = true, int nch = 10 );
    wxTextCtrl *AddNumberBox(wxString label, double &value, bool positive = true, int ndp = 2, int nch = 12 );
    wxRadioBox *AddRadioBox(wxString label, int &value, wxString options, bool horizontal = false  );
    wxRadioBox *AddRadioBox(wxString label, int &value, ListControlOption *options, bool horizontal = false );
    wxChoice *AddDropDownBox(wxString label, int &value, ListControlOption *options );
    wxChoice *AddDropDownBox(wxString label, int &value, wxString options );
    wxCheckBox *AddCheckBox(wxString label, bool &value );
    wxButton *AddButton( wxString label, wxObjectEventFunction function );
    wxFilePickerCtrl *AddOpenFileBox( wxString label, wxString &string, wxString wildcard, wxString message = wxEmptyString );
    wxFilePickerCtrl *AddSaveFileBox( wxString label, wxString &string, wxString wildcard, wxString message = wxEmptyString );
    void AddControl(wxString label, wxWindow *window );

    // Note: calls to these functions must be terminated with a null parameter
    // ie AddControls( ctrl1, ctrl2, ..., 0 );

    void AddControls( int nColumns, wxWindow *control, ... );
    void AddControl( wxWindow *control, const wxSizerFlags &flags );

    void AddSizer(wxSizer *sizer );
    void AddSizer(wxSizer *sizer, const wxSizerFlags &flags );

    // Must be called after Add... for layout etc to work
    void AddButtonsAndSize();

    // Call in the constructor for a sizeable dialog
    void MakeSizeable();

    // Add help
    void SetupHelp( wxHelpController *controller, wxString topic );

    // Run the dialog
    bool RunDialog();

    // Setting options...

    void SetListBoxOptions( wxListBox *lb, wxString &string, wxString options );
    void SetControlWithItemsOptions( wxControlWithItems *ctrl, int &value, wxString options );
    void SetControlWithItemsOptions( wxControlWithItems *ctrl, int &value, ListControlOption *options );

protected:
    // Overridable functions called in RunDialog. ReadData shown before
    // dialog displayed.
    // Apply called if result is wxID_OK

    virtual void ReadData();
    virtual void Apply();

private:
    void OnIdle( wxIdleEvent &event );
    void ShowHelp( wxCommandEvent &event );
    void ShowHelp( wxHelpEvent &event );
    wxWindow *ResetTabOrder( wxSizer *sizer, wxWindow *last );
    int nextId;
    wxSizer *sizer;
    int nSizer;
    wxSizerFlags sizerFlags;
    wxHelpController *help;
    wxString helpTopic;
    wxWindow *helpButton;
    wxWindow *lastWindow;
    long buttonFlags;

    DECLARE_EVENT_TABLE()
};


#endif
