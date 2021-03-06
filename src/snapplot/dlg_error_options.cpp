#include "snapconfig.h"
#include "wxsimpledialog.hpp"
#include "dlg_error_options.hpp"
#include "snapplot_help.hpp"

//extern "C" {
#include "plotscal.h"
#include "snap/snapglob.h"
//}

class ErrorOptionsDialog : public wxSimpleDialog
{
public:
    ErrorOptionsDialog( wxHelpController *help = 0 );
    virtual void ReadData();
    virtual void Apply();

private:
    int aPosteriori;
    int useConfLimit;
    double confLimit;
    double horErrorFactor;
    bool horAutoScale;
    double vrtErrorFactor;
    bool vrtAutoScale;
};

ErrorOptionsDialog::ErrorOptionsDialog( wxHelpController *help )
    : wxSimpleDialog("Error options")
{
    AddRadioBox("For all errors and standardised residuals", aPosteriori,
                "|Ap&riori errors|&Aposteriori errors" );
    AddSpacer();
    AddLabel("For coordinate error ellipses and height errors");
    AddRadioBox("",useConfLimit,"|&Standard errors|&Confidence limits");
    AddNumberBox("C&onfidence level (percent) or SE multipl",confLimit,true,3);
    AddSpacer();
    AddControls(3,
                Label("&Horizontal error exaggeration"),
                NumberBox( horErrorFactor,true,1),
                CheckBox( "&Autoscale", horAutoScale ),
                Label("&Vertical error exaggeration"),
                NumberBox( vrtErrorFactor,true,1),
                CheckBox( "A&utoscale", vrtAutoScale ),
                0);
    AddButtonsAndSize();
    if( help ) SetupHelp( help,  HELPBASE "dlg_error_options.html"  );
}

void ErrorOptionsDialog::ReadData()
{
    aPosteriori = apriori ? 0 : 1;
    useConfLimit = errconflim ? 1 : 0;
    confLimit = errconfval;
    horErrorFactor = errell_scale;
    vrtErrorFactor = hgterr_scale;

    double scl;
    int autoscl;
    get_errell_exaggeration( &scl, &autoscl );
    horAutoScale = (autoscl != 0);
    get_hgterr_exaggeration( &scl, &autoscl );
    vrtAutoScale = (autoscl != 0);
}

void ErrorOptionsDialog::Apply()
{
    apriori=aPosteriori ? 0 : 1;
    errconflim=useConfLimit ? 1 : 0;
    errconfval=confLimit;
    set_confidence_limit();
    double dfltscl = calc_default_error_scale();
    double scl = horErrorFactor;
    if( horAutoScale ) scl = 1.1 * scl / dfltscl;
    set_errell_exaggeration( scl, horAutoScale ? 1 : 0 );
    scl = vrtErrorFactor;
    if( vrtAutoScale ) scl = 1.1 * scl / dfltscl;
    set_hgterr_exaggeration( scl, vrtAutoScale ? 1 : 0 );
}


bool SetupErrorOptions( wxHelpController *help )
{
    ErrorOptionsDialog dlg( help );
    return dlg.RunDialog();
}