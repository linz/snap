#include "snapconfig.h"
#include "wx_includes.hpp"
#include "wxhelpabout.hpp"
#include "snapjob.hpp"
#include "snap_scriptenv.hpp"

enum
{
    CMD_FILE_CLOSE = 1,
    CMD_HELP_HELP,
    CMD_HELP_ABOUT,

    CMD_CONFIG_BASE
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
    void EnableMenuItems( wxMenu *menu );
    void SetupWindows();
    void ClearLog();

    void OnFileHistory( wxCommandEvent &event );
    void OnCmdClose( wxCommandEvent &event );
    void OnCmdHelpHelp( wxCommandEvent &event );
    void OnCmdHelpAbout( wxCommandEvent &event );
    void OnCmdConfigMenuItem( wxCommandEvent &event );
    void OnMenuOpen( wxMenuEvent &event );
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
    EVT_MENU_OPEN( SnapMgrFrame::OnMenuOpen )
    EVT_ACTIVATE( SnapMgrFrame::OnActivate )
    EVT_CLOSE( SnapMgrFrame::OnClose )
END_EVENT_TABLE()

SnapMgrFrame::SnapMgrFrame( const wxString &jobfile ) :
    wxFrame(NULL, wxID_ANY, _T("SNAP"))
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

    config = new wxConfig(_T("SnapMgr"),_T("LINZ"));

    // Load the scripting environment

    scriptenv = new SnapMgrScriptEnv(this);

    // Restore the previous working directory

    wxString curDir;
    if( config->Read( _T("WorkingDirectory"), &curDir ) )
    {
        wxSetWorkingDirectory( curDir );
    }

    SetupIcons();

    // SetupMenu must be called after loading the scripting environment

    SetupMenu();

    // Note: wxFileHistory.Load must be called after setting up menu so that the menu is populated ...

    config->SetPath( "/History" );
    fileHistory.Load( *config );

    // Set up the help file

    help = new wxHelpController( this );

    wxFileName helpFile(wxStandardPaths::Get().GetExecutablePath());
    helpFile.SetName(_T("snaphelp"));
    help->Initialize( helpFile.GetFullPath() );

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
    config->Write(_T("WorkingDirectory"),wxGetCwd());

    delete scriptenv;
    delete config;
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
    menuBar->Append( fileMenu, _T("&File") );


    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append( CMD_HELP_HELP,
                      _T("&Help\tF1"),
                      _T("Get help about snapplot"));
    menuBar->Append( helpMenu, _T("&Help") );

    // Set up the script based menu items ... these can add to the File menu if that is specified ..

    nScriptMenuItems = scriptenv->GetMenuItemCount();

    for( int i = 0; i < nScriptMenuItems; i++ )
    {
        MenuDef def;
        if( ! scriptenv->GetMenuDefinition( i, def ) ) continue;

        // Must have at least one sub menu ... put into a "&Scripts" menu if there isn't one

        wxStringTokenizer menuParts(_T(def.menu_name),_T("|"));

        if( menuParts.CountTokens() < 1 ) continue;
        wxString menuName;
        if( menuParts.CountTokens() > 1 )
        {
            menuName = menuParts.GetNextToken();
        }
        else
        {
            menuName = wxString(_T("&Scripts"));
        }

        // Find the menu, or create it if it doesn't exist...

        int menuId = menuBar->FindMenu( menuName );
        wxMenu *menu;
        if( menuId == wxNOT_FOUND )
        {
            menu = new wxMenu;
            menuBar->Insert( menuBar->GetMenuCount()-1, menu, menuName );
        }
        else
        {
            menu = menuBar->GetMenu( menuId );
        }

        // Track down any further submenus, creating as necessary

        while( menuParts.CountTokens() > 1 )
        {
            menuName = menuParts.GetNextToken();
            int id = menu->FindItem( menuName );
            if( id == wxNOT_FOUND )
            {
                wxMenu *nm = new wxMenu;
                menu->AppendSubMenu( nm, menuName );
                menu = nm;
            }
            else
            {
                // Not clear if this should be FindItem or FindItemByPosition...

                wxMenuItem *item = menu->FindItem( id );
                wxMenu *subMenu = item->GetSubMenu();
                if( subMenu == NULL )
                {
                    wxString message = wxString::Format(_T("Configuration error: Cannot create submenu %s of %s"),
                                                        menuName.c_str(), def.menu_name );
                    ::wxMessageBox( message, _T("Configuration error"), wxOK | wxICON_ERROR, this );
                    continue;
                }
                menu = subMenu;
            }
        }

        // Check the item doesn't already exist

        menuName = menuParts.GetNextToken();
        int id = menu->FindItem( menuName );
        if( id != wxNOT_FOUND )
        {
            wxString message = wxString::Format(_T("Configuration error: Cannot create menu item %s of %s"),
                                                menuName.c_str(), def.menu_name );
            ::wxMessageBox( message, _T("Configuration error"), wxOK | wxICON_ERROR, this );
            continue;
        }

        menu->Append( CMD_CONFIG_BASE + i, menuName, _T(def.description) );
        Connect( CMD_CONFIG_BASE + i, wxEVT_COMMAND_MENU_SELECTED,
                 wxCommandEventHandler(SnapMgrFrame::OnCmdConfigMenuItem ));
    }

    // Add items at the end of the file menu, add the help menu, and install the menu
    // bar...

    fileMenu->AppendSeparator();
    fileMenu->Append(CMD_FILE_CLOSE,
                     _T("&Close\tAlt-F4"),
                     _T("Quit SNAP"));

    fileHistory.UseMenu( fileMenu );

    helpMenu->AppendSeparator();
    helpMenu->Append( CMD_HELP_ABOUT,
                      _T("&About"),
                      _T("Information about this version snaplot program"));


    SetMenuBar(menuBar);
}

void SnapMgrFrame::EnableMenuItems( wxMenu *menu )
{
    int nItems = menu->GetMenuItemCount();
    for( int i = 0; i < nItems; i++ )
    {
        wxMenuItem *item = menu->FindItemByPosition( i );
        int id = item->GetId() - CMD_CONFIG_BASE;
        if( id >= 0 && id < nScriptMenuItems )
        {
            item->Enable( scriptenv->MenuIsValid( id ));
        }
        wxMenu *subMenu = item->GetSubMenu();
        if( subMenu ) EnableMenuItems( subMenu );
    }

}

void SnapMgrFrame::SetupWindows()
{
    SetBackgroundColour( wxColour(_T("WHITE")));
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
        wxString label(_T("SNAP - "));
        label.Append( scriptenv->Job()->GetFilename() );
        SetLabel( label );

        if( event.GetInt() & SNAP_JOBUPDATED_NEWJOB )
        {
            ClearLog();
            wxLogMessage( "Job location: %s", job->GetPath().c_str());
            wxLogMessage( "Command file: %s", job->GetFilename().c_str());
            wxLogMessage( "Job title:    %s", job->Title().c_str());
            wxLogMessage( "Coordinate file: %s", job->CoordinateFilename().c_str());
        }
    }
    else
    {
        SetLabel(_T("SNAP"));
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

void SnapMgrFrame::OnCmdConfigMenuItem( wxCommandEvent &event )
{
    int id = event.GetId() - CMD_CONFIG_BASE;
    if( id >= 0 && id < nScriptMenuItems )
    {
        scriptenv->RunMenuActions( id );

    }
}

void SnapMgrFrame::OnMenuOpen( wxMenuEvent &event )
{
    wxMenu *menu = event.GetMenu();
    if( menu ) EnableMenuItems( menu );
}

void SnapMgrFrame::OnClearLog( wxCommandEvent & WXUNUSED(event) )
{
    ClearLog();
}

void SnapMgrFrame::OnCmdHelpAbout( wxCommandEvent & WXUNUSED(event) )
{
    ShowHelpAbout();
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
    wxString jobfile;
    if( argc > 1 ) jobfile = _T(argv[1]);

    SnapMgrFrame *topWindow = new SnapMgrFrame( jobfile );

    topWindow->Show();


    SetTopWindow( topWindow );
    return true;
}
