#include "snapconfig.h"
#include "wx_includes.hpp"
#include "wxmainprogwindow.hpp"

//extern "C" {
#include "snapmain.h"
//}

class SnapAdjustApp: public wxApp
{
public:
    SnapAdjustApp() { }
    virtual bool OnInit()
    {
        // Need this so that "%n" outputs work!?
        _set_printf_count_output(1);


        // create and show the main frame

        wxMainProgWindow * mainWin = new wxMainProgWindow("Snap adjustment", true, true );
        SetTopWindow( mainWin );

        mainWin->Show(true);
        mainWin->RunMainProg( snap_main );

        return true;
    }
};

IMPLEMENT_APP(SnapAdjustApp)
