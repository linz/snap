#include "snapconfig.h"
#include "wx_includes.hpp"
#include "wxsplashimage.hpp"
#include "snapplot_loadlog.hpp"

#ifdef __WXGTK__
#include "resources/snap16_icon.xpm"
#include "resources/snap32_icon.xpm"
#endif

class SnapplotLoadLogger : public wxLog
{
public:
    SnapplotLoadLogger( SnapplotLoadLog *logWindow );
    void DoLogText(const wxString &msg);
private:
    SnapplotLoadLog *logWindow;

};

SnapplotLoadLogger::SnapplotLoadLogger( SnapplotLoadLog *logWindow ) :
    logWindow( logWindow )
{
}

void SnapplotLoadLogger::DoLogText(const wxString &msg)
{
    // put the text into our window
    wxTextCtrl *pText = logWindow->logText;

    // remove selection (WriteText is in fact ReplaceSelection)
#ifdef __WXMSW__
    wxTextPos nLen = pText->GetLastPosition();
    pText->SetSelection(nLen, nLen);
#endif // Windows

    pText->AppendText(msg);
    pText->AppendText("\n");

    // TODO ensure that the line can be seen
    logWindow->Update();
}

/////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( SnapplotLoadLog, wxFrame )
    EVT_BUTTON( wxID_CLOSE, SnapplotLoadLog::OnCloseButton )
    EVT_CLOSE( SnapplotLoadLog::OnClose )
END_EVENT_TABLE()

SnapplotLoadLog::SnapplotLoadLog() :
    wxFrame( 0, wxID_ANY, "Loading Snapplot ... ", wxDefaultPosition, wxDefaultSize )
{
    logging = false;

    SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
    wxIconBundle icons;
    icons.AddIcon( wxIcon(wxICON(ICO_SNAP16)) );
    icons.AddIcon( wxIcon(wxICON(ICO_SNAP32)) );
    SetIcons( icons );

    wxSizerFlags flags1;
    flags1.Border();

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    /*
    wxBitmap bmpSplash(wxBITMAP(IDB_SPLASHSCREEN));
    sizer->Add( new wxSplashImage( this, bmpSplash ), flags1 );
    sizer->AddSpacer( GetCharHeight() );
    */


    logText = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                             wxSize( GetCharWidth()*80, GetCharHeight()*20 ),
                             wxTE_MULTILINE  |
                             wxHSCROLL       |
                             // needed for Win32 to avoid 65Kb limit but it doesn't work well
                             // when using RichEdit 2.0 which we always do in the Unicode build
#if !wxUSE_UNICODE
                             wxTE_RICH       |
#endif // !wxUSE_UNICODE
                             wxTE_READONLY);

    textLog = new SnapplotLoadLogger( this );

    wxSizerFlags flags2;
    flags2.Expand().Border().Proportion(1);
    sizer->Add( logText, flags2 );
    sizer->AddSpacer( GetCharHeight() );

    flags1.Right();
    closeButton = new wxButton( this, wxID_CLOSE );
    sizer->Add( closeButton, flags1 );

    SetSizerAndFit( sizer );
}

SnapplotLoadLog::~SnapplotLoadLog()
{
    EndLogging();
    delete textLog;
}

void SnapplotLoadLog::StartLogging()
{
    if( logging ) return;
    logging = true;
    closeButton->Enable( false );
    oldLog = wxLog::SetActiveTarget( textLog );
}

void SnapplotLoadLog::EndLogging()
{
    if( ! logging ) return;
    logging = false;
    closeButton->Enable( true );
    wxLog::SetActiveTarget( oldLog );
}

void SnapplotLoadLog::OnCloseButton( wxCommandEvent & WXUNUSED(event) )
{
    if( ! logging ) Close();
}

void SnapplotLoadLog::OnClose( wxCloseEvent &event )
{
    if( logging && event.CanVeto() )
    {
        event.Veto();
        return;
    }
    else
    {
        // Pass back ..
        event.Skip();
    }
}
