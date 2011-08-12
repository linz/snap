@echo off
setlocal

REM Add hhc to path ...

PATH "C:\Program Files\HTML Help Workshop";%PATH%

REM Rebuild project
perl build_helpproject.pl snaphelp help -x gridfile_c

REM Build the help file
hhc snaphelp.hhp

REM build the concord help
perl build_helpproject.pl concord help/programs/concord help/coordsys -x linzdef trigfile gridfile_s
hhc concord.hhp

REM Copy to the debug build directory
copy snaphelp.chm ..\..\ms\built\debug
copy snaphelp.chm ..\..\ms\built\release
