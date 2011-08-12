#ifndef SNAP_SCRIPTENV_HPP
#define SNAP_SCRIPTENV_HPP
#include "wx_includes.hpp"

#include "script.hpp"
#include "snapjob.hpp"

using namespace Scripter;

DECLARE_EVENT_TYPE(wxEVT_SNAP_JOBUPDATED,-1);
DECLARE_EVENT_TYPE(wxEVT_SNAP_CLEARLOG,-1);

#define SNAP_JOBUPDATED_NEWJOB 1
#define SNAP_JOBLOADING 2

class SnapMgrScriptEnv : public Scripter::EnvBase
{
public:
    SnapMgrScriptEnv( wxWindow *frameWindow );
    ~SnapMgrScriptEnv();

    bool LoadJob( const wxString &jobFile );
    bool UpdateJob();
    bool UnloadJob( bool canVeto = true );
    SnapJob *Job() { return job; }

    // Functions supporting the definition of the menu items

    int GetMenuItemCount();
    bool GetMenuDefinition( int i, MenuDef &def );
    bool MenuIsValid( int i );
    void RunMenuActions( int i );

    // Virtual functions to support script environment

    virtual bool GetValue( const wxString &name, Value &value );
    virtual FunctionStatus EvaluateFunction( const wxString &name, int nParams, Value params[], Value &result );
    virtual void ReportError( const wxString &error );

private:

    // Sets up the image file name, used to identify the path to
    // utility programs and load the configuration script ..
    void SetupConfiguration();
    void AddSnapDirToPath();
    void GetCoordSysList();

    wxWindow *frameWindow;
    Scripter::Script *script;
    SnapJob *job;
    wxString imageName;
    wxString imagePath;
    wxString scriptPath;
    wxString coordsyslist;
    wxConfig *config;
    wxArrayString tmpFiles;

};

#endif
