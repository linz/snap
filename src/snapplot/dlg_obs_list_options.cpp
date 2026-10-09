#include "snapconfig.h"
#include "wxsimpledialog.hpp"
#include <wx/checklst.h>
#include <boost/numeric/conversion/cast.hpp>
#include "snapplot_help.hpp"

//extern "C"
//{
#include "snap/snapglob.h"
#include "plotconn.h"
//}


static const std::vector<ListControlOption> obsListOptions =
{
    { "All observations", SRL_ALL },
    { "Used observations", SRL_USED },
    { "Unused observations", SRL_UNUSED },
    { "Rejected observations", SRL_REJECTED }
};

// Note: The order here should match the order initialised for displayFields in plotconn.c
static const std::vector<ListControlOption> fieldListOptions =
{
    { "Obs id", SRF_OBSID },
    { "Date", SRF_DATE },
    { "Data type", SRF_TYPE },
    { "Status", SRF_STATUS },
    { "Standardised residual", SRF_SRES },
    { "Redundancy factor", SRF_RFAC },
    { "Line length", SRF_LENGTH },
    { "Source filename", SRF_FILE }
};

// If fields are added then the maximum default fields must be increased...

static int defaultFields[] = { SRF_FROM, SRF_TO};
static int nDefaultFields = 2;

class ObsListOptionsDialog : public wxSimpleDialog
{
public:
    ObsListOptionsDialog( wxHelpController *help );
    ~ObsListOptionsDialog();

protected:
    virtual void ReadData();
    virtual void Apply();

private:
    int obsToShow;
    wxCheckListBox *lbFieldList;
    int *fieldList;
    int *fieldLookupCode;
    int nFieldList;
};

ObsListOptionsDialog::ObsListOptionsDialog( wxHelpController *help ) :
    wxSimpleDialog( "Observation listing options")
{
    obsToShow = 0;
    lbFieldList = 0;
    int nClassification = obs_classes.count();

    nFieldList = boost::numeric_cast<int>( fieldListOptions.size() ) + nClassification;

    // Note: +1 to account for SRF_LINENO
    fieldList = new int[ nFieldList + nDefaultFields + 1 ];
    fieldLookupCode = new int[ nFieldList ];

    wxSizerFlags flags;
    flags.Expand();
    wxBoxSizer *sizer1 = new wxBoxSizer( wxHORIZONTAL );
    wxBoxSizer *sizer2 = new wxBoxSizer( wxVERTICAL );

    wxArrayString options;
    int nField = 0;

    for( const ListControlOption &option : fieldListOptions )
    {
        options.Add( wxString(option.name) );
        fieldLookupCode[nField++] = option.value;
    }

    // Note: classifications are 1 based, not 0 based
    for( int i = 0; i++ < nClassification; )
    {
        options.Add(wxString(obs_classes.name(i)));
        fieldLookupCode[nField++] = i;
    }
    sizer2->Add( Label("Fields to list"),flags );
    lbFieldList = new wxCheckListBox( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, options );
    sizer2->Add( lbFieldList,flags );
    sizer1->Add( sizer2 );
    sizer1->AddSpacer( GetCharWidth()*2 );

    sizer1->Add( RadioBox(
                     obsToShow,
                     obsListOptions,
                     "&Observations to show"
                 ), flags);

    AddSizer( sizer1 );

    AddButtonsAndSize();

    if( help ) SetupHelp( help, HELPBASE "dlg_obs_listing_options.html"  );
}

ObsListOptionsDialog::~ObsListOptionsDialog()
{
    delete [] fieldList;
    delete [] fieldLookupCode;
}

void ObsListOptionsDialog::ReadData()
{
    for( size_t i = 0; i < lbFieldList->GetCount(); i++ )
    {
        lbFieldList->Check(i,false);
    }
    int nDisplayed = get_displayed_fields( fieldList, nFieldList );
    for( int i = 0; i < nDisplayed; i++ )
    {
        for( int j = 0; j < nFieldList; j++ )
        {
            if( fieldLookupCode[j] == fieldList[i] )
            {
                lbFieldList->Check( j );
            }
        }
    }

    obsToShow = get_sres_display_option();
}

void ObsListOptionsDialog::Apply()
{
    int nf = 0;
    for( int i = 0; i < nDefaultFields; i++ ) fieldList[nf++] = defaultFields[i];
    for( size_t i = 0; i < lbFieldList->GetCount(); i++ )
    {
        if( lbFieldList->IsChecked(i) )
        {
            fieldList[nf++] = fieldLookupCode[i];
            if( fieldLookupCode[i] == SRF_FILE ) fieldList[nf++] = SRF_LINENO;
        }
    }

    set_displayed_fields( fieldList, nf );

    set_sres_display_option( obsToShow );
}

bool SetupObservationListOptions( wxHelpController *help )
{
    ObsListOptionsDialog dlg(help);
    return dlg.RunDialog();
}



