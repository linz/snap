
SETLOCAL

IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\debug

%SNAPDIR%\snapmerge in/test.crd in/testll.crd out/test1.crd
%SNAPDIR%\snapmerge -o in/test.crd in/testll.crd out/test2.crd
%SNAPDIR%\snapmerge -u in/test.crd in/testll.crd out/test3.crd
%SNAPDIR%\snapmerge -l in/test.lst in/test.crd in/testll.crd out/test4.crd
%SNAPDIR%\snapmerge in/testllg.crd in/testll.crd out/test5.crd
%SNAPDIR%\snapmerge -o in/testllg.crd in/testll.crd out/test6.crd
copy in\test.crd out\test7.crd
%SNAPDIR%\snapmerge -o out/test7.crd in/testll.crd

%SNAPDIR%\snapmerge in/testnoord.crd in/test.crd out/test8.crd
%SNAPDIR%\snapmerge in/test.crd in/testnoord.crd out/test9.crd
%SNAPDIR%\snapmerge in/testnoord.crd in/test2class.crd out/test10.crd
%SNAPDIR%\snapmerge in/test2class.crd in/test.crd out/test11.crd

