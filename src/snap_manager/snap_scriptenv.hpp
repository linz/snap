#ifndef SNAP_SCRIPTENV_HPP
#define SNAP_SCRIPTENV_HPP
#include <cstddef>
#include "wx_includes.hpp"
#include "script.hpp"

class SnapJob;

using namespace Scripter;

DECLARE_EVENT_TYPE(wxEVT_SNAP_JOBUPDATED, -1);
DECLARE_EVENT_TYPE(wxEVT_SNAP_CLEARLOG, -1);

#define SNAP_JOBUPDATED_NEWJOB 1
#define SNAP_JOBLOADING 2
#define CMD_CONFIG_BASE 100

class SnapMgrScriptEnv : public wxEvtHandler, public Scripter::EnvBase {
 public:
    SnapMgrScriptEnv(wxFrame* frameWindow);
    ~SnapMgrScriptEnv() override;

    bool LoadJob(const wxString& jobFile);
    bool UpdateJob();
    bool UnloadJob(bool canVeto = true);
    SnapJob* Job() {
        return job;
    }

    // Virtual functions to support script environment

    bool GetValue(const wxString& name, Value& value) override;
    bool AddMenuItem(const wxString& name, const wxString& description, int id) override;
    bool RemoveMenuItem(const wxString& name) override;
    void EnableMenuItem(const wxString& name, bool enabled) override;
    FunctionStatus EvaluateFunction(const wxString& name, const Value* params, Value& result) override;
    void ReportError(const wxString& error) override;

 private:
    // Sets up the image file name, used to identify the path to
    // utility programs and load the configuration script ..
    void OnCmdConfigMenuItem(wxCommandEvent& event);
    void SetupConfiguration();
    void InsertPath(const wxString& path, const wxString& envvar = "PATH");
    void AddSnapDirToPath();
    bool RemoveDirectory(const wxString& dirpath);
    wxString& GetCoordSysList();
    wxString& GetHeightRefList();
    // GetMenuItem - creates parents if parent is not null
    wxMenuItem* GetMenuItem(const wxString& name, wxMenu** parent = nullptr, bool createParents = false);
    wxMenuItem* GetMenuItemByLabel(wxMenu* menu, const wxString& label, bool wantSubMenu);
    std::size_t GetMenuInsertPosition(wxMenu* menu);

    wxFrame* frameWindow;
    Scripter::Script* script;
    SnapJob* job;
    wxString coordsyslist;
    wxString heightreflist;
    wxString scriptPath;
    wxString userScriptPath;
    wxArrayString tmpFiles;

    DECLARE_EVENT_TABLE()
};

#endif
