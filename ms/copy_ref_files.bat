@echo off
setlocal

set OPTS=/E /C /I /Y /F
set SRCDIR=..\src
set INSTALL=built

for %%v in (Debugx86, Releasex86, Debugx64, Releasex64) do (
rmdir /S /Q %INSTALL%\%%v\config
mkdir %INSTALL%\%%v\config
set SRCDIR=..\src\%version%
xcopy %OPTS% %SRCDIR%\coordsys\* %INSTALL%\%%v\config\coordsys
xcopy %OPTS% %SRCDIR%\snap\config\* %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snapspec\config\* %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snaplist\config\* %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snap_manager\config\* %INSTALL%\%%v\config
rmdir /S /Q %INSTALL%\%%v\help
mkdir %INSTALL%\%%v\help
xcopy %OPTS% %SRCDIR%\help\help\* %INSTALL%\%%v\help
)
