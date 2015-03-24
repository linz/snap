@ECHO OFF
SETLOCAL

IF "%1" == "-i" SET SNAPDIR=C:\Program Files (x86)\Land Information New Zealand\SNAP
IF "%1" == "-i" SHIFT
IF "%1" == "-r" SET SNAPDIR=..\..\ms\built\release
IF "%1" == "-r" SHIFT
IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\debug

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


