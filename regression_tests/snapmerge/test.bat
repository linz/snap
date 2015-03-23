
SETLOCAL

IF "%1" == "-i" SET SNAPDIR=C:\Program Files (x86)\Land Information New Zealand\SNAP
IF "%1" == "-i" SHIFT
IF "%1" == "-r" SET SNAPDIR=..\..\ms\built\release
IF "%1" == "-r" SHIFT
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

REM clearing orders
%SNAPDIR%\snapmerge in/test.crd in/testaltord.crd out/test20.crd
%SNAPDIR%\snapmerge -cb in/test.crd in/testaltord.crd out/test21.crd
%SNAPDIR%\snapmerge -cd in/test.crd in/testaltord.crd out/test22.crd
%SNAPDIR%\snapmerge -c in/test.crd in/testaltord.crd out/test23.crd
%SNAPDIR%\snapmerge -cd in/test.crd in/testnoord.crd out/test24.crd

