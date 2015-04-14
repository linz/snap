@echo off
setlocal

set OPTS=/E /C /I /Y /F
set SRCDIR=..\src
set INSTALL=built

for %%v in (Debug, Release) do (
rmdir /S /Q %INSTALL%\%%v\config
mkdir %INSTALL%\%%v\config
set SRCDIR=..\src\%version%
xcopy %OPTS% %SRCDIR%\coordsys\* %INSTALL%\%%v\config\coordsys
xcopy %OPTS% %SRCDIR%\snap\config\* %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snapspec\config\* %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snaplist\config\* %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snap_manager\config\* %INSTALL%\%%v\config
)
