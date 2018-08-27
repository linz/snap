REM Build wx widgets libraries

cd wxwidgets\build\msw
nmake -f makefile.vc BUILD=debug SHARED=0 RUNTIME_LIBS=static clean
nmake -f makefile.vc BUILD=release SHARED=0 RUNTIME_LIBS=static clean
nmake -f makefile.vc BUILD=debug SHARED=0 RUNTIME_LIBS=static
nmake -f makefile.vc BUILD=release SHARED=0 RUNTIME_LIBS=static
