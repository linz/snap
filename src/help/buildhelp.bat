@echo off
echo ===== Building snap help file =====
perl build_helpproject.pl -t "%TEMP%" -b -c ..\..\..\ms\built\debug -c ..\..\ms\built\release snaphelp help -x gridfile_c
echo ===== Building concord help file =====
perl build_helpproject.pl -t "%TEMP%" -b concord help/programs/concord help/coordsys -x linzdef trigfile gridfile_s
