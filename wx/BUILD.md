Building wxWidgets
==================

SNAP is currently built with wxWidgets 2.8.12.  This can be downloaded from

https://github.com/wxWidgets/wxWidgets/releases/download/v2.8.12/wxWidgets-2.8.12.tar.gz

This can be uncompressed into this directory to create the wxWidgets-2.8.12 directory.
Run fix_wxwindows_pbt.pl to fix an error in include files for windows compilation.

Then use build_wxgtk, build_wx,bat, or build_wx64.bat to make the required versions of wxWidgets.
The build_wx.bat and build_wx64.bat assume that Visual Studios 2014 is installed in its default
location.  
