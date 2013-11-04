SETLOCAL

IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\debug

del /s /q out/*

rem basic conversion with and without coordinate system conversion

echo ================ > out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -g nzgeoid05 in/test.crd   out/test1.crd >> out/snapgeoid.txt 2>&1
echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -g nzgeoid05 in/test49.crd   out/test2.crd >> out/snapgeoid.txt 2>&1

rem different and invalid geoid model

echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -g egm96 in/test.crd   out/test3.crd >> out/snapgeoid.txt 2>&1
echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -g garbage in/test.crd   out/test4.crd > out/test4.err >> out/snapgeoid.txt 2>&1

rem updating geoid information

echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -g nzgeoid05 out/test3.crd   out/test5.crd >> out/snapgeoid.txt 2>&1
echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -c -g nzgeoid05 out/test3.crd  out/test6.crd >> out/snapgeoid.txt 2>&1


rem removing heights and converting coordinates

echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -x -g nzgeoid05 out/test1.crd  out/test7.crd >> out/snapgeoid.txt 2>&1
echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -o -g nzgeoid05 out/test1.crd  out/test8.crd >> out/snapgeoid.txt 2>&1
echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -o -g nzgeoid05 out/test7.crd  out/test8a.crd >> out/snapgeoid.txt 2>&1
echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -e -g nzgeoid05 out/test8.crd  out/test9.crd >> out/snapgeoid.txt 2>&1

rem default geoid

echo ================ >> out\snapgeoid.txt 
SET GEOID=NZGEOID05
%SNAPDIR%\snapgeoid in/test.crd  out/test10.crd >> out/snapgeoid.txt 2>&1

rem out of range coordinates

echo ================ >> out\snapgeoid.txt 
%SNAPDIR%\snapgeoid -g nzgeoid05 in/test2.crd   out/test11.crd >> out/snapgeoid.txt 2>&1

