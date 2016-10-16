@echo off

SETLOCAL
IF "%1" == "-v" (
  SHIFT
  echo on
  )
IF "%1" == "-i" SET SNAPDIR=C:\Program Files (x86)\Land Information New Zealand\SNAP
IF "%1" == "-i" SHIFT
IF "%1" == "-r" SET SNAPDIR=..\..\..\ms\built\Releasex86
IF "%1" == "-r" SHIFT
IF "%1" == "-d6" SET SNAPDIR=..\..\..\ms\built\Debugx64
IF "%1" == "-d6" SHIFT
IF "%1" == "-r6" SET SNAPDIR=..\..\..\ms\built\Releasex64
IF "%1" == "-r6" SHIFT
IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\..\ms\built\Debugx86
echo Using %SNAPDIR%\dat2site

SET t=%1
IF "%t%" == "" SET t=test*

CD /d %~d0%~p0in

del /q /f ..\out\*.*

FOR /F "usebackq" %%F IN (`dir /b /on %t%.snp`) DO (
  echo Running %%F
  del /q /f ..\in\*.new ..\in\*.lst > nul 2>&1
  rem echo Running "%SNAPDIR%\dat2site" %%F
  "%SNAPDIR%\dat2site" %%F > nul
  copy ..\in\*.new ..\out\%%~nF.new > nul 2>&1
  copy ..\in\*.lst ..\out > nul 2>&1
  del /q /f ..\in\*.new ..\in\*.lst > nul 2>&1
  )

cd ..

IF "%t%" == "test*" diff -b -B -r -q out check
