SETLOCAL


IF "%1" == "-i" SET SNAPDIR=C:\Program Files (x86)\Land Information New Zealand\SNAP
IF "%1" == "-i" SHIFT
IF "%1" == "-r" SET SNAPDIR=..\..\ms\built\Releasex86
IF "%1" == "-r" SHIFT
IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\Debugx86

del /s /q out\*

rem basic conversion with and without coordinate system conversion

echo == 1 ============== > out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -g nzgeoid09 in/test.crd   out/test1.crd >> out/snapgeoid.txt 2>&1
echo == 2 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -g nzgeoid09 in/test49.crd   out/test2.crd >> out/snapgeoid.txt 2>&1

rem different and invalid geoid model

echo == 3 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -g egm96 in/test.crd   out/test3.crd >> out/snapgeoid.txt 2>&1
echo == 4 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -g garbage in/test.crd   out/test4.crd > out/test4.err 2>&1

rem updating geoid information

echo == 5 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -k -g nzgeoid09 in/test3.crd   out/test5.crd >> out/snapgeoid.txt 2>&1
echo == 6 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -g nzgeoid09 in/test3.crd  out/test6.crd >> out/snapgeoid.txt 2>&1

rem removing heights and converting coordinates

echo == 7 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -z in/test1.crd  out/test7.crd >> out/snapgeoid.txt 2>&1
echo == 8 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -o in/test1.crd  out/test8.crd >> out/snapgeoid.txt 2>&1
echo == 8a ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -o -g nzgeoid09 in/test7.crd  out/test8a.crd >> out/snapgeoid.txt 2>&1
echo == 8b ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -o -g nzgeoid09 in/test1.crd  out/test8b.crd >> out/snapgeoid.txt 2>&1
echo == 8c ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -p -o -g nzgeoid09 in/test1.crd  out/test8c.crd >> out/snapgeoid.txt 2>&1
echo == 9 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -e -g nzgeoid09 in/test8.crd  out/test9.crd >> out/snapgeoid.txt 2>&1

rem default geoid - no longer supported
rem 
rem echo == 10 ============== >> out/snapgeoid.txt 
rem export GEOID=nzgeoid09
rem "%SNAPDIR%\snapgeoid" in/test.crd  out/test10.crd >> out/snapgeoid.txt 2>&1

rem out of range coordinates

echo == 11 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -g nzgeoid09 in/test2.crd   out/test11.crd >> out/test11.err 2>&1

echo == 11a ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -i -g nzgeoid09 in/test2.crd   out/test11a.crd >> out/test11a.err 2>&1

rem Using height reference surface

echo == 12 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -h nzvd09 in/test.crd   out/test12.crd >> out/snapgeoid.txt 2>&1
echo == 13 ============== >> out/snapgeoid.txt 
"%SNAPDIR%\snapgeoid" -h nzvd09 in/test49.crd   out/test13.crd >> out/snapgeoid.txt 2>&1

for %%f in (out\*.err) do perl -pi.bak -e "s/snapgeoid\s+version[\s\d\.]*/snapgeoid version/" %%f 
perl -pi.bak -e "s/snapgeoid\s+version[\s\d\.]*/snapgeoid version/" out\snapgeoid.txt

del /q out\*.bak

echo ==========================================
echo Differences between output and check files
diff -r -b -B -q out check
