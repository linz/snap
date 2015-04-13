rem @echo off

SETLOCAL
IF "%1" == "-v" (
  SHIFT
  echo on
  )
IF "%1" == "-i" SET SNAPDIR=C:\Program Files (x86)\Land Information New Zealand\SNAP
IF "%1" == "-i" SHIFT
IF "%1" == "-r" SET SNAPDIR=..\..\..\ms\built\Release
IF "%1" == "-r" SHIFT

SET t=%1
IF "%t%" == "" SET t=test*

CD /d %~d0%~p0in
IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\..\ms\built\Debug

FOR %%F IN (%t%.snp) DO (
  echo Running %%F
  del /q /f ..\out\%%~nF.* > nul 2>&1
  echo Running "%SNAPDIR%\snap %%F
  "%SNAPDIR%\snap" %%F > nul
  perl ..\clean_snap_listing.pl %%~nF.lst > ..\out\%%~nF.lst
  IF EXIST %%~nF.err perl ..\clean_snap_listing.pl %%~nF.err > ..\out\%%~nF.err
  IF EXIST %%~nF-metadata.csv perl ..\clean_snap_listing.pl %%~nF-metadata.csv > ..\out\%%~nF-metadata.csv
  move %%~nF-stn.csv ..\out > nul 2>&1
  move %%~nF-obs.csv ..\out > nul 2>&1
  del /q /f %%~nF.err %%~nF.lst %%~nF.bin %%~nF.new %%~nF-*.csv
  )

cd ..

diff -b -B -r -q out check
