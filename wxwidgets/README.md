Building wxWidgets for Windows (GUI targets only)
=================================================

The GUI programs (snap_manager, snapadjust, snapplot) are disabled by default on Windows
and wxWidgets is not required for a standard build. Only follow these steps if you need
the GUI targets.

1) Download the required wxWidgets version (e.g. https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.4/wxWidgets-3.2.4.7z)

2) Decompress into a wxwidgets subdirectory of this directory

3) From a VS x64 Native Tools Command Prompt run build_wx64.bat
