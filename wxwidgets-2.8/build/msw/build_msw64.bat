REM building x64 versions
nmake -f makefile.vc BUILD=release TARGET_CPU=AMD64 SHARED=0
nmake -f makefile.vc BUILD=debug TARGET_CPU=AMD64 SHARED=0
