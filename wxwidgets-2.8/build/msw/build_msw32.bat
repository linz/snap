REM building x64 versions
nmake -f makefile.vc BUILD=release SHARED=0
nmake -f makefile.vc BUILD=debug SHARED=0
