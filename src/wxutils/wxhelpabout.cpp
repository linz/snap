#include "snapconfig.h"
#include "wx_includes.hpp"
#include "wxsimpledialog.hpp"
#include "wxhelpabout.hpp"
#include "wxsplashimage.hpp"
#include "util/versioninfo.h"

#ifdef __WXGTK__
#include "resources/splashscreen_bmp.xpm"
#endif

class wxHelpAbout : public wxSimpleDialog
{
public:
    wxHelpAbout() :
        wxSimpleDialog( ProgramVersion.program, wxOK )
    {
        wxBitmap bitmap( wxBITMAP(IDB_SPLASHSCREEN) );
        wxBoxSizer *sizer = new wxBoxSizer( wxHORIZONTAL );
        sizer->Add(new wxSplashImage( this, bitmap ));
        sizer->AddSpacer( GetCharWidth() );
        wxFlexGridSizer *sizer2 = new wxFlexGridSizer(2,0,GetCharWidth());
        sizer2->Add( Label("Program:"));
        sizer2->Add(Label( ProgramVersion.program ));
        sizer2->Add(Label("Version:"));
        sizer2->Add(Label( ProgramVersion.version ));
        sizer2->Add(Label("Build date:"));
        sizer2->Add(Label( ProgramVersion.builddate ));
        sizer2->Add(Label("Copyright:"));
        sizer2->Add(Label( "Land Information New Zealand" ));
        sizer->Add( sizer2 );

        AddSizer( sizer );
        AddSpacer();
        AddLabel(
            "DISCLAIMER: Land Information New Zealand does not offer any support for this software.\n"
            "The software is provided \"as is\" and without warranty of any kind. \n"
            "In no event shall LINZ be liable for loss of any kind whatsoever with respect to\n"
            "the download, installation and use of the software.\n"
            "The software is designed to work with Microsoft Windows.\n"
            "However, LINZ makes no warranty regarding the performance or non-performance\n"
            "of the software on any particular system or system configuration. ");
        AddButtonsAndSize();
    }
};

void ShowHelpAbout()
{
    wxHelpAbout helpAbout;
    helpAbout.RunDialog();
}
