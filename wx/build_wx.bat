REM Build wx widgets libraries

SETLOCAL
CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" 
cd wxWidgets-2.8.12\build\msw
nmake -f makefile.vc BUILD=debug SHARED=0 RUNTIME_LIBS=static
nmake -f makefile.vc BUILD=release SHARED=0 RUNTIME_LIBS=static