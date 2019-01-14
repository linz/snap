@echo off
chdir /d %~dp0
echo ===== Creating help project files
perl build_help_files.pl snaphelp help -x gridfile_c
perl build_help_files.pl concord help/programs/concord help/coordsys -x linzdef trigfile gridfile_s
echo ===== Building snap help file =====
perl compilechm.pl -t "%TEMP%" snaphelp 
if exist ..\..\..\ms\built\debug\ copy snaphelp.chm ..\..\..\ms\built\debug
if exist ..\..\..\ms\built\release\ copy snaphelp.chm ..\..\..\ms\built\release
echo ===== Building concord help file =====
perl compilechm.pl -t "%TEMP%" concord 
