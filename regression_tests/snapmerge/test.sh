#!/bin/sh

if [ -z $snapdir ]; then
snapdir='../../linux/debug/install'
fi

if [ "$1" = "-r" ]; then 
    echo "Testing release version"
    shift
    snapdir='../../linux/release/install'
fi

mkdir -p out

echo "Using ${snapdir}"

echo test1
"${snapdir}/snapmerge" in/test.crd in/testll.crd out/test1.crd
echo test2
"${snapdir}/snapmerge" -o in/test.crd in/testll.crd out/test2.crd
echo test3
"${snapdir}/snapmerge" -u in/test.crd in/testll.crd out/test3.crd
echo test4
"${snapdir}/snapmerge" -l in/test.lst in/test.crd in/testll.crd out/test4.crd
echo test5
"${snapdir}/snapmerge" in/testllg.crd in/testll.crd out/test5.crd
echo test6
"${snapdir}/snapmerge" -o in/testllg.crd in/testll.crd out/test6.crd
cp in/test.crd out/test7.crd
echo test7
"${snapdir}/snapmerge" -o out/test7.crd in/testll.crd

echo test8
"${snapdir}/snapmerge" in/testnoord.crd in/test.crd out/test8.crd
echo test9
"${snapdir}/snapmerge" in/test.crd in/testnoord.crd out/test9.crd
echo test10
"${snapdir}/snapmerge" in/testnoord.crd in/test2class.crd out/test10.crd
echo test11
"${snapdir}/snapmerge" in/test2class.crd in/test.crd out/test11.crd

# updating crds, classes only
echo test12
"${snapdir}/snapmerge" -oc in/test.crd in/testupd.crd out/test12.crd
echo test13
"${snapdir}/snapmerge" -oa in/test.crd in/testupd1.crd out/test13.crd
echo test14
"${snapdir}/snapmerge" -u -oa in/test.crd in/testupd1.crd out/test14.crd
echo test15
"${snapdir}/snapmerge" -ua -oa in/test.crd in/testupd1.crd out/test15.crd

# clearing orders
echo test20
"${snapdir}/snapmerge" in/test.crd in/testaltord.crd out/test20.crd
echo test21
"${snapdir}/snapmerge" -cb in/test.crd in/testaltord.crd out/test21.crd
echo test22
"${snapdir}/snapmerge" -cd in/test.crd in/testaltord.crd out/test22.crd
echo test23
"${snapdir}/snapmerge" -c in/test.crd in/testaltord.crd out/test23.crd
echo test24
"${snapdir}/snapmerge" -cd in/test.crd in/testnoord.crd out/test24.crd

diff -r -b -B -q out check
