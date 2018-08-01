#define GETVERSION_SET_PROGRAM_DATE
#include "snapconfig.h"
#include "wx_includes.hpp"
#include "wx/fs_zip.h"
#include "wxhelpabout.hpp"
#include "snapjob.hpp"
#include "snap_scriptenv.hpp"
#include "util/getversion.h"

#ifdef __WXGTK__
#include "resources/snap16_icon.xpm"
#include "resources/snap32_icon.xpm"
#endif

enum
{
    CMD_FILE_CLOSE = 1,
    CMD_HELP_HELP,
    CMD_HELP_ABOUT,
};

class wxLogPlainTextCtrl : public wxLog
{
public:
    wxLogPlainTextCtrl( wxTextCtrl *ctrl ) : txtctl( ctrl ) {}
protected:
    virtual void DoLog(wxLogLevel WXUNUSED(level), const wxChar *szmsg, time_t WXUNUSED(timestamp) )
    {
        wxString msg;
        msg << szmsg << "\n";
        txtctl->AppendText( msg );
    }
private:
    wxTextCtrl *txtctl;
    DECLARE_NO_COPY_CLASS(wxLogPlainTextCtrl)
};

class SnapMgrFrame : public wxFrame
{
public:
    SnapMgrFrame( const wxString &jobfile );
    ~SnapMgrFrame();
private:
    void SetupIcons();
    void SetupMenu();
    void SetupWindows();
    void ClearLog();

    void OnFileHistory( wxCommandEvent &event );
    void OnCmdClose( wxCommandEvent &event );
    void OnCmdHelpHelp( wxCommandEvent &event );
    void OnCmdHelpAbout( wxCommandEvent &event );
    void OnActivate( wxActivateEvent &event );

    void OnJobUpdated( wxCommandEvent &event );
    void OnClearLog( wxCommandEvent &event );

    void OnClose( wxCloseEvent &event );

    SnapMgrScriptEnv *scriptenv;
    wxConfig *config;
    wxFileHistory fileHistory;
    wxHelpController *help;
    wxTextCtrl *logCtrl;
    wxLogPlainTextCtrl *logger;
    int nScriptMenuItems;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE( SnapMgrFrame, wxFrame )
    EVT_COMMAND( wxID_ANY, wxEVT_SNAP_JOBUPDATED, SnapMgrFrame::OnJobUpdated )
    EVT_COMMAND( wxID_ANY, wxEVT_SNAP_CLEARLOG, SnapMgrFrame::OnClearLog )
    EVT_MENU( CMD_FILE_CLOSE, SnapMgrFrame::OnCmdClose )
    EVT_MENU( CMD_HELP_HELP, SnapMgrFrame::OnCmdHelpHelp )
    EVT_MENU( CMD_HELP_ABOUT, SnapMgrFrame::OnCmdHelpAbout )
    EVT_MENU_RANGE( wxID_FILE1, wxID_FILE9, SnapMgrFrame::OnFileHistory )
    EVT_ACTIVATE( SnapMgrFrame::OnActivate )
    EVT_CLOSE( SnapMgrFrame::OnClose )
END_EVENT_TABLE()

SnapMgrFrame::SnapMgrFrame( const wxString &jobfile ) :
    wxFrame(NULL, wxID_ANY, "SNAP")
{
    nScriptMenuItems = 0;
    logger = 0;
    logCtrl = 0;

    // Setup the main logging window ..

    CreateStatusBar();

    SetupWindows();
    logger = new wxLogPlainTextCtrl( logCtrl );
    wxLog::SetActiveTarget( logger );

    // Get the configuration information
    // and set it as the global configuration (accessible with Get)
    // and cleared with 

    config = new wxConfig("SnapMgr","LINZ");
    wxConfigBase::Set(config);

    // Restore the previous working directory

    wxString curDir;
    if( config->Read( "WorkingDirectory", &curDir ) )
    {
        wxSetWorkingDirectory( curDir );
    }

    SetupIcons();

    // SetupMenu must be called before loading the scripting environment

    SetupMenu();

    // Note: wxFileHistory.Load must be called after setting up menu so that the menu is populated ...

    config->SetPath( "/History" );
    fileHistory.Load( *config );

    for( int i=fileHistory.GetCount(); i--; )
    {
        wxString filename=fileHistory.GetHistoryFile(i);
        if( ! wxFileName(filename).FileExists() )
        {
            fileHistory.RemoveFileFromHistory(i);
        }
    }

    // Set up the help file

    help = new wxHelpController( this );

    wxFileName helpFile(wxStandardPaths::Get().GetExecutablePath());
    helpFile.SetName("snaphelp");
    help->Initialize( helpFile.GetFullPath() );

    // Load the scripting environment

    scriptenv = new SnapMgrScriptEnv(this);

    if( ! jobfile.IsEmpty() ) scriptenv->LoadJob( jobfile );
}

SnapMgrFrame::~SnapMgrFrame()
{
    wxLog::SetActiveTarget(0);
    delete logger;

    delete help;

    config->SetPath( "/History" );
    fileHistory.Save( *config );
    config->SetPath( "/" );
    config->Write("WorkingDirectory",wxGetCwd());

    delete scriptenv;
}

void SnapMgrFrame::SetupIcons()
{
    wxIconBundle icons;
    icons.AddIcon( wxIcon(wxICON(ICO_SNAP16)) );
    icons.AddIcon( wxIcon(wxICON(ICO_SNAP32)) );
    SetIcons( icons );
}

void SnapMgrFrame::SetupMenu()
{

    // Set up the file menu ..

    wxMenuBar *menuBar = new wxMenuBar;

    wxMenu *fileMenu = new wxMenu;
    menuBar->Append( fileMenu, "&File" );

    // Add items at the end of the file menu, add the help menu, and install the menu
    // bar...

    fileMenu->AppendSeparator();
    fileMenu->Append(CMD_FILE_CLOSE,
                     "&Close\tAlt-F4",
                     "Quit SNAP");

    fileHistory.UseMenu( fileMenu );


    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append( CMD_HELP_HELP,
                      "&Help\tF1",
                      "Get help about snapplot");

    helpMenu->AppendSeparator();
    helpMenu->Append( CMD_HELP_ABOUT,
                      "&About",
                      "Information about this version snaplot program");

    menuBar->Append( helpMenu, "&Help" );

    SetMenuBar(menuBar);
}


void SnapMgrFrame::SetupWindows()
{
    SetBackgroundColour( wxColour("WHITE"));
    logCtrl = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxHSCROLL | wxTE_RICH | wxTE_READONLY );
    SetClientSize( wxSize( GetCharWidth()*120, GetCharHeight()*40));
    /*
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    wxSizerFlags flags;
    flags.Expand();
    sizer->Add( logCtrl, flags );
    SetSizerAndFit( sizer );
    */
}

void SnapMgrFrame::ClearLog()
{
    logCtrl->Clear();
}

void SnapMgrFrame::OnFileHistory( wxCommandEvent &event )
{
    wxString jobFile(fileHistory.GetHistoryFile(event.GetId() - wxID_FILE1));
    if( ! jobFile.IsEmpty() )
    {
        scriptenv->LoadJob( jobFile );
    }
}

void SnapMgrFrame::OnJobUpdated( wxCommandEvent &event )
{
    if( event.GetInt() == SNAP_JOBLOADING )
    {
        ClearLog();
        wxLogMessage("Loading job ... ");
        return;
    }

    SnapJob *job = scriptenv->Job();
    if( job )
    {
        wxString jobfile= job->GetFullFilename();
        fileHistory.AddFileToHistory( jobfile );
        wxString label("SNAP - ");
        label.Append( scriptenv->Job()->GetFilename() );
        SetLabel( label );

        if( event.GetInt() & SNAP_JOBUPDATED_NEWJOB )
        {
            ClearLog();
            wxLogMessage( "Job location: %s", job->GetPath());
            wxLogMessage( "Command file: %s", job->GetFilename());
            if( job->IsOk())
            {
                wxLogMessage( "Job title:    %s", job->Title());
                wxLogMessage( "Coordinate file: %s", job->CoordinateFilename());
            }
            else
            {
                for( size_t i = 0; i < job->Errors().Count(); i++ )
                {
                    wxLogMessage( "%s\n", job->Errors()[i] );
                }
            }
        }
    }
    else
    {
        SetLabel("SNAP");
    }

}

void SnapMgrFrame::OnCmdClose( wxCommandEvent & WXUNUSED(event) )
{
    Close();
}

void SnapMgrFrame::OnCmdHelpHelp( wxCommandEvent & WXUNUSED(event) )
{
    help->DisplayContents();
}

void SnapMgrFrame::OnClearLog( wxCommandEvent & WXUNUSED(event) )
{
    ClearLog();
}

void SnapMgrFrame::OnCmdHelpAbout( wxCommandEvent & WXUNUSED(event) )
{
    ShowHelpAbout(PROGRAM_NAME,PROGRAM_VERSION,PROGRAM_DATE);
}

void SnapMgrFrame::OnActivate( wxActivateEvent & WXUNUSED(event) )
{
    scriptenv->UpdateJob();
}

void SnapMgrFrame::OnClose( wxCloseEvent &event )
{
    if( ! scriptenv->UnloadJob( event.CanVeto()) )
    {
        event.Veto();
    }
    else
    {
        Destroy();
    }
}

////////////////////////////////////////////////////////////////

class SnapMgrApp : public wxApp
{
public:
    virtual bool OnInit();
};

IMPLEMENT_APP( SnapMgrApp );

bool SnapMgrApp::OnInit()
{
    // For help system
    wxFileSystem::AddHandler(new wxZipFSHandler);
    wxString jobfile;
    if( argc > 1 ) jobfile = argv[1];

    SnapMgrFrame *topWindow = new SnapMgrFrame( jobfile );

    topWindow->Show();


    SetTopWindow( topWindow );
    return true;
}
