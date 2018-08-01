#include "snapconfig.h"
#include "wxsimpledialog.hpp"
#include "dlg_findstation.hpp"
#include "snapplot_help.hpp"

//extern "C"
//{
#include "snap/stnadj.h"
#include "network/network.h"
//}

class FindStationDialog : public wxSimpleDialog
{
public:
    FindStationDialog( wxHelpController *help = 0 );
    virtual bool Validate();
    int stationId;
    bool zoomMap;

private:
    wxString stationCode;
    wxTextCtrl *stationCtrl;
};

FindStationDialog::FindStationDialog( wxHelpController *help )
    : wxSimpleDialog("Find Station")
{
    AddLabel( "Search for a station");
    AddSpacer();
    stationCtrl = AddTextBox( "Station code", stationCode, 10 );
    AddSpacer();
    AddCheckBox("Zoom map to station", zoomMap );
    AddButtonsAndSize();
    if( help ) SetupHelp( help,  HELPBASE "dlg_find_station.html"  );
}

bool FindStationDialog::Validate()
{
    wxString stationCode = stationCtrl->GetValue().Trim().Trim(false);
    if( stationCode.Len() == 0 )
    {
        wxMessageBox("You must enter a station code","Error",wxOK | wxICON_EXCLAMATION);
        return false;
    }

    stationId = find_station( net, stationCode.mb_str() );
    if( stationId <= 0 )
    {
        wxMessageBox(wxString::Format("Station %s is not defined in this network",
                    stationCode),"Error", wxOK | wxICON_EXCLAMATION );
        return false;
    }
    return true;
}


int GetFindStationId( wxHelpController *help, bool &zoomMap )
{
    FindStationDialog dlg( help );
    dlg.zoomMap = zoomMap;
    int stationId = 0;
    if( dlg.RunDialog() )
    {
        stationId = dlg.stationId;
        zoomMap = dlg.zoomMap;
    }
    return stationId;
}
