rem @echo off
SETLOCAL

IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\debug

del /q in\*.bin >null 2>&1
del /q in\*.lst >null 2>&1
del /q in\*.new >null 2>&1
del /q out\*.lst >null 2>&1

set test=T%1
if %test% == T set test=all
goto %test%

:all
:t1
echo Running test 1
"%SNAPDIR%\snap" in\snapspec1.snp
"%SNAPDIR%\snapspec" in\snapspec1.bin in\snapspec1.cfg out\snapspec1.lst
if NOT %test% == all goto done

:t2
echo Running test 2
"%SNAPDIR%\snap" in\snapspec2.snp
"%SNAPDIR%\snapspec" in\snapspec2.bin in\snapspec2.cfg out\snapspec2.lst
if NOT %test% == all goto done

:t3
echo Running test 3
"%SNAPDIR%\snap" in\snapspec3.snp
"%SNAPDIR%\snapspec" in\snapspec3.bin in\snapspec3.cfg out\snapspec3.lst
if NOT %test% == all goto done

:t4
echo Running test 4
"%SNAPDIR%\snap" in\snapspec4.snp
"%SNAPDIR%\snapspec" in\snapspec4.bin in\snapspec4.cfg out\snapspec4.lst
if NOT %test% == all goto done

:t5
echo Running test 5
"%SNAPDIR%\snap" in\snapspec5.snp
"%SNAPDIR%\snapspec" in\snapspec5.bin in\snapspec5.cfg out\snapspec5.lst
if NOT %test% == all goto done

:t6
echo Running test 6
"%SNAPDIR%\snap" in\snapspec6.snp
"%SNAPDIR%\snapspec" in\snapspec6.bin in\snapspec6.cfg out\snapspec6.lst
if NOT %test% == all goto done

:t7
echo Running test 7
"%SNAPDIR%\snap" in\snapspec7.snp
"%SNAPDIR%\snapspec" in\snapspec7.bin in\snapspec7.cfg out\snapspec7.lst
"%SNAPDIR%\snapspec" -o 5 in\snapspec7.bin in\snapspec7.cfg out\snapspec7a.lst
"%SNAPDIR%\snapspec" -a in\snapspec7.bin in\snapspec7.cfg out\snapspec7b.lst
"%SNAPDIR%\snapspec" -a in\snapspec7.bin in\snapspec7d.cfg out\snapspec7d.lst
"%SNAPDIR%\snapspec" -a in\snapspec7.bin in\snapspec7e.cfg out\snapspec7e.lst
"%SNAPDIR%\snapspec" -a in\snapspec7.bin in\snapspec7f.cfg out\snapspec7f.lst
"%SNAPDIR%\snapspec" -a in\snapspec7.bin in\snapspec7g.cfg out\snapspec7g.lst
"%SNAPDIR%\snap" in\snapspec7h.snp
"%SNAPDIR%\snapspec" -a in\snapspec7h.bin in\snapspec7.cfg out\snapspec7h.lst
if NOT %test% == all goto done

:t8
echo Running test 8
"%SNAPDIR%\snap" in\snapspec8.snp
"%SNAPDIR%\snapspec" in\snapspec8.bin in\snapspec8.cfg out\snapspec8.lst
if NOT %test% == all goto done

:t9
echo Running test 9
"%SNAPDIR%\snap" in\snapspec9.snp
"%SNAPDIR%\snapspec" in\snapspec9.bin in\snapspec9.cfg out\snapspec9.lst
if NOT %test% == all goto done

:t10
echo Running test 10
"%SNAPDIR%\snap" in\snapspec10.snp
"%SNAPDIR%\snapspec" in\snapspec10.bin in\snapspec10.cfg out\snapspec10.lst
if NOT %test% == all goto done

:done
if "%2" == "-k" goto end
del in\*.bin >nul 2>&1
del in\*.new >nul 2>&1
:end

perl cleanlist.pl out/*.lst
del out\*.bak > nul 2>&1

