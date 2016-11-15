#!/bin/sh

if [ -z $SNAPDIR ]; then
SNAPDIR='../../unix/debug/install'
fi

if [ "$1" = "-r" ]; then 
    echo ""Testing release version""
    shift
    SNAPDIR='../../unix/release/install'
fi

rm -f out/*
mkdir -p out

echo "Null conversion"
"${SNAPDIR}/snapconv" in/test1.crd NZGD2000 out/test1a.crd > out/test1a.log

echo "Conversion to projections"
"${SNAPDIR}/snapconv" in/test1.crd NZTM out/test1b.crd > out/test1b.log
"${SNAPDIR}/snapconv" in/test2.crd NZTM out/test2b.crd > out/test2b.log

echo "Conversion to XYZ"
"${SNAPDIR}/snapconv" in/test1.crd NZGD2000_XYZ out/test1c.crd > out/test1c.log
"${SNAPDIR}/snapconv" in/test2.crd NZGD2000_XYZ out/test2c.crd > out/test2c.log

echo "Conversion NZGD1949 datum"
"${SNAPDIR}/snapconv" in/test1.crd NZGD1949 out/test1d.crd > out/test1d.log
"${SNAPDIR}/snapconv" in/test2.crd NZGD1949 out/test2d.crd > out/test2d.log

echo "Conversion to decimal degrees"
"${SNAPDIR}/snapconv" -d in/test1.crd NZGD2000 out/test1e.crd > out/test1e.log

echo "Conversions involving deformation model"
"${SNAPDIR}/snapconv" -y 2015-01-02 -d in/test1.crd ITRF2008 out/test1f.crd > out/test1f.log
"${SNAPDIR}/snapconv" -y 2005-01-02 -d in/test1.crd ITRF2008 out/test1g.crd > out/test1g.log
"${SNAPDIR}/snapconv" -y 2005-01-02 -d in/test2.crd ITRF2008 out/test2g.crd > out/test2g.log
"${SNAPDIR}/snapconv" -d in/test1.crd ITRF2008 out/test1h.crd > out/test1h.log

echo "Testing date formats"
"${SNAPDIR}/snapconv" -y 20050102 -d in/test1.crd ITRF2008 out/test1i.crd > out/test1g.log
"${SNAPDIR}/snapconv" -y 02012005 -d in/test1.crd ITRF2008 out/test1j.crd > out/test1g.log
"${SNAPDIR}/snapconv" -y 02-01-2005 -d in/test1.crd ITRF2008 out/test1k.crd > out/test1g.log
"${SNAPDIR}/snapconv" -y 02-JAN-2005 -d in/test1.crd ITRF2008 out/test1l.crd > out/test1g.log
"${SNAPDIR}/snapconv" -y 2005 -d in/test1.crd ITRF2008 out/test1m.crd > out/test1g.log
"${SNAPDIR}/snapconv" -y 2005.2 -d in/test1.crd ITRF2008 out/test1n.crd > out/test1g.log


for f in out/*.log; do 
    perl -pi -e "s/snapconv[\\s\\d\\.]*/snapconv/" $f
    perl -ni -e "print if ! /Electric Fence/" $f
done

echo "=========================================="
echo "Differences between output and check files"
diff -r -b -B -q out check
