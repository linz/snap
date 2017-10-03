rem @echo off

SETLOCAL
IF "%1" == "-v" (
  SHIFT
  echo on
  )
IF "%1" == "-i" SET SNAPDIR=C:\Program Files (x86)\Land Information New Zealand\SNAP
IF "%1" == "-i" SHIFT
IF "%1" == "-r" SET SNAPDIR=..\..\..\ms\built\Releasex86
IF "%1" == "-r" SHIFT
IF "%1" == "-i6" SET SNAPDIR=C:\Program Files\Land Information New Zealand\SNAP64
IF "%1" == "-i6" SHIFT
IF "%1" == "-d6" SET SNAPDIR=..\..\..\ms\built\Debugx64
IF "%1" == "-d6" SHIFT
IF "%1" == "-r6" SET SNAPDIR=..\..\..\ms\built\Releasex64
IF "%1" == "-r6" SHIFT

SET t=%1
IF "%t%" == "" SET t=test*

CD /d %~d0%~p0in
IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\..\ms\built\Debugx86

FOR %%F IN (%t%.snp) DO (
  echo Running %%F
  del /q /f ..\out\%%~nF.* > nul 2>&1
  echo Running "%SNAPDIR%\snap %%F
  "%SNAPDIR%\snap" %%F > nul
  perl ..\clean_snap_listing.pl %%~nF.lst > ..\out\%%~nF.lst
  IF EXIST %%~nF.err perl ..\clean_snap_listing.pl %%~nF.err > ..\out\%%~nF.err
  IF EXIST %%~nF-metadata.csv perl ..\clean_snap_listing.pl %%~nF-metadata.csv > ..\out\%%~nF-metadata.csv
  IF EXIST %%~nF-obs.csv perl ..\clean_snap_listing.pl %%~nF-obs.csv > ..\out\%%~nF-obs.csv
  IF EXIST %%~nF-stn.csv perl ..\clean_snap_listing.pl %%~nF-stn.csv > ..\out\%%~nF-stn.csv
  IF EXIST %%~nF.snx perl ..\clean_snap_listing.pl %%~nF.snx > ..\out\%%~nF.snx
  move %%~nF.cvr.json ..\out > nul 2>&1
  move %%~nF.soln.json ..\out > nul 2>&1
  move %%~nF.cvr ..\out > nul 2>&1
  del /q /f %%~nF.err %%~nF.lst %%~nF.bin %%~nF.new %%~nF-*.csv
  )

cd ..

IF "%t%" == "test*" diff -b -B -r -q out check
