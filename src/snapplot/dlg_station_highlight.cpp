#include "snapconfig.h"
#include "wx_includes.hpp"
#include "wxsimpledialog.hpp"
#include "ctrl_station_selector.hpp"
#include "dlg_station_highlight.hpp"
#include "snapplot_help.hpp"

//extern "C" {
#include "snap/stnadj.h"
#include "snap/snapglob.h"
#include "network/network.h"
#include "plotscal.h"
#include "plotstns.h"
#include "plotconn.h"
//}


typedef struct
{
    char *label;
    unsigned char mask;
    unsigned char incompatible;
} SelectionData;

enum { flgAll=1, flgRej=2, flgHFix=4, flgVFix=8,
       flgHFlt=16, flgVFlt=32, flgHAdj=64, flgVAdj=128
     };

const int nStationSelectionOptions = 7;

// TODO: Sort out accelerator keys

const SelectionData stationSelectionOption[nStationSelectionOptions] =
{
    {"F&ixed horizontally",flgHFix, flgAll | flgRej | flgHFlt | flgHAdj },
    {"&Floated horizontally",flgHFlt,flgAll | flgRej | flgHFix | flgHAdj },
    {"Adjust &horizontally",flgHAdj,flgAll | flgRej | flgHFix | flgHFlt },
    {"Fi&xed vertically",flgVFix,flgAll | flgRej | flgVFlt | flgVAdj },
    {"Fl&oated vertically",flgVFlt,flgAll | flgRej | flgVFix | flgVAdj },
    {"Adjust &vertically",flgVAdj,flgAll | flgRej | flgVFix | flgVFlt },
    {"Re&jected",flgRej,flgAll | flgHFix | flgVFix | flgHFlt | flgVFlt | flgHAdj | flgVAdj }
};

enum { scAll, scStatus, scHorAdj, scHorErr, scVrtAdj, scVrtErr, scOrder };

static char horErrOpt[128];
static char vrtErrOpt[128];

static wxRadioBoxOption stnCriteria[] =
{
    {"All stations", scAll},
    {"By status (select on right ->)", scStatus},
    {"Horizontal adjustment > threshold", scHorAdj},
    {horErrOpt, scHorErr},
    {"Vertical adjustment > threshold", scVrtAdj},
    {vrtErrOpt, scVrtErr},
    {"Station order = threshold", scOrder },
    {0,0}
};

static wxRadioBoxOption obsHighlightOptions[] =
{
    {"&Don't highlight observations", PCONN_HIGHLIGHT_NONE },
    {"&Between highlighted stations", PCONN_HIGHLIGHT_IF_BOTH },
    {"&To or from highlighted stations", PCONN_HIGHLIGHT_IF_EITHER },
    {0,0}
};

class StationHighlightDialog : public wxSimpleDialog
{
public:
    StationHighlightDialog( bool hideShow, wxHelpController *help );
    void ApplyChanges();
protected:

    virtual void ReadData();

private:
    void OnAddStations( wxCommandEvent & WXUNUSED(event) ) { SelectStations( true ); }
    void OnRemoveStations( wxCommandEvent & WXUNUSED(event) ) { SelectStations( false ); }
    void OnClearStations( wxCommandEvent &event );
    void SelectStations( bool select );
    void OnIdle( wxIdleEvent &event );

    bool hideShow;
    wxStationSelectorGrid *stationSelector;
    bool falseOption;
    bool showHiddenStnObs;
    int obsHighlightOption;
    int stnSelOpt;
    int nSelected;

    wxString stnSelValue;
    wxRadioBox *rbSelOpt;
    wxTextCtrl *txtSelValue;
    wxCheckBox *cbOption[ nStationSelectionOptions ];
    wxStaticText *lbStationCount;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE( StationHighlightDialog, wxSimpleDialog )
    EVT_IDLE( StationHighlightDialog::OnIdle )
END_EVENT_TABLE()

StationHighlightDialog::StationHighlightDialog( bool hideShow, wxHelpController *help ) :
    wxSimpleDialog( hideShow ? "Hide stations" : "Highlight stations", 0),
    hideShow( hideShow )
{
    // MakeSizeable();
    falseOption = false;
    stnSelOpt = scAll;
    stnSelValue = "";
    nSelected = -1;

    if( use_confidence_limit )
    {
        sprintf(horErrOpt,"%s %.1lf%% horizontal conf lim > threshold",
                aposteriori_errors ? "A posteriori" : "A priori", confidence_limit);
        sprintf(vrtErrOpt,"%s %.1lf%% vertical conf lim > threshold",
                aposteriori_errors ? "A posteriori" : "A priori", confidence_limit);
    }
    else
    {
        sprintf(horErrOpt,"%s horizontal error > threshold",
                aposteriori_errors ? "A posteriori" : "A priori");
        sprintf(vrtErrOpt,"%s vertical error > threshold",
                aposteriori_errors ? "A posteriori" : "A priori");
    }

    wxBoxSizer *box1 = new wxBoxSizer( wxHORIZONTAL );
    wxBoxSizer *bxCol1 = new wxBoxSizer( wxVERTICAL );
    stationSelector = new wxStationSelectorGrid( this );
    stationSelector->SetSize( wxSize( GetCharWidth()*40, GetCharHeight()*30) );
    stationSelector->SetMinSize( wxSize( GetCharWidth()*40, GetCharHeight()*30) );

    bxCol1->Add( stationSelector, wxSizerFlags().Expand().Proportion(1)  );
    bxCol1->AddSpacer( GetCharHeight() );
    lbStationCount = Label("   ");
    bxCol1->Add( lbStationCount );
    box1->Add( bxCol1, wxSizerFlags().Expand().Proportion(1) );
    box1->AddSpacer( GetCharWidth()*2 );

    wxBoxSizer *box2 = new wxBoxSizer( wxVERTICAL );
    box2->Add( Label("Stations to change") );
    wxBoxSizer *bxStn1 = new wxBoxSizer( wxHORIZONTAL );
    rbSelOpt = RadioBox( stnSelOpt, stnCriteria );
    bxStn1->Add( rbSelOpt );
    bxStn1->AddSpacer( GetCharWidth()*2 );

    wxBoxSizer *bxStn2 = new wxBoxSizer( wxVERTICAL );
    bxStn2->AddSpacer( GetCharHeight() );
    bxStn2->Add( Label( "Status options" ) );

    for( int i = 0; i < nStationSelectionOptions; i++ )
    {
        cbOption[i] = CheckBox( stationSelectionOption[i].label, falseOption );
        cbOption[i]->SetValue( false );
        bxStn2->Add( cbOption[i] );
    }
    bxStn2->AddSpacer( GetCharHeight() );
    wxBoxSizer *bxStn3 = new wxBoxSizer( wxHORIZONTAL );
    bxStn3->Add( Label("Threshold: ") );
    txtSelValue = TextBox( stnSelValue, 10 );
    bxStn3->Add( txtSelValue );
    bxStn2->Add( bxStn3 );

    bxStn1->Add( bxStn2 );
    box2->Add( bxStn1 );

    box2->AddSpacer( GetCharHeight() );

    wxBoxSizer *box3 = new wxBoxSizer( wxHORIZONTAL );
    int buttonSpace = GetCharWidth();
    wxButton *defaultButton = Button( "&Add", wxCommandEventHandler( StationHighlightDialog::OnAddStations ));
    box3->Add( defaultButton );
    box3->AddSpacer(buttonSpace);
    box3->Add( Button( "&Remove", wxCommandEventHandler( StationHighlightDialog::OnRemoveStations )) );
    box3->AddSpacer(buttonSpace);
    box3->Add( Button( "&Clear all", wxCommandEventHandler( StationHighlightDialog::OnClearStations )) );
    defaultButton->SetDefault();
    box2->Add( box3 );

    box2->AddSpacer( GetCharHeight() );

    if( ! hideShow )
    {
        box2->Add(
            RadioBox( obsHighlightOption, obsHighlightOptions, "Highlight observations" )
        );
    }
    else
    {
        box2->Add( CheckBox( "Show observations to hidden stations", showHiddenStnObs ) );
    }


    box2->AddStretchSpacer();
    wxBoxSizer *box4 = new wxBoxSizer( wxHORIZONTAL );
    box4->Add( new wxButton(this, wxID_CANCEL, "&OK" ));

    if( help )
    {
        box4->AddSpacer(buttonSpace);
        box4->Add( new wxButton(this, wxID_HELP ));
        if( hideShow )
        {
            SetupHelp( help, _T(HELPBASE "dlg_hideshow_stations.html"));
        }
        else
        {
            SetupHelp( help, _T(HELPBASE "dlg_highlight_stations.html"));
        }
    }
    box4->AddSpacer(buttonSpace);

    box2->Add( box4, wxSizerFlags().Right() );

    box1->Add( box2, wxSizerFlags().Expand() );

    AddSizer( box1, wxSizerFlags().Border().Expand().Proportion(1) );
    // AddSpacer();

    AddButtonsAndSize();
}

void StationHighlightDialog::ReadData()
{
    if( ! hideShow )
    {
        int hltOpt;
        double threshold;
        get_obs_highlight_option( &hltOpt, &threshold );
        obsHighlightOption = hltOpt;
    }
    else
    {
        showHiddenStnObs = (show_hidden_stn_obs != 0);
    }

    for( int i = 0; i < stationSelector->StationCount(); i++ )
    {
        int istn = stationSelector->StationNumber( i );
        bool select = hideShow ? (station_hidden(istn) != 0) : (station_highlighted( istn ) != 0);
        stationSelector->Select( i, select  );
    }

}

// Changes are always applied once the user exits the dialog ..

void StationHighlightDialog::ApplyChanges()
{
    TransferDataFromWindow();
    if( ! hideShow )
    {
        int hltOpt;
        double threshold;
        get_obs_highlight_option( &hltOpt, &threshold );
        set_obs_highlight_option( obsHighlightOption, threshold );
    }
    else
    {
        show_hidden_stn_obs = showHiddenStnObs ? 1 : 0;
    }

    for( int i = 0; i < stationSelector->StationCount(); i++ )
    {
        int istn = stationSelector->StationNumber( i );
        if( stationSelector->IsSelected(i) )
        {
            if( hideShow ) { hide_station(istn); }
            else { highlight_station( istn ); }
        }
        else
        {
            if( hideShow ) { unhide_station(istn); }
            else { unhighlight_station( istn ); }
        }
    }
}

void StationHighlightDialog::OnClearStations( wxCommandEvent & WXUNUSED(event) )
{
    for( int i= 0; i < stationSelector->StationCount(); i++ )
    {
        stationSelector->Select( i, false );
    }
    stationSelector->Refresh();
}

void StationHighlightDialog::SelectStations( bool select )
{
    unsigned char mask = 0;
    int i;
    int iorder = 0;
    double value = 0.0;

    stnSelOpt = stnCriteria[rbSelOpt->GetSelection()].value;
    // rbSelOpt->SetSelection(0);

    if( stnSelOpt == scAll )
    {
    }
    else if( stnSelOpt == scStatus )
    {
        for( i = 0; i < nStationSelectionOptions; i++ )
        {
            if( cbOption[i]->IsChecked() )
            {
                mask |= stationSelectionOption[i].mask;
            }
        }
        if( ! mask ) return;
    }
    else if( stnSelOpt == scOrder )
    {
        wxString strOrder = txtSelValue->GetValue().Trim().Trim(false);
        iorder = network_order_id( net, (char *) (strOrder.c_str()), 0 );
        if( iorder <= 0 ) return;
    }
    else
    {
        wxString strValue = txtSelValue->GetValue();
        if( ! strValue.ToDouble( & value ) )
        {
            wxMessageBox("Threshold value must be a number","Error",wxOK | wxICON_EXCLAMATION );
            return;
        }
        if( stnSelOpt == scHorErr && errell_factor > 0.0 ) value /= errell_factor;
        if( stnSelOpt == scVrtErr && hgterr_factor > 0.0 ) value /= hgterr_factor;
    }

    for( i= 0; i < stationSelector->StationCount(); i++ )
    {
        // No point testing station if status won't be changed..
        bool selected = stationSelector->IsSelected(i);
        if( selected == select ) continue;

        station *st = 0;
        stn_adjustment *sa = 0;
        int istn = stationSelector->StationNumber( i );
        st = stnptr( istn );

        if( ! st ) continue;
        sa = stnadj( st );

        // For each criteria type apply the tests, and continue
        // loop if not met...

        if( stnSelOpt == scAll )
        {
        }
        else if( stnSelOpt == scStatus )
        {
            if( ! sa ) continue;
            unsigned char stnMask = flgAll;
            if( sa->flag.rejected )
            {
                stnMask |= flgRej;
            }
            else
            {
                if( sa->flag.adj_h )
                {
                    if( sa->flag.float_h ) stnMask |= flgHFlt; else stnMask |= flgHAdj;
                }
                else stnMask |= flgHFix;
                if( sa->flag.adj_v )
                {
                    if( sa->flag.float_v ) stnMask |= flgVFlt; else stnMask |= flgVAdj;
                }
                else stnMask |= flgVFix;
            }
            if( ! ((mask & stnMask) == mask) ) continue;
        }
        else if( stnSelOpt == scOrder )
        {
            int orderid = get_station_class( st, net->orderclsid );
            if( orderid != iorder ) continue;
        }
        else
        {
            double emax, emin, b1;
            double dxyz[3];

            if( ! sa ) continue;
            double testValue = 0.0;
            switch( stnSelOpt )
            {
            case scHorErr:
                get_error_ellipse( istn, &emax, &emin, &b1 );
                testValue = emax;
                break;

            case scHorAdj:
                get_station_adjustment( istn, dxyz );
                testValue = _hypot(dxyz[0],dxyz[1]);
                break;

            case scVrtErr:
                get_height_error( istn, &testValue );
                break;

            case scVrtAdj:
                get_station_adjustment( istn, dxyz );
                testValue = fabs(dxyz[2]);
                break;

            default:
                continue;
                break;
            }

            if( testValue <= value ) continue;
        }

        // If got this far then apply the new selection option
        stationSelector->Select( i, select );
    }
    stationSelector->Refresh();
}

// Unnecessary overhead as seems to be continuous idle events, but
// easiest way to ensure selections on list directly are counted.

void StationHighlightDialog::OnIdle( wxIdleEvent & WXUNUSED(event) )
{
    int nSel = 0;
    for( int i = 0; i < stationSelector->StationCount(); i++ )
    {
        if( stationSelector->IsSelected(i) ) { nSel++; }
    }
    if( nSel == nSelected ) return;
    nSelected = nSel;
    wxString labelStr = wxString::Format("%d/%d stations %s",
                                         nSelected, stationSelector->StationCount(),
                                         hideShow ? " are hidden" : " are highlighted" );
    lbStationCount->SetLabel( labelStr );
}

// TODO: see if I can fix this
/*

This code was from original model .. turns off incompatible options when
a checkbox is selected

void StationHighlightDialog::HandleSelectionButton( void *p ) {
   ccCheckBox **cbArray = (ccCheckBox **) p;
   int id = (int) (cbArray[0]->Id());
   cbArray -= id;
   for( int i = 0; i < 8; i++ ) {
      if( id == i ) continue;
      if( selDef[i].incompatible & selDef[id].mask ) cbArray[i]->ClearToggle();
      }
   }

   */

bool SetupStationHighlights( bool hideShow, wxHelpController *help )
{
    StationHighlightDialog dlg( hideShow, help );
    dlg.RunDialog();
    dlg.ApplyChanges();
    return true;
}
