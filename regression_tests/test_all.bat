@ECHO OFF
SETLOCAL

IF "%1" == "-i" SET SNAPDIR=C:\Program Files (x86)\Land Information New Zealand\SNAP
IF "%1" == "-i" SHIFT
IF "%1" == "-r" SET SNAPDIR=..\..\ms\built\releasex86
IF "%1" == "-r" SHIFT
IF "%1" == "-d6" SET SNAPDIR=..\..\ms\built\debugx64
IF "%1" == "-d6" SHIFT
IF "%1" == "-r6" SET SNAPDIR=..\..\ms\built\releasex64
IF "%1" == "-r6" SHIFT
IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\debugx86

for /d %%f in (*) do (
  if exist "%%f\check" (
     echo "****************************************"
     echo "Testing %%f"
     setlocal
     cd %%f
     (call .\test.bat) > nul 2>&1
     diff -r -b -B -q check out
     cd ..
     endlocal
  )

)


