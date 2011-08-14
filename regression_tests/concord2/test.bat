@echo off
SETLOCAL

IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\debug
SET concord="%SNAPDIR%\concord"
SET coordsysdef=cstest/coordsys.def
dir %concord%.*

del /q out\*.*

%concord% -Z > out\test_version.out
%concord% -L > out\test_crdsys.out

echo basic conversion with and without output file


%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,H -N6 in\test1.in out\test1.out > out\test1.txt
%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,H -N6 in\test1.in  > out\test2.txt


echo XYZ conversion
%concord% -iNZGD2000,NEH,H -oNZGD2000_XYZ -N6 in\test1.in out\test3.out > out\test3.txt 

echo TM projection
%concord% -iNZGD2000,NEH,H -oWELLTM2000,NEH -N6 in\test1.in out\test4.out > out\test4.txt 

echo NZMG projection
%concord% -iNZGD1949,NEH,H -oNZMG,NEH -N6 in\test1.in out\test5.out > out\test5.txt 

echo LCC projection
%concord% -iWGS84,NEH,H -oST57-60_LCC,NEH -N6 in\test1.in out\test6.out > out\test6.txt 


echo PS projection
%concord% -iWGS84,NEH,H -oANT_PS,NEH -N6 in\test1.in out\test7.out > out\test7.txt 

echo Geoid calculation
%concord% -iNZGD2000,NEH,H -oNZGD2000,NEO,H -gnzgtest09 -N6 in\test1.in out\test8.out > out\test8.txt

echo Default geoid - egm96 in this case
%concord% -iNZGD2000,NEH,H -oNZGD2000,NEO,H -N6 in\test1.in out\test8a.out > out\test8a.txt

echo Geoid calculation - invalid geoid
%concord% -iNZGD2000,NEH,H -oNZGD2000,NEO,H -gNoSuchGeoid -N6 in\test1.in out\test8b.out > out\test8b.txt


echo Different output options

%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,M -N6 in\test1.in out\test9.out > out\test9.txt

%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,D -N6 in\test1.in out\test10.out > out\test10.txt

%concord% -iNZGD2000,NEH,H -oNZGD2000,ENH -N6 in\test1.in out\test11.out > out\test11.txt 

%concord% -INZGD2000,ENH,D -oNZGD2000,ENO,H -gnzgtest09 -p5 in\test.lln out\test12.out > out\test12.txt

echo Reference frame conversions

echo Bursa wolf
%concord% -iNZGD2000,NEH,H -oWGS84BW,NEH,D -P8 -N6 in\test1.in out\test13.out > out\test13.txt

echo Grid
%concord% -iNZGD2000,NEH,H -oNZGD1949,NEH,D -P8 -N6 in\test1.in  out\test14.out > out\test14.txt

echo Reference frame grid deformation

%concord% -l NZGD2000@2010.0 > out\test15.txt
echo epoch = 0
%concord% -iNZGD2000,NE,D -oNZTM_D,EN -P4 -N6 in\test15.in out\test15a.out >> out\test15.txt
echo epoch = 2000.0
%concord% -iNZGD2000,NE,D -oNZTM_D@2000.0,EN -P4 -N6 in\test15.in out\test15b.out >> out\test15.txt
echo epoch = 2010.0
%concord% -iNZGD2000D,NE,D -oNZTM_D@2010.0,EN -P4 -N6 in\test15.in out\test15c.out >> out\test15.txt

echo Test each coordinate system with official coordsysdef file

set coordsysdef=

for /F %%c in ( crdsyslist.txt ) do (

   %concord% -INZGD2000,NEH,H -o%%c -N6 -P6 in\test1.in out\test_%%c.out > out\temp.txt
   )
