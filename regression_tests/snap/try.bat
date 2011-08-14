@echo off

SETLOCAL
IF "%1" == "-v" (
  SHIFT
  echo on
  )
IF "%1" == "-i" SET SNAPDIR=C:\Program Files\Land Information New Zealand\SNAP
IF "%1" == "-i" SHIFT

SET t=%1
SET vi=c:\bin\vim\vim72\gvim.exe

IF "%t%" == "" (
echo Need test name as parameter
goto exit
)

CD /d %~d0%~p0in

if not exist %t%.snp (
echo Snap command file %t%.snap does't exist
goto exit
)

IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\..\ms\built\debug

echo Running %t%
"%SNAPDIR%\snap" %t%

start %vi% %t%.lst
if exist %t%.err start %vi% %t%.err

:exit
