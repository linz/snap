#include "snapconfig.h"
#include "wxsimpledialog.hpp"
#include "dlg_pen_options.hpp"
#include "snapplot_help.hpp"

//extern "C"
//{
#include "plotscal.h"
#include "plotconn.h"
//}

// TODO: Handle validation messages and pen options..

const int maxPens = 20;

class PenOptionsDialog : public wxSimpleDialog
{
public:
    PenOptionsDialog( bool isStdRes, wxHelpController *help );

protected:
    virtual void ReadData();
    virtual void Apply();

private:
    bool residuals;
    int nGroups;
    int aPosteriori;
    double  maxSres;
};

PenOptionsDialog::PenOptionsDialog( bool isStdRes, wxHelpController *help )
    : wxSimpleDialog( isStdRes ? "Standardised residual colours" : "Redundancy factor colours" ),
      residuals(isStdRes)
{
    if( residuals )
    {
        AddRadioBox("Residuals to use", aPosteriori,
                    "|Ap&riori residuals|&Aposteriori residuals" );
        AddSpacer();
        AddNumberBox( "&Maximum residual class", maxSres );
        AddSpacer();
    }
    AddNumberBox( "&Number of classes", nGroups );
    AddButtonsAndSize();
    if( help )
    {
        if( isStdRes )
        {
            SetupHelp( help, HELPBASE "dlg_stdres_pens.html" );
        }
        else
        {
            SetupHelp( help, HELPBASE "dlg_redundancy_pens.html" );
        }
    }
}

void PenOptionsDialog::ReadData()
{
    int aposts, npens;

    if( residuals )
    {
        get_sres_pen_options( &maxSres, &aposts, &npens);
        aPosteriori = aposts;
        nGroups = npens;
    }
    else
    {
        get_rfac_pen_options( &npens );
        nGroups = npens;
    }
}

void PenOptionsDialog::Apply()
{
    if( nGroups < 1 ) nGroups = 2;
    if( nGroups > maxPens ) nGroups = maxPens;
    if( residuals )
    {
        setup_sres_pens( maxSres, aPosteriori, nGroups);
    }
    else
    {
        setup_rfac_pens( nGroups );
    }
}

bool SetupStandardisedResidualPens( wxHelpController *help )
{
    PenOptionsDialog dlg(true, help);
    return dlg.RunDialog();
}

bool SetupRedundancyFactorPens( wxHelpController *help )
{
    PenOptionsDialog dlg(false, help);
    return dlg.RunDialog();
}
