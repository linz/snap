
setlocal

set OPTS=/E /C /I /Y /K 
set SRCDIR=..\src

rmdir /S /Q %INSTALL%\config
mkdir %INSTALL%\config

for %%v in (debug, release) do (
set SRCDIR=..\src\%version%
xcopy %OPTS% %SRCDIR%\coordsys %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snap\config\* %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snapspec\config\* %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snaplist\config\* %INSTALL%\%%v\config
xcopy %OPTS% %SRCDIR%\snap_manager\config\* %INSTALL%\%%v\config
)
