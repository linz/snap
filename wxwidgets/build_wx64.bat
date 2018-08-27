REM Build wx widgets libraries

cd wxwidgets\build\msw
nmake -f makefile.vc BUILD=debug SHARED=0 RUNTIME_LIBS=static TARGET_CPU=AMD64 clean
nmake -f makefile.vc BUILD=release SHARED=0 RUNTIME_LIBS=static TARGET_CPU=AMD64 clean
nmake -f makefile.vc BUILD=debug SHARED=0 RUNTIME_LIBS=static TARGET_CPU=AMD64 
nmake -f makefile.vc BUILD=release SHARED=0 RUNTIME_LIBS=static TARGET_CPU=AMD64
