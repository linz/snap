#include "wx_includes.hpp"
#include "wxhelpabout.hpp"
#include "wxregexvalidator.hpp"
#include "wxsimpledialog.hpp"
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

//extern "C"
//{
#include "coordsys/coordsys.h"
#include "util/pi.h"
#include "util/dms.h"
#include "util/fileutil.h"
#include "util/errdef.h"
//}

// Custom events for the application

DECLARE_EVENT_TYPE(wxEVT_CONVERT_COORDS,-1)
DEFINE_EVENT_TYPE(wxEVT_CONVERT_COORDS)

DECLARE_EVENT_TYPE(wxEVT_CLEAR_RESULTS,-1)
DEFINE_EVENT_TYPE(wxEVT_CLEAR_RESULTS)

DECLARE_EVENT_TYPE(wxEVT_PREV_RESULT,-1)
DEFINE_EVENT_TYPE(wxEVT_PREV_RESULT)

DECLARE_EVENT_TYPE(wxEVT_NEXT_RESULT,-1)
DEFINE_EVENT_TYPE(wxEVT_NEXT_RESULT)

DECLARE_EVENT_TYPE(wxEVT_SHOW_MESSAGE,-1)
DEFINE_EVENT_TYPE(wxEVT_SHOW_MESSAGE)

DECLARE_EVENT_TYPE(wxEVT_SET_CONVERT_ID,-1)
DEFINE_EVENT_TYPE(wxEVT_SET_CONVERT_ID)

DECLARE_EVENT_TYPE(wxEVT_CLEAR_CONVERT_ID,-1)
DEFINE_EVENT_TYPE(wxEVT_CLEAR_CONVERT_ID)


#define IDM_ABOUT 10099 // System menu "About..." ID
#define IDM_HELP 10098  // System menu "Help..." ID
#define IDM_OPTIONS 10097 // System menu "Options..." ID
#define IDM_STAY_ON_TOP 10096 // System menu "Stay on top..." ID


// wxcoordControl - control for each coordinate

class wxCoordControl : public wxTextCtrl
{
public:
    wxCoordControl(wxWindow *parent, const wxString &regex = wxEmptyString, const wxString &help = wxEmptyString, int nchar = 20, const wxString &cardPfx = wxEmptyString );
    void UpdateValue( const wxString &value );
    void AllowLowerCase( bool value = true ) { forceUpper = !value; }
    void ResetFont( const wxFont &font );
    void ResetNChar( int nchar ) { ncharmax = nchar; }
    const wxString &CardPrefix() { return cardPrefix; }
protected:
private:
    static wxColour validColour;
    static wxColour invalidColour;

    bool updating;
    bool forceUpper;
    int ncharmax;
    wxString help;
    wxString cardPrefix;

    void OnSetFocus( wxFocusEvent &event );
    void OnChar( wxKeyEvent &event );
    void OnEnterKey( wxCommandEvent &event );
    void OnTextUpdated( wxCommandEvent &event );
    void Resize();

    void SendEvent( WXTYPE eventType );

    DECLARE_CLASS(wxCoordControl);
    DECLARE_EVENT_TABLE();
};

IMPLEMENT_CLASS(wxCoordControl, wxTextCtrl);

BEGIN_EVENT_TABLE( wxCoordControl, wxTextCtrl )
    EVT_SET_FOCUS(OnSetFocus )
    EVT_TEXT(wxID_ANY, OnTextUpdated)
    EVT_TEXT_ENTER( wxID_ANY, OnEnterKey )
    EVT_CHAR( OnChar )
END_EVENT_TABLE()


wxColor wxCoordControl::validColour = wxColour(255,255,255);
wxColor wxCoordControl::invalidColour = wxColor(255,200,200);

wxCoordControl::wxCoordControl(wxWindow *parent, const wxString &regex, const wxString &help, int nchar, const wxString &cardPfx ) :
    wxTextCtrl(
        parent,
        wxID_ANY,
        "",
        wxDefaultPosition,
        wxDefaultSize,
        wxTE_LEFT | wxTE_PROCESS_ENTER,
        wxRegexpStringValidator( 0, regex )
    ),
    help(help)
{
    updating = false;
    forceUpper = true;
    ncharmax = nchar;
    Resize();
    cardPrefix = cardPfx;
}


void wxCoordControl::Resize()
{
    SetSize(wxSize(GetCharWidth()*(ncharmax+1), (GetCharHeight()*5)/3 ));
    SetMinSize(wxSize(GetCharWidth()*(ncharmax+1), (GetCharHeight()*4)/3 ));
}

void wxCoordControl::ResetFont( const wxFont &font )
{
    SetFont( font );
    Resize();
}


void wxCoordControl::OnSetFocus( wxFocusEvent &event )
{
    SetSelection(-1,-1);
    SendEvent( wxEVT_SHOW_MESSAGE );
    if( GetValidator()->Validate(this) ) SendEvent( wxEVT_SET_CONVERT_ID );
    else SendEvent( wxEVT_CLEAR_CONVERT_ID );
}

void wxCoordControl::OnEnterKey( wxCommandEvent &event )
{
    if( GetValidator()->Validate(this))
    {
        SendEvent( wxEVT_CONVERT_COORDS );
        SetSelection(-1,-1);
    }
}

void wxCoordControl::OnChar( wxKeyEvent &event )
{
    wxUint32 code = event.GetKeyCode();
    switch( code )
    {
    case WXK_PAGEUP:
    case WXK_NUMPAD_PAGEUP: SendEvent(wxEVT_PREV_RESULT); return;
    case WXK_PAGEDOWN:
    case WXK_NUMPAD_PAGEDOWN:  SendEvent(wxEVT_NEXT_RESULT); return;
    }
    event.Skip();
}

void wxCoordControl::SendEvent( WXTYPE eventType )
{
    wxCommandEvent evt( eventType, GetId());
    evt.SetString( help );
    GetEventHandler()->ProcessEvent(evt);
}

void wxCoordControl::UpdateValue( const wxString &value )
{
    updating = true;
    SetValue( value );
    updating = false;
}

void wxCoordControl::OnTextUpdated( wxCommandEvent &event )
{
    wxString value = GetValue();
    if( forceUpper ) value = value.Upper();

    if( ! value.IsSameAs(GetValue()))
    {
        long ip = GetInsertionPoint();
        SetValue(value);
        SetInsertionPoint(ip);
        return;
    }

    bool isValid = GetValidator()->Validate(this);

    wxColor background;
    if( GetValue().IsEmpty() || isValid)
    {
        background = validColour;
    }
    else
    {
        background = invalidColour;
    }
    if( background != GetBackgroundColour()) { SetBackgroundColour(background); Refresh();}

    if( ! updating )
    {
        SendEvent( wxEVT_CLEAR_RESULTS);
        if( isValid ) SendEvent( wxEVT_SET_CONVERT_ID );
        else SendEvent( wxEVT_CLEAR_CONVERT_ID );
    }
}

/////////////////////////////////////////////////////////////////

class NZMapConvOptionsDialog;

class NZMapConvWindow : public wxDialog
{
public:
    friend class NZMapConvOptionsDialog;

    NZMapConvWindow(wxChar *image);
    virtual ~NZMapConvWindow();
    wxHelpController *HelpController() { return helpctl; }
    void SetupTest( const wxString &inputFile, const wxString &outputFile );

protected:
    virtual bool MSWTranslateMessage(WXMSG* pMsg);

private:
    wxCoordControl *nz260;
    wxCoordControl *nzmg;
    wxCoordControl *nzgd49;
    wxCoordControl *nztopo50;
    wxCoordControl *nztm;
    wxCoordControl *nzgd2k;

    wxCoordControl *coordCtls[6];

    wxStaticText *lblnz260;
    wxStaticText *lblnzmg;
    wxStaticText *lblnzgd49;
    wxStaticText *lblnztopo50;
    wxStaticText *lblnztm;
    wxStaticText *lblnzgd2k;

    wxStaticText *coordLabels[6];

    wxButton *convertButton;
    wxButton *nextButton;
    wxButton *prevButton;
    wxButton *optionsButton;
    wxButton *helpButton;
    // wxButton *closeButton;

    wxStaticText *help;

    wxFlexGridSizer *crd2ksizer;
    wxFlexGridSizer *crd49sizer;
    wxFlexGridSizer *buttonsizer;
    wxFlexGridSizer *crdPanelSizer;
    wxFlexGridSizer *mainSizer;

    wxHelpController *helpctl;

    wxFont coordFont;
    bool labelsAbove;
    bool verticalLayout;
    bool showMapRef;
    bool showProj;
    bool showLatLon;
    bool showButtons;
    bool showOptions;
    bool showCardOption;

    int convertId;
    void ClearResults( wxCommandEvent &event );
    void SetConvertId( wxCommandEvent &event );
    void ClearConvertId( wxCommandEvent &event );
    void ConvertCoords( wxCommandEvent &event );
    void ShowMessage( wxCommandEvent &event );
    void OnClose( wxCloseEvent &event );
    void OnChar( wxKeyEvent &event );
    void OnButton( wxCommandEvent &event );
    void OnActivate( wxActivateEvent &event );

    bool GetStayOnTop();
    void SetStayOnTop( bool set );
    void SetupLabels();
    void ResetLayout();
    void FitIntoDisplay();

    void ShowAboutBox();
    void ShowHelp();
    void ShowHelp(wxHelpEvent &event );
    void ChooseOptions();

    void GetBaseSettings();
    void GetWindowSettings();
    void SaveSettings();
    void SetupSystemMenu();
    void SetupIcons();
    bool SetupConversions(wxChar *image);
    void SetupOutputFormats();
    void DeleteOutputFormats();

    void SendEvent( WXTYPE eventType );

    bool Read_NZMS260();
    bool Read_NZMG();
    bool Read_NZGD49();
    bool Read_NZTopo50();
    bool Read_NZTM();
    bool Read_NZGD2000();


    bool ReadMapRef( const wxString value );
    bool ConvertNZMS260ToNZMG();
    bool ConvertNZTopo50ToNZTM();
    bool ReadProj( const wxString value );
    bool ReadLatLon( const wxString value, double *crd );

    void ShowConvertedCoords();
    void WriteLatLonCoords( wxCoordControl *ctl, double *crd );
    void WriteProjCoords( wxCoordControl *ctl );
    void WriteNZMS260Coords();
    void WriteNZTopo50Coords();
    void WriteMapRefCoords(wxCoordControl *ctl);

    void ShowError( const wxString &error );

    void SaveResult();
    void ShowResult( int nResult );
    void ShowPrevResult( wxCommandEvent &event );
    void ShowNextResult( wxCommandEvent &event );

    void RunTests();

    double mape, mapn;
    char mapletters[3];
    long mapnumber;

    coord_conversion cv_to_nzmg;
    coord_conversion cv_from_nzmg;
    coord_conversion cv_to_nztm;
    coord_conversion cv_from_nztm;
    coord_conversion cv_to_nzgd49;
    coord_conversion cv_from_nzgd49;

    double crd2k[3];
    double crdconv[3];

    int ll_ndp;
    int ll_fmt; // 2 = deg, 1 = deg/min, 0 = deg/min/sec, 3 = CARD format
    bool ll_en; // order of coords

    int prj_ndp;
    bool prj_en;
    int prj_fmt;  // 0 = plain crds, 1 = CARD format projec.

    bool mapref8; // true for 8 digit map references

    void *dmsf_lat;
    void *dmsf_lon;

    wxArrayString savedResults;
    int savedResultsPointer;
    int maxSavedResults;

    wxString inputTestFile;
    wxString outputTestFile;

    HMENU hSystemMenu;

    DECLARE_EVENT_TABLE();
};

class NZMapConvOptionsDialog : public wxSimpleDialog
{
public:
    NZMapConvOptionsDialog( NZMapConvWindow *conv );

protected:
    virtual void Apply();
private:
    NZMapConvWindow *conv;
    wxFont proposedFont;
    wxStaticText *fontDescription;
    int ref8;
    int pen;
    int len;
    bool stayOnTop;
    wxSpinCtrl *ll_ndp_spinner;
    wxSpinCtrl *prj_ndp_spinner;
    wxString FontDescription();
    void SelectFont( wxCommandEvent &event );
};

BEGIN_EVENT_TABLE( NZMapConvWindow, wxDialog )
    EVT_COMMAND(wxID_ANY,wxEVT_CONVERT_COORDS, ConvertCoords )
    EVT_COMMAND(wxID_ANY,wxEVT_CLEAR_RESULTS, ClearResults )
    EVT_COMMAND(wxID_ANY,wxEVT_SHOW_MESSAGE, ShowMessage )
    EVT_COMMAND(wxID_ANY,wxEVT_PREV_RESULT, ShowPrevResult )
    EVT_COMMAND(wxID_ANY,wxEVT_NEXT_RESULT, ShowNextResult )
    EVT_COMMAND(wxID_ANY,wxEVT_SET_CONVERT_ID, SetConvertId )
    EVT_COMMAND(wxID_ANY,wxEVT_CLEAR_CONVERT_ID, ClearConvertId )
    EVT_BUTTON(wxID_ANY,OnButton)
    EVT_HELP(wxID_ANY,ShowHelp)
    EVT_CHAR(OnChar)
    EVT_ACTIVATE(OnActivate)
    EVT_CLOSE(OnClose)
END_EVENT_TABLE()

NZMapConvWindow::NZMapConvWindow(wxChar *image)
    : wxDialog (
        NULL,
        wxID_ANY,
        _T("NZ Map Reference Converter"),
        wxDefaultPosition,
        wxDefaultSize,
        wxSYSTEM_MENU | wxCLOSE_BOX | wxCAPTION
    )
{
    helpctl = 0;
    convertId = 0;
    dmsf_lat = 0;
    dmsf_lon = 0;

    ll_ndp = 0;
    ll_fmt = 0;
    ll_en = false;
    prj_ndp=0;
    prj_fmt=0;
    prj_en=true;
    mapref8 = false;
    dmsf_lat = 0;
    dmsf_lon = 0;
    savedResultsPointer = 0;
    maxSavedResults = 100;

    labelsAbove = true;
    verticalLayout = false;
    showMapRef = true;
    showProj = true;
    showLatLon = true;
    showButtons = true;
    showOptions = true;
    showCardOption = false;

    GetBaseSettings();

    SetupSystemMenu();
    SetupIcons();

    if( ! SetupConversions(image) )
    {
        Close();
    }

    SetupOutputFormats();


    char *latLonValidationString =
        "~-0-9SEse() .,~"
        "^\\s*("
        "(S?\\s*|-)[34]\\d(\\.\\d+)?\\s*S?(\\s*\\,\\s*|\\s+)(E?\\s*)1[67]\\d(\\.\\d+)?\\s*E?"
        "|(S?\\s*|-)[34]\\d\\s+[0-5]?\\d(\\.\\d+)?\\s*S?(\\s*\\,\\s*|\\s+)(E?\\s*)1[67]\\d\\s+[0-5]?\\d(\\.\\d+)?\\s*E?"
        "|(S?\\s*|-)[34]\\d\\s+[0-5]?\\d\\s+[0-5]?\\d(\\.\\d+)?\\s*S?(\\s*\\,\\s*|\\s+)(E?\\s*)1[67]\\d\\s+[0-5]?\\d\\s+[0-5]?\\d(\\.\\d+)?\\s*E?"
        "|(E?\\s*)1[67]\\d(\\.\\d+)?\\s*E?(\\s*\\,\\s*|\\s+)(S?\\s*)[34]\\d(\\.\\d+)?\\s*S?"
        "|(E?\\s*)1[67]\\d\\s+[0-5]?\\d(\\.\\d+)?\\s*E?(\\s*\\,\\s*|\\s+)(S?\\s*)[34]\\d\\s+[0-5]?\\d(\\.\\d+)?\\s*S?"
        "|(E?\\s*)1[67]\\d\\s+[0-5]?\\d\\s+[0-5]?\\d(\\.\\d+)?\\s*E?(\\s*\\,\\s*|\\s+)(S?\\s*)[34]\\d\\s+[0-5]?\\d\\s+[0-5]?\\d(\\.\\d+)?\\s*S?"
        "|LL2?\\(\\s*1\\d\\d(?:\\.\\d+)?\\s*\\,\\s*\\-\\d\\d(?:\\.\\d+)?\\s*\\)"
        ")\\s*$"
        "~silent"
        ;
    lblnz260 = coordLabels[0] = new wxStaticText(this,-1,wxT("&A. NZ260 map reference:"));
    nz260 = coordCtls[0] = new wxCoordControl(this,
            "~a-zA-Z0-9 ~^\\s*[A-Z][0-5]\\d\\s*(\\d{2}\\s*\\d{2}|\\d{3}\\s*\\d{3}|\\d{4}\\s*\\d{4})\\s*$~silent",
            "Enter NZMS260 map reference, eg R27 714010");

    lblnzmg = coordLabels[1] = new wxStaticText(this,wxID_ANY,"");

    nzmg = coordCtls[1] = new wxCoordControl(this,
            "~0-9mMeEnN.,() ~"
            "^\\s*("
            "(?:[eE]\\s*)?[1-3]\\d{6}(?:\\.\\d+)?(?:\\s*[mM]?[eE])?(\\s*\\,\\s*|\\s+)(?:[nN]\\s*)?[4-7]\\d{6}(?:\\.\\d+)?(?:\\s*[mM]?[nN])?"
            "|(?:[nN]\\s*)?[4-7]\\d{6}(?:\\.\\d+)?(?:\\s*[mM]?[nN])?(\\s*\\,\\s*|\\s+)(?:[eE]\\s*)?[1-3]\\d{6}(?:\\.\\d+)?(?:\\s*[mM]?[eE])?"
            "|EN2?\\(\\s*[1-3]\\d{6}(?:\\.\\d+)?\\s*\\,\\s*[4-7]\\d{6}(?:\\.\\d+)?\\s*\\)"
            ")\\s*$"
            "~silent",
            "Enter NZMG easting and northing, eg 2671411 6001003",
            32,
            "EN2");
    nzmg->AllowLowerCase();
    lblnzgd49 = coordLabels[2] = new wxStaticText(this,wxID_ANY,"");
    nzgd49 = coordCtls[2] = new wxCoordControl(this,latLonValidationString,
            "Enter NZGD1949 latitude and longitude eg 41 11 00S 174 55 27E.  Can be deg min sec, deg min, or deg",
            32,
            "LL2");
    lblnztopo50 = coordLabels[3] = new wxStaticText(this,-1,wxT("&D. NZ Topo50 map reference:"));
    nztopo50 = coordCtls[3] = new wxCoordControl(this,
            "~a-zA-Z0-9 ~^\\s*(A[S-Z]|B[A-HJ-NP-Z]|C[A-HJK])[0-4]\\d\\s*(\\d{2}\\s*\\d{2}|\\d{3}\\s*\\d{3}|\\d{4}\\s*\\d{4})\\s*$~silent",
            "Enter NZ Topo50 map reference, eg BQ32 614 393"
                                                );
    lblnztm = coordLabels[4] = new wxStaticText(this,wxID_ANY,"");
    nztm = coordCtls[4]=new wxCoordControl(this,
                                           "~0-9mMeEnN.,() ~"
                                           "^\\s*("
                                           "(?:[eE]\\s*)?[12]\\d{6}(?:\\.\\d+)?(?:\\s*[mM]?[eE])?(\\s*\\,\\s*|\\s+)(?:[nN]\\s*)?[4-6]\\d{6}(?:\\.\\d+)?(?:\\s*[mM]?[nN])?"
                                           "|(?:[nN]\\s*)?[4-6]\\d{6}(?:\\.\\d+)?(?:\\s*[mM]?[nN])?(\\s*\\,\\s*|\\s+)(?:[eE]\\s*)?[12]\\d{6}(?:\\.\\d+)?(?:\\s*[mM]?[eE])?"
                                           "|EN2?\\(\\s*[12]\\d{6}(?:\\.\\d+)?\\s*\\,\\s*[4-6]\\d{6}(?:\\.\\d+)?\\s*\\)"
                                           ")\\s*$~silent",
                                           "Enter NZTM2000 (Transverse Mercator) easting and northing, eg 1761390 5439289",
                                           32,
                                           "EN");
    nztm->AllowLowerCase();
    lblnzgd2k = coordLabels[5] = new wxStaticText(this,wxID_ANY,"");
    nzgd2k = coordCtls[5] = new wxCoordControl(this,latLonValidationString,
            "Enter NZGD2000 latitude and longitude eg 41 10 54S 174 55 27E.  Can be deg min sec, deg min, or deg",
            32,
            "LL");
    SetupLabels();

    coordFont = nzmg->GetFont();

    convertButton = new wxButton(this,wxID_ANY,wxT("Convert"));
    convertButton->SetDefault();
    convertButton->Enable(false);

    prevButton = new wxButton(this,wxID_ANY,wxT("&Previous"));
    prevButton->Enable(false);

    nextButton = new wxButton(this,wxID_ANY,wxT("&Next"));
    nextButton->Enable(false);

    optionsButton = 0;
    if( showOptions )
    {
        optionsButton = new wxButton(this,wxID_ANY,wxT("&Options..."));
    }

    helpButton = new wxButton(this,wxID_ANY,wxT("&Help..."));

    // closeButton = new wxButton(this,wxID_ANY,wxT("Close"));

    help = new wxStaticText(this,wxID_ANY,"");

    mainSizer = new wxFlexGridSizer(1);
    crdPanelSizer = new wxFlexGridSizer(4);
    crd49sizer = new wxFlexGridSizer(2);
    crd2ksizer = new wxFlexGridSizer(2);
    buttonsizer = new wxFlexGridSizer(1);

    wxSizerFlags flags;
    flags.Border();

    crd49sizer->Add( lblnz260,flags );
    crd49sizer->Add( nz260,flags );
    crd49sizer->Add( lblnzmg,flags );
    crd49sizer->Add( nzmg,flags );
    crd49sizer->Add( lblnzgd49,flags );
    crd49sizer->Add( nzgd49,flags );

    crd2ksizer->Add(lblnztopo50,flags);
    crd2ksizer->Add(nztopo50,flags);
    crd2ksizer->Add(lblnztm,flags);
    crd2ksizer->Add(nztm,flags);
    crd2ksizer->Add(lblnzgd2k,flags);
    crd2ksizer->Add(nzgd2k,flags);

    buttonsizer->Add(convertButton,wxSizerFlags().Expand());
    buttonsizer->Add(prevButton,wxSizerFlags().Expand());
    buttonsizer->Add(nextButton,wxSizerFlags().Expand());
    if( showOptions ) buttonsizer->Add(optionsButton,wxSizerFlags().Expand());
    buttonsizer->Add(helpButton,wxSizerFlags().Expand());
    // buttonsizer->Add(closeButton,wxSizerFlags().Expand());

    crdPanelSizer->Add(crd49sizer);
    crdPanelSizer->Add(crd2ksizer);
    crdPanelSizer->AddSpacer(20);
    crdPanelSizer->Add(buttonsizer);

    mainSizer->Add(crdPanelSizer);
    mainSizer->Add(help,wxSizerFlags(0).Border().Expand());

    SetSizerAndFit(mainSizer);

    GetWindowSettings();
    ResetLayout();

    helpctl = new wxHelpController( this );

    wxFileName helpFile(wxStandardPaths::Get().GetExecutablePath());
    helpFile.SetName(_T("nzmapconv"));
    helpctl->Initialize( helpFile.GetFullPath() );

}

NZMapConvWindow::~NZMapConvWindow()
{
    delete helpctl;
    DeleteOutputFormats();
}

void NZMapConvWindow::SetupLabels()
{
    if( prj_en )
    {
        lblnzmg->SetLabel("&B. NZMG coordinates (east/north):");
        lblnztm->SetLabel("&E. NZTM2000 coordinates (east/north):");
    }
    else
    {
        lblnzmg->SetLabel("&B. NZMG coordinates (north/east):");
        lblnztm->SetLabel("&E. NZTM2000 coordinates (north/east):");
    }
    if( ll_en )
    {
        lblnzgd49->SetLabel("&C. NZGD49 (lon/lat):");
        lblnzgd2k->SetLabel("&F. NZGD2000/WGS84 lon/lat:");
    }
    else
    {
        lblnzgd49->SetLabel("&C. NZGD49 (lat/lon):");
        lblnzgd2k->SetLabel("&F. NZGD2000/WGS84 lat/lon:");
    }

    wxChar labelChar = 'A';
    for( int i = 0; i < 6; i++ )
    {
        if( coordLabels[i]->IsShown())
        {
            wxString s = coordLabels[i]->GetLabel();
            s[1] = labelChar;
            coordLabels[i]->SetLabel(s);
            labelChar++;
        }
    }
}

void NZMapConvWindow::ResetLayout()
{
    help->SetLabel(wxT(""));
    crd2ksizer->SetCols( labelsAbove ? 1 : 2 );
    crd49sizer->SetCols( labelsAbove ? 1 : 2 );
    buttonsizer->SetCols( verticalLayout ? (labelsAbove ? 3 : 6): 1);
    crdPanelSizer->SetCols( verticalLayout ? 1 : 4);

    wxSizerFlags flags;
    flags.Border();
    wxSizer *addSizer = verticalLayout ? crd49sizer : crd2ksizer;
    for( int i=3; i<6; i++ )
    {
        crd49sizer->Detach(coordLabels[i]);
        crd2ksizer->Detach(coordLabels[i]);
        addSizer->Add(coordLabels[i],flags);
        crd49sizer->Detach(coordCtls[i]);
        crd2ksizer->Detach(coordCtls[i]);
        addSizer->Add(coordCtls[i],flags);
    }

    nz260->Show(showMapRef);
    lblnz260->Show(showMapRef);
    nztopo50->Show(showMapRef);
    lblnztopo50->Show(showMapRef);

    nzmg->Show(showProj);
    lblnzmg->Show(showProj);
    nztm->Show(showProj);
    lblnztm->Show(showProj);

    nzgd49->Show(showLatLon);
    lblnzgd49->Show(showLatLon);
    nzgd2k->Show(showLatLon);
    lblnzgd2k->Show(showLatLon);

    int ncharmax = 32;
    if( showProj )
    {
        int ncharcrd = 30+2*prj_ndp;
        if( ncharcrd > ncharmax ) ncharmax = ncharcrd;
    }
    if( showLatLon )
    {
        int dms = ll_fmt;
        if( dms > 2 ) dms = 2;
        int ncharcrd = 17+6*(2-dms)+2*ll_ndp;
        if( ncharcrd > ncharmax ) ncharmax = ncharcrd;
    }

    nzmg->ResetNChar(ncharmax);
    nzgd49->ResetNChar(ncharmax);
    nztm->ResetNChar(ncharmax);
    nzgd2k->ResetNChar(ncharmax);

    SetupLabels();

    crdPanelSizer->Show(buttonsizer,showButtons);

    for( int i = 0; i < 6; i++ ) coordCtls[i]->ResetFont( coordFont );

    Layout();
    GetSizer()->SetSizeHints(this);
    FitIntoDisplay();
}

void NZMapConvWindow::FitIntoDisplay()
{
    int displayId = wxDisplay::GetFromWindow(this);
    if( displayId == wxNOT_FOUND ) displayId = 0;
    wxRect r = wxDisplay(displayId).GetClientArea();

    r.SetHeight( r.GetHeight()-GetRect().GetHeight());
    r.SetWidth( r.GetWidth()-GetRect().GetWidth());

    int x, y;
    GetPosition(&x,&y);

    if( x < r.GetLeft() || x > r.GetRight() || y < r.GetTop() || y > r.GetBottom())
    {
        if( x > r.GetRight()) x = r.GetRight();
        if( x < r.GetLeft()) x = r.GetLeft();
        if( y > r.GetBottom()) y = r.GetBottom();
        if( y < r.GetTop()) y = r.GetTop();
        SetPosition(wxPoint(x,y));
    }
}

void NZMapConvWindow::SetupSystemMenu()
{
    hSystemMenu = ::GetSystemMenu((HWND__ *) GetHWND(),FALSE);
    ::AppendMenu(hSystemMenu,MF_SEPARATOR,0,"");
    ::AppendMenu(hSystemMenu,MF_STRING,IDM_STAY_ON_TOP,"Stay on top");
    if( showOptions )
    {
        ::AppendMenu(hSystemMenu,MF_STRING,IDM_OPTIONS,"Options ...\tAlt+O");
    }
    ::AppendMenu(hSystemMenu,MF_STRING,IDM_HELP,"Help\tF1");
    ::AppendMenu(hSystemMenu,MF_STRING,IDM_ABOUT,"About...");
    ::DrawMenuBar((HWND__ *) GetHWND());
}


void NZMapConvWindow::SetupIcons()
{
    wxIconBundle icons;
    icons.AddIcon( wxIcon(wxICON(ICO_NZMAPCONV16)) );
    icons.AddIcon( wxIcon(wxICON(ICO_NZMAPCONV32)) );
    SetIcons( icons );
}

bool NZMapConvWindow::MSWTranslateMessage(WXMSG* pMsg)
{
    if( pMsg->message == WM_SYSCOMMAND )
    {
        switch( pMsg->wParam )
        {
        case IDM_STAY_ON_TOP: SetStayOnTop( ! GetStayOnTop()); return true;
        case IDM_OPTIONS: ChooseOptions(); return true;
        case IDM_ABOUT: ShowAboutBox(); return true;
        case IDM_HELP: ShowHelp(); return true;
        }
    }
    return wxDialog::MSWTranslateMessage(pMsg);
}

bool NZMapConvWindow::GetStayOnTop()
{
    return (GetWindowStyleFlag() & wxSTAY_ON_TOP) != 0;
}

void NZMapConvWindow::SetStayOnTop( bool set )
{
    long style = GetWindowStyleFlag();
    style &= ~ wxSTAY_ON_TOP;
    if( set ) style |= wxSTAY_ON_TOP;
    SetWindowStyleFlag( style );

    UINT checked = set ? MF_CHECKED : MF_UNCHECKED;
    ::CheckMenuItem( hSystemMenu, IDM_STAY_ON_TOP, MF_BYCOMMAND | checked );
}

void NZMapConvWindow::ShowAboutBox()
{
    ShowHelpAbout();
}

void NZMapConvWindow::ShowHelp()
{
    helpctl->DisplayContents();
}

void NZMapConvWindow::ShowHelp( wxHelpEvent & WXUNUSED(event) )
{
    ShowHelp();
}


void NZMapConvWindow::OnClose( wxCloseEvent &event )
{
    SaveSettings();
    Destroy();
}

void NZMapConvWindow::OnChar( wxKeyEvent &event )
{
    wxUint32 code = event.GetKeyCode();
    switch( code )
    {
    case WXK_PAGEUP:
    case WXK_NUMPAD_PAGEUP: SendEvent(wxEVT_PREV_RESULT); return;
    case WXK_PAGEDOWN:
    case WXK_NUMPAD_PAGEDOWN:  SendEvent(wxEVT_NEXT_RESULT); return;
    }
    event.Skip();
}

void NZMapConvWindow::OnButton( wxCommandEvent &event )
{
    if( event.GetId() == convertButton->GetId())
    {
        SendEvent( wxEVT_CONVERT_COORDS );
    }
    else if( event.GetId() == nextButton->GetId())
    {
        SendEvent( wxEVT_NEXT_RESULT );
    }
    else if( event.GetId() == prevButton->GetId())
    {
        SendEvent( wxEVT_PREV_RESULT );
    }
    else if( optionsButton && event.GetId() == optionsButton->GetId())
    {
        ChooseOptions();
    }
    else if( event.GetId() == helpButton->GetId())
    {
        ShowHelp();
    }
    /*else if( event.GetId() == closeButton->GetId())
    {
    	Close();
    }*/
}

void NZMapConvWindow::ClearResults( wxCommandEvent &event )
{
    wxWindowList children = GetChildren();
    for( size_t i = 0; i < children.GetCount(); i++)
    {
        wxWindow *w = children.Item(i)->GetData();
        if( w->GetId() == event.GetId()) continue;
        if( w->IsKindOf(CLASSINFO(wxCoordControl)))
        {
            wxCoordControl *cc = (wxCoordControl *)w;
            cc->UpdateValue(wxEmptyString);
        }
    }
    ShowError( event.GetString());
    savedResultsPointer = savedResults.Count();
    nextButton->Enable(false);
    prevButton->Enable( savedResults.Count() > 0 );
}

void NZMapConvWindow::ShowMessage( wxCommandEvent &event )
{
    wxString message = event.GetString();
    ShowError( message);
}

void NZMapConvWindow::ShowError( const wxString &error )
{
    wxString message = error;
    if(verticalLayout && labelsAbove )
    {
        int pos = message.Find(" eg ");
        if( pos != wxNOT_FOUND )
        {
            message = message.Mid(pos+1);
        }
    }
    help->SetLabel( message );
    help->Refresh();
}

bool NZMapConvWindow::SetupConversions(wxChar *image)
{
    bool result = true;
    install_default_crdsys_file();

    coordsys *cnzmg = load_coordsys("NZMG");
    if( ! cnzmg )
    {
        result = false;
        ::wxMessageBox( "Cannot load NZMG coordinate system", "Coordinate system error", wxICON_EXCLAMATION | wxOK );
    }

    coordsys *cnztm = load_coordsys("NZTM");
    if( ! cnztm )
    {
        result = false;
        ::wxMessageBox( "Cannot load NZTM coordinate system", "Coordinate system error", wxICON_EXCLAMATION | wxOK );
    }

    coordsys *cnzgd49 = load_coordsys("NZGD1949");
    if( ! cnzgd49 )
    {
        result = false;
        ::wxMessageBox( "Cannot load NZDG1949 coordinate system", "Coordinate system error", wxICON_EXCLAMATION | wxOK );
    }

    coordsys *cnzgd2000 = load_coordsys("NZGD2000");
    if( ! cnzgd2000 )
    {
        result = false;
        ::wxMessageBox( "Cannot load NZDG2000 coordinate system", "Coordinate system error", wxICON_EXCLAMATION | wxOK );
    }


    if( 0 != define_coord_conversion( &cv_to_nzmg, cnzgd2000, cnzmg ) ||
            0 != define_coord_conversion( &cv_from_nzmg, cnzmg, cnzgd2000 ) )
    {
        ::wxMessageBox( "Cannot convert NZMG to NZDG2000", "Coordinate system error", wxICON_EXCLAMATION | wxOK );
        result = false;
    }

    if( 0 != define_coord_conversion( &cv_to_nztm, cnzgd2000, cnztm ) ||
            0 != define_coord_conversion( &cv_from_nztm, cnztm, cnzgd2000 ) )
    {
        ::wxMessageBox( "Cannot convert NZTM to NZDG2000", "Coordinate system error", wxICON_EXCLAMATION | wxOK );
        result = false;
    }

    if( 0 != define_coord_conversion( &cv_to_nzgd49, cnzgd2000, cnzgd49 ) ||
            0 != define_coord_conversion( &cv_from_nzgd49, cnzgd49, cnzgd2000 ) )
    {
        ::wxMessageBox( "Cannot convert NZGD1949 to NZDG2000", "Coordinate system error", wxICON_EXCLAMATION | wxOK );
        result = false;
    }

    return result;
}

void NZMapConvWindow::DeleteOutputFormats()
{
    if( dmsf_lat )  { delete_dms_format(dmsf_lat); dmsf_lat = 0; }
    if( dmsf_lon )  { delete_dms_format(dmsf_lon); dmsf_lon = 0; }
}

void NZMapConvWindow::SetupOutputFormats()
{
    DeleteOutputFormats();
    if( prj_fmt == 1 ) prj_en = true;
    if( ll_fmt == 3 ) ll_en=true;
    int flags = DMSF_FMT_INPUT_RADIANS;
    if( ll_fmt == 0 ) flags |= DMSF_FMT_DMS;
    if( ll_fmt == 1 ) flags |= DMSF_FMT_DM;
    if( ll_fmt == 2 ) flags |= DMSF_FMT_DEG;
    if( ll_fmt == 3 ) flags |= DMSF_FMT_DEG;
    dmsf_lat = create_dms_format(2,ll_ndp,flags,0,0,0,"N","S");
    dmsf_lon = create_dms_format(3,ll_ndp,flags,0,0,0,"E","W");
}

void NZMapConvWindow::SetConvertId( wxCommandEvent &event )
{
    convertId = event.GetId();
    convertButton->Enable(true);
}

void NZMapConvWindow::ClearConvertId( wxCommandEvent &event )
{
    convertId = 0;
    convertButton->Enable(false);
}

void NZMapConvWindow::ConvertCoords( wxCommandEvent &event )
{
    if( ! convertId ) return;
    event.SetId( convertId );
    if( ! dmsf_lat ) SetupOutputFormats();
    ClearResults( event );
    bool result = false;
    if( convertId == nz260->GetId()) { result = Read_NZMS260(); }
    if( convertId == nzmg->GetId()) { result = Read_NZMG(); }
    if( convertId == nzgd49->GetId()) { result = Read_NZGD49(); }
    if( convertId == nztopo50->GetId()) { result = Read_NZTopo50(); }
    if( convertId == nztm->GetId()) { result = Read_NZTM(); }
    if( convertId == nzgd2k->GetId()) { result = Read_NZGD2000(); }
    if( ! result )
    {
        ShowError("Input coordinate/reference is not valid");
    }
    else
    {
        ShowConvertedCoords();
        SaveResult();
        convertButton->Enable(false);
    }
}

bool NZMapConvWindow::Read_NZMS260()
{
    if( ! ReadMapRef(nz260->GetValue()) ) return false;
    if( ! ConvertNZMS260ToNZMG() ) return false;
    if( convert_coords(&cv_from_nzmg,crdconv,0,crd2k,0) != OK) return false;
    return true;
}

bool NZMapConvWindow::Read_NZMG()
{
    if( ! ReadProj( nzmg->GetValue() ) ) return false;
    if( convert_coords(&cv_from_nzmg,crdconv,0,crd2k,0) != OK ) return false;
    return true;
}

bool NZMapConvWindow::Read_NZGD49()
{
    if( ! ReadLatLon( nzgd49->GetValue(),crdconv) ) return false;
    if( convert_coords(&cv_from_nzgd49,crdconv,0,crd2k,0) != OK ) return false;
    return true;
}

bool NZMapConvWindow::Read_NZTopo50()
{
    if( ! ReadMapRef(nztopo50->GetValue()) ) return false;
    if( ! ConvertNZTopo50ToNZTM() ) return false;
    if( convert_coords(&cv_from_nztm,crdconv,0,crd2k,0) != OK ) return false;
    return true;
}

bool NZMapConvWindow::Read_NZTM()
{
    if( ! ReadProj( nztm->GetValue() )) return false;
    if( convert_coords(&cv_from_nztm,crdconv,0,crd2k,0) != OK ) return false;
    return true;
}

bool NZMapConvWindow::Read_NZGD2000()
{
    return ReadLatLon( nzgd2k->GetValue(),crd2k);
}

bool NZMapConvWindow::ReadMapRef( const wxString value )
{
    bool result = false;
    static wxRegEx re("^\\s*([A-Z]{1,2})(\\d\\d)\\s*(?:(\\d\\d)\\s*(\\d\\d)|(\\d\\d\\d)\\s*(\\d\\d\\d)|(\\d\\d\\d\\d)\\s*(\\d\\d\\d\\d))\\s*$",wxRE_ADVANCED);
    if( re.Matches(value))
    {
        wxString match;
        match = re.GetMatch( value, 1);
        mapletters[0] = match.GetChar(0);
        mapletters[1] = 0;
        if( match.Len() > 1 ) mapletters[1] = match.GetChar(1);
        re.GetMatch( value, 2).ToLong(&mapnumber);
        if( re.GetMatch(value,3).Len() == 2 )
        {
            re.GetMatch( value, 3 ).ToDouble(&mape);
            re.GetMatch( value, 4 ).ToDouble(&mapn);
            mape *= 1000; mapn *= 1000;
        }
        else if( re.GetMatch(value,5).Len() == 3 )
        {
            re.GetMatch( value, 5 ).ToDouble(&mape);
            re.GetMatch( value, 6 ).ToDouble(&mapn);
            mape *= 100; mapn *= 100;
        }
        else
        {
            re.GetMatch( value, 7 ).ToDouble(&mape);
            re.GetMatch( value, 8 ).ToDouble(&mapn);
            mape *= 10; mapn *= 10;
        }
        result = true;
    }
    return result;
}

bool NZMapConvWindow::ConvertNZMS260ToNZMG()
{

    double n0 = 6790000.0+15000.0-30000.0*mapnumber;
    double e0 = 1970000.0+40000.0*(mapletters[0]-65)+20000.0;
    double ed = mape;
    double nd = mapn;

    n0 = nd + int((n0-nd)/100000+0.5)*100000;
    e0 = ed + int((e0-ed)/100000+0.5)*100000;

    crdconv[0] = e0;
    crdconv[1] = n0;
    crdconv[2] = 0.0;

    return true;
}

bool NZMapConvWindow::ConvertNZTopo50ToNZTM()
{
    int nsheet = mapletters[1] - 65;
    if( nsheet >= 15 ) nsheet--;
    if( nsheet >= 9 ) nsheet--;
    nsheet += (mapletters[0]-65)*24-16;
    double n0 = (6234000-18000) - 36000*nsheet;
    double e0 = 1084000+24000*(mapnumber-4)+12000;
    double ed = mape;
    double nd = mapn;
    n0 = int((n0 - nd)/100000+0.5)*100000 + nd;
    e0 = int((e0 - ed)/100000+0.5)*100000 + ed;

    crdconv[0] = e0;
    crdconv[1] = n0;
    crdconv[2] = 0.0;

    return true;
}

bool NZMapConvWindow::ReadProj( const wxString value )
{
    bool result = false;
    static wxRegEx re("^\\s*(?:[eEnN]\\s*)?(\\d{7}(?:\\.\\d+)?)(?:\\s*[mM]?[eEnN])?(?:\\s*\\,\\s*|\\s+)(?:[eEnN]\\s*)?(\\d{7}(?:\\.\\d+)?)(?:\\s*[mM]?[eEnN])?\\s*",wxRE_ADVANCED);
    static wxRegEx recard("^\\s*EN2?\\(\\s*(\\d{7}(?:\\.\\d+)?)\\s*\\,\\s*(\\d{7}(?:\\.\\d+)?)\\s*\\)\\s*$",wxRE_ADVANCED);
    if( re.Matches(value))
    {
        double c1, c2;
        re.GetMatch(value,1).ToDouble(&c1);
        re.GetMatch(value,2).ToDouble(&c2);
        if( c2 < c1 ) { double tmp = c1; c1 = c2; c2 = tmp; }
        crdconv[CRD_EAST] = c1;
        crdconv[CRD_NORTH] = c2;
        crdconv[2] = 0.0;
        result = true;
    }
    else if( recard.Matches(value))
    {
        double c1, c2;
        recard.GetMatch(value,1).ToDouble(&c1);
        recard.GetMatch(value,2).ToDouble(&c2);
        crdconv[CRD_EAST] = c1;
        crdconv[CRD_NORTH] = c2;
        crdconv[2] = 0.0;
        result = true;
    }
    return result;
}

bool NZMapConvWindow::ReadLatLon( const wxString value, double *crd )
{
    static wxRegEx red("^\\s*(?:[ES]?\\s*|-)(\\d+(?:\\.\\d+)?)\\s*[ES]?(?:\\s*\\,\\s*|\\s+)(?:[ES]?\\s*)(\\d+(?:\\.\\d+)?)\\s*[ES]?\\s*$",wxRE_ADVANCED);
    static wxRegEx redm("^\\s*(?:[ES]?\\s*|-)(\\d+)\\s+([0-5]?\\d(?:\\.\\d+)?)\\s*[ES]?(?:\\s*\\,\\s*|\\s+)(?:[ES]?\\s*|\\-)(\\d+)\\s+([0-5]?\\d(?:\\.\\d+)?)\\s*[ES]?\\s*$",wxRE_ADVANCED);
    static wxRegEx redms("^\\s*(?:[ES]?\\s*|-)(\\d+)\\s+([0-5]?\\d)\\s+([0-5]?\\d(?:\\.\\d+)?)\\s*[ES]?(?:\\s*\\,\\s*|\\s+)(?:[ES]?\\s*)(\\d+)\\s+([0-5]?\\d)\\s+([0-5]?\\d(?:\\.\\d+)?)\\s*[ES]?\\s*$",wxRE_ADVANCED);
    static wxRegEx recard("^\\s*LL2?\\(\\s*(1\\d\\d(?:\\.\\d+)?)\\s*\\,\\s*\\-(\\d\\d(?:\\.\\d+)?)\\s*\\)\\s*$",wxRE_ADVANCED);

    double d1 = 0, d2 = 0, m1 = 0, m2 = 0, s1 = 0, s2 = 0, c1, c2;
    bool result = true;
    if( red.Matches(value))
    {
        red.GetMatch(value,1).ToDouble(&c1);
        red.GetMatch(value,2).ToDouble(&c2);
    }
    else if( redm.Matches(value))
    {
        redm.GetMatch(value,1).ToDouble(&d1);
        redm.GetMatch(value,2).ToDouble(&m1);
        redm.GetMatch(value,3).ToDouble(&d2);
        redm.GetMatch(value,4).ToDouble(&m2);
        c1 = d1 + m1/60.0;
        c2 = d2 + m2/60.0;
    }
    else if( redms.Matches(value))
    {
        redms.GetMatch(value,1).ToDouble(&d1);
        redms.GetMatch(value,2).ToDouble(&m1);
        redms.GetMatch(value,3).ToDouble(&s1);
        redms.GetMatch(value,4).ToDouble(&d2);
        redms.GetMatch(value,5).ToDouble(&m2);
        redms.GetMatch(value,6).ToDouble(&s2);
        c1 = d1 + m1/60.0 + s1/3600.0;
        c2 = d2 + m2/60.0 + s2/3600.0;
    }
    else if ( recard.Matches(value))
    {
        recard.GetMatch(value,1).ToDouble(&c1);
        recard.GetMatch(value,2).ToDouble(&c2);
    }
    else
    {
        result = false;
    }
    if( ! result  ) return false;

    if( c2 > c1 ) { double tmp = c1; c1 = c2; c2 = tmp; }
    c2 = -c2;
    crd[0] = c1*DTOR;
    crd[1] = c2*DTOR;
    crd[2] = 0.0;

    return true;
}

void NZMapConvWindow::ShowConvertedCoords()
{
    int sourceId = convertId;
    // sourceId = 0;
    if( sourceId != nzgd2k->GetId()) WriteLatLonCoords( nzgd2k, crd2k );
    if( sourceId != nzgd49->GetId() && convert_coords( &cv_to_nzgd49, crd2k, 0, crdconv, 0 ) == OK )
    {
        WriteLatLonCoords( nzgd49, crdconv );
    }
    if( convert_coords( &cv_to_nzmg, crd2k, 0, crdconv, 0 ) == OK )
    {
        if( sourceId != nzmg->GetId()) WriteProjCoords( nzmg );
        if( sourceId != nz260->GetId()) WriteNZMS260Coords();
    }
    if( convert_coords( &cv_to_nztm, crd2k, 0, crdconv, 0 ) == OK )
    {
        if( sourceId != nztm->GetId()) WriteProjCoords( nztm );
        if( sourceId != nztopo50->GetId()) WriteNZTopo50Coords();
    }
}

void NZMapConvWindow::WriteLatLonCoords( wxCoordControl *ctl, double *crd )
{
    char lat[64], lon[64];
    wxString ll("");
    if( ll_fmt == 3 )
    {
        sprintf(lat,"%.*lf",ll_ndp,crd[CRD_LAT]*RTOD);
        sprintf(lon,"%.*lf",ll_ndp,crd[CRD_LON]*RTOD);
        ll.Append(ctl->CardPrefix());
        ll.Append("(");
        ll.Append(lon);
        ll.Append(",");
        ll.Append(lat);
        ll.Append(")");
    }
    else
    {
        dms_string(crd[CRD_LAT],dmsf_lat,lat);
        dms_string(crd[CRD_LON],dmsf_lon,lon);
        if( ll_en )
        {
            ll.Append( lon );
            ll.Append( "    " );
            ll.Append( lat );
        }
        else
        {
            ll.Append( lat );
            ll.Append( "    " );
            ll.Append( lon );
        }
    }
    ctl->UpdateValue( ll );
}

void NZMapConvWindow::WriteProjCoords( wxCoordControl *ctl )
{
    char e[20], n[20];
    sprintf(e,"%.*lf",prj_ndp,crdconv[CRD_EAST]);
    sprintf(n,"%.*lf",prj_ndp,crdconv[CRD_NORTH]);
    wxString prj("");
    if( prj_fmt )
    {
        prj.Append(ctl->CardPrefix());
        prj.Append("(");
        prj.Append(e);
        prj.Append(",");
        prj.Append(n);
        prj.Append(")");
    }
    else if( prj_en )
    {
        prj.Append( e );
        prj.Append( " mE    " );
        prj.Append( n );
        prj.Append( " mN" );
    }
    else
    {
        prj.Append( n );
        prj.Append( " mN   " );
        prj.Append( e );
        prj.Append( " mE" );
    }
    ctl->UpdateValue( prj );
}

void NZMapConvWindow::WriteNZMS260Coords()
{
    double e = crdconv[CRD_EAST];
    double n = crdconv[CRD_NORTH];
    if( n > 6790000 || n < 5290000 || e < 1970000 || e > 3010000 ) return;
    int ns = int( (6790000-n)/30000 ); if( ns > 50 ) ns = 50;
    mapnumber = ns+1;

    int es = int( (e-1970000)/40000 ); if( es > 25 ) es = 25;
    mapletters[0] = 65+es; mapletters[1] = 0;

    mapn = n;
    mape = e;

    WriteMapRefCoords( nz260 );
}

void NZMapConvWindow::WriteNZTopo50Coords()
{
    char *sheets = "AS AT AU AV AW AX AY AZ BA BB BC BD BE BF BG BH BJ BK BL BM BN BP BQ BR BS BT BU BV BW BX BY BZ CA CB CC CD CE CF CG CH CJ CK";

    double e = crdconv[CRD_EAST];
    double n = crdconv[CRD_NORTH];

    if( n > 6234000 || n < 4722000 || e < 1084000 || e > 2108000 ) return;

    int ns = int( (6234000-n)/36000 ); if( ns > 41 ) ns = 41;
    int es = int( (e-1084000)/24000 ); if( es > 43 ) es = 43;
    strncpy(mapletters,sheets+ns*3,2);
    mapletters[2] = 0;
    mapnumber = int(es+4);

    mapn = n;
    mape = e;

    WriteMapRefCoords( nztopo50 );
}

void NZMapConvWindow::WriteMapRefCoords(wxCoordControl *ctl)
{
    char mapref[20];
    if( mapref8 )
    {
        sprintf(mapref,"%s%02d %04d %04d",mapletters,mapnumber,int((mape+5)/10)%10000,int((mapn+5)/10)%10000);
    }
    else
    {
        sprintf(mapref,"%s%02d %03d %03d",mapletters,mapnumber,int((mape+50)/100)%1000,int((mapn+50)/100)%1000);
    }
    ctl->UpdateValue(mapref);
}

void NZMapConvWindow::SaveResult()
{
    wxString results;
    results.Append(nz260->GetValue());
    results.Append("~");
    results.Append(nzmg->GetValue());
    results.Append("~");
    results.Append(nzgd49->GetValue());
    results.Append("~");
    results.Append(nztopo50->GetValue());
    results.Append("~");
    results.Append(nztm->GetValue());
    results.Append("~");
    results.Append(nzgd2k->GetValue());

    savedResults.Add(results);
    if( ((int)savedResults.Count()) > maxSavedResults ) savedResults.RemoveAt(0);
    savedResultsPointer = savedResults.Count()-1;
    nextButton->Enable(false);
    prevButton->Enable(savedResults.Count() > 1);
}

void NZMapConvWindow::ShowResult( int nResult )
{
    if( nResult < 0 || nResult >= ((int)savedResults.Count())) return;
    wxStringTokenizer tok;
    tok.SetString( savedResults.Item(nResult), "~" );
    nz260->UpdateValue(tok.GetNextToken());
    nzmg->UpdateValue(tok.GetNextToken());
    nzgd49->UpdateValue(tok.GetNextToken());
    nztopo50->UpdateValue(tok.GetNextToken());
    nztm->UpdateValue(tok.GetNextToken());
    nzgd2k->UpdateValue(tok.GetNextToken());

    savedResultsPointer = nResult;
    nextButton->Enable(nResult < ((int)savedResults.Count())-1);
    prevButton->Enable(nResult > 0);
}

void NZMapConvWindow::ShowPrevResult(wxCommandEvent &event)
{
    ShowResult( savedResultsPointer-1 );
}

void NZMapConvWindow::ShowNextResult(wxCommandEvent &event)
{
    ShowResult(savedResultsPointer+1);
}

void NZMapConvWindow::SendEvent( WXTYPE eventType )
{
    wxCommandEvent evt( eventType, GetId());
    GetEventHandler()->ProcessEvent(evt);
}

void NZMapConvWindow::ChooseOptions()
{
    NZMapConvOptionsDialog dlg(this);

    if( showOptions && dlg.RunDialog())
    {
        SetupOutputFormats();
        ResetLayout();
    }
}


void NZMapConvWindow::SaveSettings()
{
    wxConfig *cfg = new wxConfig("NZMapConv","LINZ");
    cfg->SetPath("/Settings");
    cfg->Write("LatLonFormat",ll_fmt);
    cfg->Write("LatLonOrder",ll_en);
    cfg->Write("LatLonDecimalPlaces",ll_ndp);
    cfg->Write("ProjectionFormat",prj_fmt);
    cfg->Write("ProjectionOrder",prj_en);
    cfg->Write("ProjectionDecimalPlaces",prj_ndp);
    cfg->Write("MapReference8Digigs",mapref8);
    cfg->Write("StayOnTop",GetStayOnTop());
    cfg->Write("CoordLabelsAbove",labelsAbove);
    cfg->Write("VerticalLayout",verticalLayout);
    cfg->Write("ShowMapRef",showMapRef);
    cfg->Write("ShowProjCoords",showProj);
    cfg->Write("ShowLatLonCoords",showLatLon);
    cfg->Write("ShowButtons",showButtons);
    cfg->Write("ShowOptions",showOptions);
    cfg->Write("ShowCardOption",showCardOption);
    wxString fontInfo = coordFont.GetNativeFontInfoDesc();
    cfg->Write("CoordFont",fontInfo);
    wxPoint pos = GetPosition();
    cfg->Write("PositionX",pos.x);
    cfg->Write("PositionY",pos.y);
    delete cfg;
}

void NZMapConvWindow::GetBaseSettings()
{
    wxConfig *cfg = new wxConfig("NZMapConv","LINZ");
    cfg->SetPath("/Settings");
    cfg->Read("LatLonFormat",&ll_fmt);
    cfg->Read("LatLonOrder",&ll_en);
    cfg->Read("LatLonDecimalPlaces",&ll_ndp);
    cfg->Read("ProjectionFormat",&prj_fmt);
    cfg->Read("ProjectionOrder",&prj_en);
    cfg->Read("ProjectionDecimalPlaces",&prj_ndp);
    cfg->Read("MapReference8Digigs",&mapref8);
    cfg->Read("CoordLabelsAbove",&labelsAbove);
    cfg->Read("VerticalLayout",&verticalLayout);
    cfg->Read("ShowMapRef",&showMapRef);
    cfg->Read("ShowProjCoords",&showProj);
    cfg->Read("ShowLatLonCoords",&showLatLon);
    cfg->Read("ShowButtons",&showButtons);
    cfg->Read("ShowOptions",&showOptions);
    cfg->Read("ShowCardOption",&showCardOption);
}

void NZMapConvWindow::GetWindowSettings()
{
    bool ontop;
    wxConfig *cfg = new wxConfig("NZMapConv","LINZ");
    cfg->SetPath("/Settings");
    wxString fontInfo;
    if( cfg->Read("CoordFont",&fontInfo))
    {
        wxFont proposedFont;
        if( proposedFont.SetNativeFontInfo(fontInfo)) coordFont = proposedFont;
    }
    int x, y;
    cfg->Read("PositionX",&x);
    cfg->Read("PositionY",&y);

    DeleteOutputFormats();

    SetPosition(wxPoint(x, y));

    cfg->Read("StayOnTop",&ontop);
    SetStayOnTop( ontop );
    SetupLabels();
    ResetLayout();

    FitIntoDisplay();

    delete cfg;
}

void NZMapConvWindow::SetupTest( const wxString &inputFile, const wxString &outputFile )
{
    inputTestFile = inputFile;
    outputTestFile = outputFile;
}

void NZMapConvWindow::OnActivate( wxActivateEvent &event )
{
    if( ! inputTestFile.IsEmpty() && ! outputTestFile.IsEmpty() )
    {
        RunTests();
    }
}

void NZMapConvWindow::RunTests()
{
    wxFileOutputStream output( outputTestFile );
    if( ! output.IsOk() ) return;
    wxTextOutputStream os(output);

    wxFileInputStream input( inputTestFile );
    if( ! input.IsOk() )
    {
        os << "Unable to open test input file " << inputTestFile << endl;
        return;
    }
    inputTestFile="";
    outputTestFile="";
    wxTextInputStream is(input);

    while( ! input.Eof() )
    {
        wxCoordControl *ctl = 0;
        wxString data = is.ReadLine();

        if( data.Len() <= 0 ) continue;
        switch( data[0] )
        {
        case 'A': ctl = nz260; break;
        case 'B': ctl = nzmg; break;
        case 'C': ctl = nzgd49; break;
        case 'D': ctl = nztopo50; break;
        case 'E': ctl = nztm; break;
        case 'F': ctl = nzgd2k; break;
        default: continue;
        }

        ctl->SetValue( data.Mid(1) );

        wxCommandEvent evt;
        ConvertCoords(evt);

        os << data[0] << "|";
        for( int i = 0; i < 6; i++ )
        {
            os << coordCtls[i]->GetValue() << "|";
        }
        os << help->GetLabel() << endl;
    }
    Close();
}


NZMapConvOptionsDialog::NZMapConvOptionsDialog( NZMapConvWindow *conv ) :
    wxSimpleDialog("NZ Map Reference Converter Options"),
    conv(conv)
{
    ref8 = conv->mapref8 ? 1 : 0;
    pen = conv->prj_en ? 0 : 1;
    len = conv->ll_en ? 1 : 0;
    proposedFont = conv->coordFont;
    stayOnTop = conv->GetStayOnTop();

    wxSizerFlags right = wxSizerFlags().Right();
    wxFlexGridSizer *sizer1 = new wxFlexGridSizer(2,2,10);
    sizer1->Add( Label("&Map reference coordinates"));
    sizer1->AddSpacer(5);
    sizer1->Add( Label(wxT("Type:")),right);
    sizer1->Add( RadioBox(ref8,"~6 digit reference~8 digit reference",0,true));
    sizer1->AddSpacer(5);
    sizer1->AddSpacer(5);
    sizer1->Add( Label("&Projection coordinates"));
    sizer1->AddSpacer(0);
    if( conv->showCardOption )
    {
        sizer1->Add( Label(wxT("Format:")),right);
        sizer1->Add( RadioBox(conv->prj_fmt,"~EN coordinates~CARD",0,true));
    }
    sizer1->Add( Label(wxT("Order:")),right);
    sizer1->Add( RadioBox(pen,"~Easting,Northing~Northing,Easting",0,true));
    sizer1->Add( Label("Decimal places:"),right);
    prj_ndp_spinner = new wxSpinCtrl(this);
    prj_ndp_spinner->SetRange(0,3);
    prj_ndp_spinner->SetValue(conv->prj_ndp);
    sizer1->Add(prj_ndp_spinner);
    sizer1->AddSpacer(5);
    sizer1->AddSpacer(5);

    wxFlexGridSizer *sizer2 = sizer1; // new wxFlexGridSizer(2,10,10);
    sizer2->Add( Label("&Latitude/longitude coordinates"));
    sizer2->AddSpacer(0);
    sizer2->Add( Label(wxT("Format:")),right);
    if( conv->showCardOption )
    {
        sizer2->Add( RadioBox(conv->ll_fmt,"~deg/min/sec~deg/min~degrees~CARD",0,true));
    }
    else
    {
        sizer2->Add( RadioBox(conv->ll_fmt,"~deg/min/sec~deg/min~degrees",0,true));
    }
    sizer2->Add( Label("Order:"),right);
    sizer2->Add( RadioBox(len,"~Latitude,Longitude~Longitude,Latitude",0,true));
    sizer2->Add( Label("Decimal places:"),right);
    ll_ndp_spinner = new wxSpinCtrl(this);
    ll_ndp_spinner->SetRange(0,8);
    ll_ndp_spinner->SetValue(conv->ll_ndp);
    sizer2->Add(ll_ndp_spinner);

    sizer2->AddSpacer(5);
    sizer2->AddSpacer(5);
    sizer2->Add( Label(wxT("Coordinate font")));
    wxFlexGridSizer *bs = new wxFlexGridSizer(3);
    bs->Add(Button("Select...",wxCommandEventHandler(NZMapConvOptionsDialog::SelectFont)));
    fontDescription = Label(FontDescription());
    bs->AddSpacer(20);
    bs->Add(fontDescription);
    sizer2->Add(bs);

    sizer2->Add(Label(wxT("Labels above coordinates")));
    sizer2->Add(CheckBox(0,conv->labelsAbove));
    sizer2->Add(Label(wxT("Vertical layout")));
    sizer2->Add(CheckBox(0,conv->verticalLayout));
    sizer2->Add(Label("Coordinate types"));

    wxFlexGridSizer *bs2 = new wxFlexGridSizer(10);
    bs2->Add(CheckBox("Map refs",conv->showMapRef));
    bs2->Add(CheckBox("Projection",conv->showProj));
    bs2->Add(CheckBox("Lat/Lon",conv->showLatLon));
    sizer2->Add(bs2);
    sizer2->Add(Label(wxT("Show buttons")));
    sizer2->Add(CheckBox(0,conv->showButtons));

    sizer2->Add( Label(wxT("Keep converter on top")));
    sizer2->Add( CheckBox(0,stayOnTop) );

    //wxFlexGridSizer *sizer3 = new wxFlexGridSizer(2,0,20);
    //sizer3->Add(sizer1);
    //sizer3->Add(sizer2);

    AddSizer(sizer1);

    SetupHelp(conv->HelpController(),wxT("options.html"));
    AddButtonsAndSize();
}

void NZMapConvOptionsDialog::SelectFont( wxCommandEvent &event )
{
    wxFont newFont = ::wxGetFontFromUser(this,proposedFont,"Coordinate font");
    if( newFont.IsOk() )
    {
        proposedFont = newFont;
        if( fontDescription ) fontDescription->SetLabel(FontDescription());
    }
}

wxString NZMapConvOptionsDialog::FontDescription()
{
    return proposedFont.GetNativeFontInfoUserDesc();
}

void NZMapConvOptionsDialog::Apply()
{
    conv->mapref8 = ref8 == 1;
    conv->prj_en = pen == 0;
    conv->ll_en = len == 1;
    conv->prj_ndp = prj_ndp_spinner->GetValue();
    conv->ll_ndp = ll_ndp_spinner->GetValue();
    conv->coordFont = proposedFont;
    conv->SetStayOnTop( stayOnTop );
}

// ////////////////////////////////////////////////////

class NZMapConvApp: public wxApp
{
    virtual bool OnInit();
};

IMPLEMENT_APP(NZMapConvApp)


bool NZMapConvApp::OnInit()
{
    NZMapConvWindow *top = new NZMapConvWindow(argv[0]);
    if( argc > 2 ) top->SetupTest( argv[1], argv[2] );
    top->Show( true );
    SetTopWindow( top);
    return true;
}
