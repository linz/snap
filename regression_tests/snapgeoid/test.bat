SETLOCAL

IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\debug

rem basic conversion with and without coordinate system conversion

%SNAPDIR%\snapgeoid -g nzgeoid05 in/test.crd   out/test1.crd
%SNAPDIR%\snapgeoid -g nzgeoid05 in/test49.crd   out/test2.crd

rem different and invalid geoid model

%SNAPDIR%\snapgeoid -g egm96 in/test.crd   out/test3.crd
%SNAPDIR%\snapgeoid -g garbage in/test.crd   out/test4.crd > out/test4.err

rem updating geoid information

%SNAPDIR%\snapgeoid -g nzgeoid05 out/test3.crd   out/test5.crd
%SNAPDIR%\snapgeoid -c -g nzgeoid05 out/test3.crd  out/test6.crd


rem removing heights and converting coordinates

%SNAPDIR%\snapgeoid -x -g nzgeoid05 out/test1.crd  out/test7.crd
%SNAPDIR%\snapgeoid -o -g nzgeoid05 out/test1.crd  out/test8.crd
%SNAPDIR%\snapgeoid -o -g nzgeoid05 out/test7.crd  out/test8a.crd
%SNAPDIR%\snapgeoid -e -g nzgeoid05 out/test8.crd  out/test9.crd

rem default geoid

%SNAPDIR%\snapgeoid in/test.crd  out/test10.crd

rem out of range coordinates

%SNAPDIR%\snapgeoid -g nzgeoid05 in/test2.crd   out/test11.crd

