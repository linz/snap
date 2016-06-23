@echo off
setlocal

REM Add hhc to path ...

PATH "C:\Program Files\HTML Help Workshop";%PATH%

REM Rebuild project
perl ..\..\help\build_helpproject.pl nzmapconv .

REM Build the help file
hhc nzmapconv.hhp

REM Copy to the debug build directory
copy nzmapconv.chm ..\..\..\ms\built\debug
copy nzmapconv.chm ..\..\..\ms\built\release
