#include "snapconfig.h"
#include "wx_includes.hpp"
#include "wxsimpledialog.hpp"
#include "dlg_obs_options.hpp"
#include "snapplot_help.hpp"

//extern "C"
//{
#include "plotscal.h"
#include "plotconn.h"
//}

// TODO: Handle values and ranges in validator ... validation messages

class ObsOptionsDialog : public wxSimpleDialog
{
public:
    ObsOptionsDialog( wxHelpController *help );

protected:
    virtual void ReadData();
    virtual void Apply();

private:
    bool showOneWay;
    int mergeOpt;
    double obsOffset;
    bool autoScaleOffset;
    bool showHiddenStnObs;
    wxString separation;
};

ObsOptionsDialog::ObsOptionsDialog( wxHelpController *help )
    : wxSimpleDialog( "Observation display options")
{
    AddRadioBox( "Observations of the same line",
                 mergeOpt,
                 "|&Merge all observations"
                 "|&Show observations with different colours separately"
                 "|Show &all observations separately"
               );

    AddSpacer();
    AddCheckBox("Show &one way observations with dotted line", showOneWay );
    AddSpacer();
    AddCheckBox("S&how observations to and from hidden stations", showHiddenStnObs );
    AddSpacer();
    AddNumberBox("O&ffset between observations", obsOffset, true, 1 );
    AddButtonsAndSize();
    if( help ) SetupHelp( help,  HELPBASE "dlg_obs_display_options.html" );
}

void ObsOptionsDialog::ReadData()
{
    switch ( merge_common_obs )
    {
    case PCONN_ONE_CONNECTION: mergeOpt = 0; break;
    case PCONN_DIFFERENT_TYPES: mergeOpt = 1; break;
    case PCONN_ALL_CONNECTIONS: mergeOpt = 2; break;
    };

    showOneWay = (show_oneway_obs != 0);
    showHiddenStnObs = (show_hidden_stn_obs != 0);
    autoScaleOffset = (autospacing != 0);
    obsOffset= offset_spacing;
}

void ObsOptionsDialog::Apply()
{
    switch ( mergeOpt )
    {
    case 0: merge_common_obs = PCONN_ONE_CONNECTION; break;
    case 1: merge_common_obs = PCONN_DIFFERENT_TYPES; break;
    case 2: merge_common_obs = PCONN_ALL_CONNECTIONS; break;
    };

    show_oneway_obs = showOneWay ? 1 : 0;
    show_hidden_stn_obs = showHiddenStnObs ? 1 : 0;
    autospacing = autoScaleOffset ? 1 : 0;
    offset_spacing = obsOffset;
}

bool RunObsOptsDialog( wxHelpController *help )
{
    ObsOptionsDialog dlg(help);
    return dlg.RunDialog();
}
