#ifndef WXDIALOGBASE_HPP
#define WXDIALOGBASE_HPP

#include "wx_includes.hpp"

#include <string>
#include <vector>

// ListControlOption Really simple structure for defining a set of radio button labels and
// integer values.  This is supplied as a std::vector<ListControlOption> to the .. or
// Add.. function.  The validators copy the values, so the vector need not outlive the call.

struct ListControlOption
{
    std::string name;
    int value;
};

class wxOptionValidatorBase : public wxValidator
{
public:
    wxOptionValidatorBase( const std::vector<ListControlOption> &options, int *value = nullptr );
    wxOptionValidatorBase( const wxOptionValidatorBase &validator );
    bool TransferToWindow() override;
    bool TransferFromWindow() override;
    bool Validate( wxWindow *parent ) override;
protected:
    virtual int GetSelectedIndex()=0;
    virtual void SetSelectedIndex( int i )=0;
private:
    int *_value;
    std::vector<int> _optValues;
};

class wxRadioBoxOptionValidator : public wxOptionValidatorBase
{
public:
    wxRadioBoxOptionValidator( const std::vector<ListControlOption> &options, int *value = nullptr ) : wxOptionValidatorBase(options,value) {};
    wxRadioBoxOptionValidator( const wxRadioBoxOptionValidator &validator )  = default;
    [[nodiscard]] wxValidator *Clone() const override { return new wxRadioBoxOptionValidator( *this ); }
protected:
    int GetSelectedIndex() override;
    void SetSelectedIndex( int i ) override;
};


class wxSimpleListBoxValidator : public wxValidator
{
public:
    wxSimpleListBoxValidator( char delimiter, wxString *value );
    wxSimpleListBoxValidator( const wxSimpleListBoxValidator &validator );

    [[nodiscard]] wxValidator *Clone() const override { return new wxSimpleListBoxValidator( *this ); }
    bool TransferToWindow() override;
    bool TransferFromWindow() override;
    bool Validate( wxWindow *parent ) override;

private:
    wxString *value=nullptr;
    char delimiter=0;
};

class wxFilePickerValidator : public wxValidator
{
public:
    wxFilePickerValidator( wxString *filename, const wxString &message = wxEmptyString );
    wxFilePickerValidator( const wxFilePickerValidator &validator );

    [[nodiscard]] wxValidator *Clone() const override { return new wxFilePickerValidator( *this ); }
    bool TransferToWindow() override;
    bool TransferFromWindow() override;
    bool Validate( wxWindow *parent ) override;

private:
    wxString *filename;
    wxString message;
};

class wxSimpleDialog : public wxDialog
{
public:
    wxSimpleDialog( const wxString& title, long buttonFlags  = wxOK | wxCANCEL | wxHELP );
    ~wxSimpleDialog() override;

    // Stuff for creating controls

    wxStaticText *Label( const wxString& text, wxWindow *window = nullptr );
    wxTextCtrl *TextBox( wxString &string, int nch = 20, int nlines = 1, wxString validChar = wxEmptyString );
    wxTextCtrl *NumberBox( int &value, bool positive = true, int nch = 10 );
    wxTextCtrl *NumberBox( double &value, bool positive = true, int ndp = 2, int nch = 12 );

    // Options defined as ~value1~value2~... corresponding to 0,1,...
    wxRadioBox *RadioBox( int &value, wxString options, const wxString& label = wxEmptyString, bool horizontal = false );
    wxRadioBox *RadioBox( int &value, const std::vector<ListControlOption> &options, const wxString& label = wxEmptyString, bool horizontal = false );
    wxListBox *ListBox( wxString &value, wxString options, int type, int width=-1, int height=-1 );
    wxChoice *DropDownBox( int &value, wxString options );
    wxChoice *DropDownBox( int &value, const std::vector<ListControlOption> &options );
    wxCheckBox *CheckBox( const wxString& label, bool &value );
    wxFilePickerCtrl *OpenFileBox( wxString &filename, const wxString& label, const wxString& wildcard, const wxString& message = wxEmptyString );
    wxFilePickerCtrl *SaveFileBox( wxString &filename, const wxString& label, const wxString& wildcard, const wxString& message = wxEmptyString );
    wxButton *Button( const wxString& label, wxObjectEventFunction function );

    // Declare button function in dialog class as..
    // void MyFunc( wxCommandEvent &event );
    //
    // Build button with ..
    // Button("MyButton",wxCommandEventHandler( MyDialog::MyFunc ))

    // Stuff for adding controls in the constructor

    void AddSpacer();
    wxStaticText *AddLabel(wxString label, wxWindow *window = nullptr );
    wxTextCtrl *AddTextBox(wxString label, wxString &string, int nch = 20, int nlines = 1, wxString validChar = wxEmptyString );
    wxListBox *AddListBox( wxString label, wxString &string, wxString options, int type, int width=-1, int height=-1 );
    wxTextCtrl *AddNumberBox(wxString label, int &value, bool positive = true, int nch = 10 );
    wxTextCtrl *AddNumberBox(wxString label, double &value, bool positive = true, int ndp = 2, int nch = 12 );
    wxRadioBox *AddRadioBox(wxString label, int &value, wxString options, bool horizontal = false  );
    wxRadioBox *AddRadioBox(wxString label, int &value, const std::vector<ListControlOption> &options, bool horizontal = false );
    wxChoice *AddDropDownBox(wxString label, int &value, const std::vector<ListControlOption> &options );
    wxChoice *AddDropDownBox(wxString label, int &value, wxString options );
    wxCheckBox *AddCheckBox(wxString label, bool &value );
    wxButton *AddButton( wxString label, wxObjectEventFunction function );
    wxFilePickerCtrl *AddOpenFileBox( const wxString& label, wxString &string, wxString wildcard, wxString message = wxEmptyString );
    wxFilePickerCtrl *AddSaveFileBox( const wxString& label, wxString &string, wxString wildcard, wxString message = wxEmptyString );
    void AddControl(const wxString& label, wxWindow *window );

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
    void SetControlWithItemsOptions( wxControlWithItems *ctrl, int &value, const std::vector<ListControlOption> &options );

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
