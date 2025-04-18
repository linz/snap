REM Build wx widgets libraries

cd wxwidgets\build\msw
echo Cleaning old build products
nmake -f makefile.vc BUILD=debug SHARED=0 RUNTIME_LIBS=static TARGET_CPU=AMD64 clean >NUL
nmake -f makefile.vc BUILD=release SHARED=0 RUNTIME_LIBS=static TARGET_CPU=AMD64 clean >NULL
echo Add /FS flag to CL - slower build but may prevent failures access pdb file
SET CL=/FS
echo Building wxwidgets 64bit debug version
nmake -f makefile.vc BUILD=debug SHARED=0 RUNTIME_LIBS=static TARGET_CPU=AMD64 > build_wx64d.log 2>&1
echo Building wxwidgets 64bit release version
nmake -f makefile.vc BUILD=release SHARED=0 RUNTIME_LIBS=static TARGET_CPU=AMD64 > build_wx64.log 2>&1
