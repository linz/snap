#ifndef WXDIALOGBASE_HPP
#define WXDIALOGBASE_HPP

#include "wx_includes.hpp"

// wxRadioBoxOption Really simple structure for defining a set of radio button labels and
// integer values.  This is supplied as array RadioOption *options to the .. or
// Add.. function.  In each case it is assumed that the lifetime of the array exceeds that
// of the objects using it...

struct wxRadioBoxOption
{
    char *name;
    int value;
};

class wxRadioBoxOptionValidator : public wxValidator
{
public:
    wxRadioBoxOptionValidator( wxRadioBoxOption *options, int *value = NULL);
    wxRadioBoxOptionValidator( const wxRadioBoxOptionValidator &validator );

    virtual wxValidator *Clone() const { return new wxRadioBoxOptionValidator( *this ); }
    virtual bool TransferToWindow();
    virtual bool TransferFromWindow();
    virtual bool Validate( wxWindow *parent );

private:
    int *value;
    wxRadioBoxOption *options;
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
    wxString *value;
    char delimiter;
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
    wxSimpleDialog( const char *title, long buttonFlags  = wxOK | wxCANCEL | wxHELP );
    virtual ~wxSimpleDialog();

    // Stuff for creating controls

    wxStaticText *Label( const char *text, wxWindow *window = NULL );
    wxTextCtrl *TextBox( wxString &string, int nch = 20, int nlines = 1, const char *validChar = NULL );
    wxTextCtrl *NumberBox( int &value, bool positive = true, int nch = 10 );
    wxTextCtrl *NumberBox( double &value, bool positive = true, int ndp = 2, int nch = 12 );

    // Options defined as ~value1~value2~... corresponding to 0,1,...
    wxRadioBox *RadioBox( int &value, const char *options, const char *label = 0, bool horizontal = false );
    wxRadioBox *RadioBox( int &value, wxRadioBoxOption *options, const char *label = 0, bool horizontal = false );
    wxListBox *ListBox( wxString &value, const char *options, int type, int width=-1, int height=-1 );
    wxChoice *DropDownBox( int &value, const char *options );
    wxChoice *DropDownBox( int &value, wxRadioBoxOption *options );
    wxCheckBox *CheckBox( const char *label, bool &value );
    wxFilePickerCtrl *OpenFileBox( wxString &filename, const char *label, const char *wildcard, const char *message = 0 );
    wxFilePickerCtrl *SaveFileBox( wxString &filename, const char *label, const char *wildcard, const char *message = 0 );
    wxButton *Button( const char *label, wxObjectEventFunction function );

    // Declare button function in dialog class as..
    // void MyFunc( wxCommandEvent &event );
    //
    // Build button with ..
    // Button("MyButton",wxCommandEventHandler( MyDialog::MyFunc ))

    // Stuff for adding controls in the constructor

    void AddSpacer();
    wxStaticText *AddLabel(const char *label, wxWindow *window = NULL);
    wxTextCtrl *AddTextBox(const char *label, wxString &string, int nch = 20, int nlines = 1, const char *validChar = NULL );
    wxListBox *AddListBox( const char *label, wxString &string, const char *options, int type, int width=-1, int height=-1 );
    wxTextCtrl *AddNumberBox(const char *label, int &value, bool positive = true, int nch = 10 );
    wxTextCtrl *AddNumberBox(const char *label, double &value, bool positive = true, int ndp = 2, int nch = 12 );
    wxRadioBox *AddRadioBox(const char *label, int &value, const char *options, bool horizontal = false  );
    wxRadioBox *AddRadioBox(const char *label, int &value, wxRadioBoxOption *options, bool horizontal = false );
    wxChoice *AddDropDownBox(const char *label, int &value, wxRadioBoxOption *options );
    wxChoice *AddDropDownBox(const char *label, int &value, const char *options );
    wxCheckBox *AddCheckBox(const char *label, bool &value );
    wxButton *AddButton( const char *label, wxObjectEventFunction function );
    wxFilePickerCtrl *AddOpenFileBox( const char *label, wxString &string, const char *wildcard, const char *message = 0 );
    wxFilePickerCtrl *AddSaveFileBox( const char *label, wxString &string, const char *wildcard, const char *message = 0 );
    void AddControl(const char *label, wxWindow *window );

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

    void SetListBoxOptions( wxListBox *lb, wxString &string, const char *options );
    void SetControlWithItemsOptions( wxControlWithItems *ctrl, int &value, const char *options );
    void SetControlWithItemsOptions( wxControlWithItems *ctrl, int &value, wxRadioBoxOption *options );

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
