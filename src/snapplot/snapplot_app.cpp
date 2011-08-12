#include "snapconfig.h"
#include "snapplot_app.hpp"
#include "snapplot_frame.hpp"
#include "snapplot_loadlog.hpp"
#include "snapplot_util.hpp"

#include <stdio.h>

//extern "C" {
#include "util/progress.h"
#include "util/xprintf.h"
#include "snapplot_util.h"
#include "snapplot_load.h"
//}


IMPLEMENT_APP(SnapplotApp)

bool SnapplotApp::OnInit()
{
    // Need this so that "%n" outputs work!?
    _set_printf_count_output(1);

    // Turn of progress meter as default meter writes to standard output stream
    uninstall_progress_meter();

    // Trap printf output from library routines ..
    set_printf_target( print_log_args );

    // Get rid of timestamps on the log file ..
    wxLog::SetTimestamp(0);

    // Trap errors and other output from SNAP library code ..
    ImplementErrorHandler();

    // Load the SNAP data ...
    // If it fails then display the errors and exit...

    SnapplotLoadLog *loadlog = new SnapplotLoadLog();
    loadlog->StartLogging();
    loadlog->Show();


    bool success = (snapplot_load( argc, argv ) != 0);
    loadlog->EndLogging();

    if( success )
    {
        delete loadlog;
        wxImage::AddHandler( new wxPNGHandler() );
        SnapplotFrame* frame = new SnapplotFrame;
        frame->Show(true);
        SetTopWindow( frame );
    }
    else
    {
        ::wxMessageBox( _T("Failed to load data"), _T("Snapplot load errors"), wxOK | wxICON_HAND, loadlog );
        SetTopWindow( loadlog );
    }


    return true;
}

int SnapplotApp::OnExit()
{
    snapplot_unload();
    return wxApp::OnExit();
}