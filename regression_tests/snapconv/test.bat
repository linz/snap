@echo off

SETLOCAL
IF "%1" == "-v" (
  SHIFT
  echo on
  )
IF "%1" == "-i" SET SNAPDIR=C:\Program Files (x86)\Land Information New Zealand\SNAP
IF "%1" == "-i" SHIFT
IF "%1" == "-r" SET SNAPDIR=..\..\ms\built\Releasex86
IF "%1" == "-r" SHIFT

SET t=%1
IF "%t%" == "" SET t=test*

del /f /q out\*

IF "%SNAPDIR%" == "" SET SNAPDIR=..\..\ms\built\Debugx86


echo Null conversion
"%SNAPDIR%\snapconv" in\test1.crd NZGD2000 out\test1a.crd > out\test1a.log

echo Conversion to projections
"%SNAPDIR%\snapconv" in\test1.crd NZTM out\test1b.crd > out\test1b.log
"%SNAPDIR%\snapconv" in\test2.crd NZTM out\test2b.crd > out\test2b.log

echo Conversion to XYZ
"%SNAPDIR%\snapconv" in\test1.crd NZGD2000_XYZ out\test1c.crd > out\test1c.log
"%SNAPDIR%\snapconv" in\test2.crd NZGD2000_XYZ out\test2c.crd > out\test2c.log

echo Conversion NZGD1949 datum
"%SNAPDIR%\snapconv" in\test1.crd NZGD1949 out\test1d.crd > out\test1d.log
"%SNAPDIR%\snapconv" in\test2.crd NZGD1949 out\test2d.crd > out\test2d.log

echo Conversion to decimal degrees
"%SNAPDIR%\snapconv" -d in\test1.crd NZGD2000 out\test1e.crd > out\test1e.log

echo Conversions involving deformation model
"%SNAPDIR%\snapconv" -y 2015-01-02 -d in\test1.crd ITRF2008 out\test1f.crd > out\test1f.log
"%SNAPDIR%\snapconv" -y 2005-01-02 -d in\test1.crd ITRF2008 out\test1g.crd > out\test1g.log
"%SNAPDIR%\snapconv" -y 2005-01-02 -d in\test2.crd ITRF2008 out\test2g.crd > out\test2g.log
"%SNAPDIR%\snapconv" -d in\test1.crd ITRF2008 out\test1h.crd > out\test1h.log

echo Testing date formats
"%SNAPDIR%\snapconv" -y 20050102 -d in\test1.crd ITRF2008 out\test1i.crd > out\test1g.log
"%SNAPDIR%\snapconv" -y 02012005 -d in\test1.crd ITRF2008 out\test1j.crd > out\test1g.log
"%SNAPDIR%\snapconv" -y 02-01-2005 -d in\test1.crd ITRF2008 out\test1k.crd > out\test1g.log
"%SNAPDIR%\snapconv" -y 02-JAN-2005 -d in\test1.crd ITRF2008 out\test1l.crd > out\test1g.log
"%SNAPDIR%\snapconv" -y 2005 -d in\test1.crd ITRF2008 out\test1m.crd > out\test1g.log
"%SNAPDIR%\snapconv" -y 2005.2 -d in\test1.crd ITRF2008 out\test1n.crd > out\test1g.log

echo Conversions involving height reference surface
"%SNAPDIR%\snapconv" in\test1.crd NZGD2000/NZVD09 out\test1o.crd > out\test1o.log
"%SNAPDIR%\snapconv" in\test3.crd NZGD2000/NZVD09 out\test3o.crd > out\test3o.log
"%SNAPDIR%\snapconv" in\test4.crd NZGD2000/NZVD09 out\test4o.crd > out\test4o.log
"%SNAPDIR%\snapconv" -p in\test1.crd NZGD2000/NZVD09 out\test1p.crd > out\test1p.log
"%SNAPDIR%\snapconv" -p in\test3.crd NZGD2000/NZVD09 out\test3p.crd > out\test3p.log
"%SNAPDIR%\snapconv" -p in\test4.crd NZGD2000/NZVD09 out\test4p.crd > out\test4p.log
"%SNAPDIR%\snapconv" -o in\test3.crd NZGD2000/NZVD09 out\test3q.crd > out\test3q.log

echo Setting output height type
"%SNAPDIR%\snapconv" -o in\test1.crd NZGD2000 out\test1r.crd > out\test1r.log
"%SNAPDIR%\snapconv" -e in\test1.crd NZGD2000 out\test1s.crd > out\test1s.log
"%SNAPDIR%\snapconv" -e in\test5.crd NZGD2000 out\test5r.crd > out\test5r.log
"%SNAPDIR%\snapconv" -o in\test5.crd NZGD2000 out\test5s.crd > out\test5s.log


for %%f in (out\*.log) do perl -pi.bak -e s/snapconv[\s\d\.]*\:/snapconv:/ %%f 
del /q out\*.bak

echo =============================================
echo Differences between generated and check files
diff -r -b -B -q out check
