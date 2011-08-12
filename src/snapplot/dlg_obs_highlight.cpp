#include "snapconfig.h"
#include "wxsimpledialog.hpp"
#include "dlg_obs_highlight.hpp"
#include "snapplot_help.hpp"

//extern "C"
//{
#include "plotconn.h"
//}


static wxRadioBoxOption obsHighlightOptions[] =
{
    {"&Don't highlight observations", PCONN_HIGHLIGHT_NONE },
    {"Apriori &standardised residuals greater than threshold", PCONN_HIGHLIGHT_SRES },
    {"&Aposteriori standardised residuals greater than threshold", PCONN_HIGHLIGHT_APOST_SRES },
    {"&Redundancy factor less than threshold", PCONN_HIGHLIGHT_RFAC },
    {"Re&jected observations", PCONN_HIGHLIGHT_REJECTED },
    {"&Unused observations", PCONN_HIGHLIGHT_UNUSED },
    {"&Between highlighted stations", PCONN_HIGHLIGHT_IF_BOTH },
    {"&To or from highlighted stations", PCONN_HIGHLIGHT_IF_EITHER },
    {0,0}
};

class ObsHighlightDialog : public wxSimpleDialog
{
public:
    ObsHighlightDialog( wxHelpController *help = 0 );

protected:
    virtual void ReadData();
    virtual void Apply();

private:
    int nOpts;
    int highlightOpt;
    double threshold;
};


ObsHighlightDialog::ObsHighlightDialog( wxHelpController *help ) :
    wxSimpleDialog("Highlight observations")
{

    AddRadioBox( 0, highlightOpt, obsHighlightOptions );
    AddSpacer();
    AddNumberBox( "T&hreshold value", threshold, true, 2 );
    AddButtonsAndSize();
    if( help ) SetupHelp( help, _T( HELPBASE "dlg_highlight_observations.html" ) );
}

void ObsHighlightDialog::ReadData()
{
    int hltOpt;
    get_obs_highlight_option( &hltOpt, &threshold );
    highlightOpt = hltOpt;
}

void ObsHighlightDialog::Apply()
{
    set_obs_highlight_option( highlightOpt, threshold );
}

bool SetupObsHighlights( wxHelpController *help )
{
    ObsHighlightDialog dlg(help);
    return dlg.RunDialog();
}
