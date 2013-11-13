@echo off
SETLOCAL

IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\debug
SET concord="%SNAPDIR%\concord"
SET coordsysdef=cstest\coordsys.def
dir %concord%.*

del /q out\*.*

%concord% -Z > out\test_version.out
%concord% -L > out\test_crdsys.out

REM Example descriptive outputs 

%concord% -L NZGD2000 > out\test_list_NZGD2000.out 2>&1
%concord% -L NZGD2000_XYZ > out\test_list_NZGD2000_XYZ.out 2>&1

echo basic conversion with and without output file

%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,H -N6 in\test1.in out\test1.out > out\test1.txt
%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,H -N6 in\test1.in  > out\test2.txt

REM Invalid defintions 

%concord% -L BAD1 > out\test_list_BAD1.out 2>&1
%concord% -L BAD2 > out\test_list_BAD2.out 2>&1
%concord% -L BAD4 > out\test_list_BAD4.out 2>&1

echo basic conversion with and without output file

%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,H -N6 in\test1.in out\test1.out > out\test1.txt 2>&1
%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,H -N6 in\test1.in  > out\test2.txt 2>&1

echo XYZ conversion
%concord% -iNZGD2000,NEH,H -oNZGD2000_XYZ -N6 in\test1.in out\test3.out > out\test3.txt  2>&1

echo TM projection
%concord% -iNZGD2000,NEH,H -oWELLTM2000,NEH -N6 in\test1.in out\test4.out > out\test4.txt  2>&1

echo NZMG projection
%concord% -iNZGD1949,NEH,H -oNZMG,NEH -N6 in\test1.in out\test5.out > out\test5.txt  2>&1

echo LCC projection
%concord% -iWGS84,NEH,H -oST57-60_LCC,NEH -N6 in\test1.in out\test6.out > out\test6.txt  2>&1


echo PS projection
%concord% -iWGS84,NEH,H -oANT_PS,NEH -N6 in\test1.in out\test7.out > out\test7.txt  2>&1

echo Geoid calculation
%concord% -iNZGD2000,NEH,H -oNZGD2000,NEO,H -gcstest\NZgTest09 -N6 in\test1.in out\test8.out > out\test8.txt 2>&1

echo Default geoid - egm96 in this case
rem (Test no longer valid - just loadng file directly)
%concord% -iNZGD2000,NEH,H -oNZGD2000,NEO,H -gcstest\geoid -N6 in\test1.in out\test8a.out > out\test8a.txt 2>&1

echo Geoid calculation - invalid geoid
%concord% -iNZGD2000,NEH,H -oNZGD2000,NEO,H -gNoSuchGeoid -N6 in\test1.in out\test8b.out > out\test8b.txt 2>&1


echo Different output options

%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,M -N6 in\test1.in out\test9.out > out\test9.txt 2>&1

%concord% -iNZGD2000,NEH,H -oNZGD2000,NEH,D -N6 in\test1.in out\test10.out > out\test10.txt 2>&1

%concord% -iNZGD2000,NEH,H -oNZGD2000,ENH -N6 in\test1.in out\test11.out > out\test11.txt  2>&1

%concord% -INZGD2000,ENH,D -oNZGD2000,ENO,H -gcstest\NZgTest09 -p5 in\test.lln out\test12.out > out\test12.txt 2>&1

echo Reference frame conversions

echo Bursa wolf
%concord% -iNZGD2000,NEH,H -oIERSBW,NEH,D -P8 -N6 in\test1.in out\test13.out > out\test13.txt 2>&1

echo Grid
%concord% -iNZGD2000,NEH,H -oNZGD1949,NEH,D -P8 -N6 in\test1.in  out\test14.out > out\test14.txt 2>&1

echo Reference frame grid deformation

%concord% -l NZGD2000D@2010.0 > out\test15.txt 2>&1
echo epoch = 0 >> out\test15.txt
%concord% -iNZGD2000D,NE,D -oNZTM_D,EN -P4 -N6 in\test15.in out\test15a.out >> out\test15.txt 2>&1
echo epoch = 2000.0 >> out\test15.txt
%concord% -iNZGD2000D,NE,D -oNZTM,EN -P4 -N6 -Y2000 in\test15.in out\test15b.out >> out\test15.txt 2>&1
echo epoch = 2010.0 >> out\test15.txt
%concord% -iNZGD2000D,NE,D -oNZTM,EN -Y2010 -P4 -N6 in\test15.in out\test15c.out >> out\test15.txt 2>&1
echo epoch = 2010.0 - same datum >> out\test15.txt
%concord% -iNZGD2000D,NE,D -oNZTM_D,EN -Y2010 -P4 -N6 in\test15.in out\test15d.out >> out\test15.txt 2>&1
echo Converting epochs - same datum >> out\test15.txt
%concord% -iNZGD2000D@2000,NE,D -oNZTM_D@2010,EN -P4 -N6 in\test15.in out\test15e.out >> out\test15.txt 2>&1

echo Testing Bursa Wolf 14 param

%concord% -L TESTBW14_XYZ > out\test16.txt 2>&1
echo Reference XYZ coords >> out\test16.txt
%concord% -INZGD2000,NEH,H -oNZGD2000_XYZ -P4 -N6 in\test1.in out\test16a.out >> out\test16.txt 2>&1
echo epoch = 2000.0 >> out\test16.txt
%concord% -INZGD2000,NEH,H -oTESTBW14_XYZ -P4 -N6 -Y2000 in\test1.in out\test16b.out >> out\test16.txt 2>&1
echo epoch = 2010.0 >> out\test16.txt
%concord% -INZGD2000,NEH,H -oTESTBW14_XYZ -P4 -N6 -Y2010 in\test1.in out\test16c.out >> out\test16.txt 2>&1
echo no epoch >> out\test16.txt
%concord% -INZGD2000,NEH,H -oTESTBW14_XYZ -P4 -N6 in\test1.in out\test16d.out >> out\test16.txt 2>&1

echo Test IERS version of parameters
%concord% -L IERSBW_XYZ > out\test17.txt
echo IERS version of ref frame transformation >> out\test17.txt
%concord% -L IERSBWE_XYZ >> out\test17.txt
echo IERS version of ref frame transformation >> out\test17.txt
%concord% -INZGD2000,NEH,H -oIERSBW_XYZ -P4 -N6 in\test1.in out\test17a >> out\test17.txt 2>&1
echo IERS version of ref frame transformation >> out\test17.txt
%concord% -INZGD2000,NEH,H -oIERSBWE_XYZ -P4 -N6 in\test1.in out\test17b >> out\test17.txt 2>&1

echo Test each coordinate system with official coordsysdef file

set coordsysdef=

for /F %%c in ( crdsyslist.txt ) do (

   echo ======================= >> out\crdsys.txt
   echo Testing %%c >> out\crdsys.txt
   %concord% -L %%c > out\crdsys_list_%%c.txt 2>&1
   %concord% -INZGD2000,NEH,H -o%%c -N6 -P6 in\test1.in out\test_%%c.out >> out\crdsys.txt 2>&1
   )


for /F %%c in ( crdsyslist2.txt ) do (
   echo ======================= >> out\crdsys.txt
   echo Testing %%c >> out\crdsys.txt
   %concord% -L %%c > out\crdsys_list_%%c.txt 2>&1
   %concord% -IITRF96,NEH,H -o%%c -N6 -P6 in\test1.in out\test_%%c.out >> out\crdsys.txt 2>&1
   )

REM List ITRF coordinate systems

echo Testing ITRF coordinate systems

echo ===== ITRF96 ===== > out\itrf.txt
%concord% -L ITRF96 >> out\itrf.txt 2>&1
echo ===== ITRF96_XYZ ===== >> out\itrf.txt
%concord% -L ITRF96_XYZ >> out\itrf.txt 2>&1
echo ===== ITRF2000 ===== >> out\itrf.txt
%concord% -L ITRF2000 >> out\itrf.txt 2>&1
echo ===== ITRF2000_XYZ ===== >> out\itrf.txt
%concord% -L ITRF2000_XYZ >> out\itrf.txt 2>&1
echo ===== ITRF2005 ===== >> out\itrf.txt
%concord% -L ITRF2005 >> out\itrf.txt 2>&1
echo ===== ITRF2005_XYZ ===== >> out\itrf.txt
%concord% -L ITRF2005_XYZ >> out\itrf.txt 2>&1
echo ===== ITRF2008 ===== >> out\itrf.txt
%concord% -L ITRF2008 >> out\itrf.txt 2>&1
echo ===== ITRF2008_XYZ ===== >> out\itrf.txt
%concord% -L ITRF2008_XYZ >> out\itrf.txt 2>&1

echo ITRF96 to ITRF2000 without defining an epoch >> out\itrf.txt
%concord% -IITRF96_XYZ -oITRF2000_XYZ -N6 -P4 in\global.xyz out\test_bad >> out\itrf.txt 2>&1

echo ITRF96 to ITRF2000 >> out\itrf.txt
%concord% -IITRF96_XYZ -oITRF2000_XYZ -Y2000 -N6 -P4 in\global.xyz out\test_ITRF2000b.out >> out\itrf.txt 2>&1
%concord% -IITRF96_XYZ -oITRF2000_XYZ -Y2010 -N6 -P4 in\global.xyz out\test_ITRF2000c.out >> out\itrf.txt 2>&1

echo ITRF96 to ITRF2005 >> out\itrf.txt
%concord% -IITRF96_XYZ -oITRF2005_XYZ -Y2000 -N6 -P4 in\global.xyz out\test_ITRF2005b.out >> out\itrf.txt 2>&1
%concord% -IITRF96_XYZ -oITRF2005_XYZ -Y2010 -N6 -P4 in\global.xyz out\test_ITRF2005c.out >> out\itrf.txt 2>&1

echo ITRF96 to ITRF2008 >> out\itrf.txt
%concord% -IITRF96_XYZ -oITRF2008_XYZ -Y2000 -N6 -P4 in\global.xyz out\test_ITRF2008b.out >> out\itrf.txt 2>&1
%concord% -IITRF96_XYZ -oITRF2008_XYZ -Y2010 -N6 -P4 in\global.xyz out\test_ITRF2008c.out >> out\itrf.txt 2>&1

echo NZGD2000 to ITRF96 >> out\itrf.txt
%concord% -INZGD2000,NE,D -oITRF96,NEH,D -Y2000 -N8 -P8 in\test15.in out\test_ITRF96d.out >> out\itrf.txt 2>&1
%concord% -INZGD2000,NE,D -oITRF96,NEH,D -Y2010 -N8 -P8 in\test15.in out\test_ITRF96e.out >> out\itrf.txt 2>&1

echo NZGD2000 to ITRF2000 >> out\itrf.txt
%concord% -INZGD2000,NE,D -oITRF2000,NEH,D -Y2000 -N8 -P8 in\test15.in out\test_ITRF2000d.out >> out\itrf.txt 2>&1
%concord% -INZGD2000,NE,D -oITRF2000,NEH,D -Y2010 -N8 -P8 in\test15.in out\test_ITRF2000e.out >> out\itrf.txt 2>&1

echo NZGD2000 to ITRF2005 >> out\itrf.txt
%concord% -INZGD2000,NE,D -oITRF2005,NEH,D -Y2000 -N8 -P8 in\test15.in out\test_ITRF2005d.out >> out\itrf.txt 2>&1
%concord% -INZGD2000,NE,D -oITRF2005,NEH,D -Y2010 -N8 -P8 in\test15.in out\test_ITRF2005e.out >> out\itrf.txt 2>&1

echo NZGD2000 to ITRF2008 >> out\itrf.txt
%concord% -INZGD2000,NE,D -oITRF2008,NEH,D -Y2000 -N8 -P8 in\test15.in out\test_ITRF2008d.out >> out\itrf.txt 2>&1
%concord% -INZGD2000,NE,D -oITRF2008,NEH,D -Y2010 -N8 -P8 in\test15.in out\test_ITRF2008e.out >> out\itrf.txt 2>&1

perl fix_output.pl out\*.*
del /q out\*.bak

