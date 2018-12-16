REM Build wx widgets libraries

cd wxwidgets\build\msw
nmake -f makefile.vc BUILD=debug SHARED=0 RUNTIME_LIBS=static clean >NULL
nmake -f makefile.vc BUILD=release SHARED=0 RUNTIME_LIBS=static clean >NULL
echo Building wxwidgets 32bit debug version
nmake -f makefile.vc BUILD=debug SHARED=0 RUNTIME_LIBS=static  > build_wxd.log 2>&1
echo Building wxwidgets 32bit release version
nmake -f makefile.vc BUILD=release SHARED=0 RUNTIME_LIBS=static  > build_wx.log 2>&1
