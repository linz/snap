#!/bin/sh

if [ "$1" = "-r" ]; then
   snapdir="../../unix/release/install"
   shift
fi

if [ "${snapdir}" = "" ]; then
   snapdir="../../unix/debug/install"
fi

echo "Testing dat2site in $snapdir"

rm -f in/*.bin
rm -f in/*.lst
rm -f in/*.new
rm -f out/*.lst

test="t$1"
if [ $test = "t" ]; then test="all"; fi

if [ $test = "t1" -o $test = "all" ]; then
echo Running test 1
${snapdir}/snap in/snapspec1.snp
${snapdir}/snapspec in/snapspec1.bin in/snapspec1.cfg out/snapspec1.lst
fi

if [ $test = "t2" -o $test = "all" ]; then
echo Running test 2
${snapdir}/snap in/snapspec2.snp
${snapdir}/snapspec in/snapspec2.bin in/snapspec2.cfg out/snapspec2.lst
fi

if [ $test = "t3" -o $test = "all" ]; then
echo Running test 3
${snapdir}/snap in/snapspec3.snp
${snapdir}/snapspec in/snapspec3.bin in/snapspec3.cfg out/snapspec3.lst
fi

if [ $test = "t4" -o $test = "all" ]; then
echo Running test 4
${snapdir}/snap in/snapspec4.snp
${snapdir}/snapspec in/snapspec4.bin in/snapspec4.cfg out/snapspec4.lst
fi

if [ $test = "t5" -o $test = "all" ]; then
echo Running test 5
${snapdir}/snap in/snapspec5.snp
${snapdir}/snapspec in/snapspec5.bin in/snapspec5.cfg out/snapspec5.lst
fi

if [ $test = "t6" -o $test = "all" ]; then
echo Running test 6
${snapdir}/snap in/snapspec6.snp
${snapdir}/snapspec in/snapspec6.bin in/snapspec6.cfg out/snapspec6.lst
fi

if [ $test = "t7" -o $test = "all" ]; then
echo Running test 7
${snapdir}/snap in/snapspec7.snp
${snapdir}/snapspec in/snapspec7.bin in/snapspec7.cfg out/snapspec7.lst
${snapdir}/snapspec -o 5 in/snapspec7.bin in/snapspec7.cfg out/snapspec7a.lst
${snapdir}/snapspec -a in/snapspec7.bin in/snapspec7.cfg out/snapspec7b.lst
${snapdir}/snapspec -a in/snapspec7.bin in/snapspec7d.cfg out/snapspec7d.lst
${snapdir}/snapspec -a in/snapspec7.bin in/snapspec7e.cfg out/snapspec7e.lst
${snapdir}/snapspec -a in/snapspec7.bin in/snapspec7f.cfg out/snapspec7f.lst
${snapdir}/snapspec -a in/snapspec7.bin in/snapspec7g.cfg out/snapspec7g.lst
${snapdir}/snap in/snapspec7h.snp
${snapdir}/snapspec -a in/snapspec7h.bin in/snapspec7.cfg out/snapspec7h.lst
fi

if [ $test = "t8" -o $test = "all" ]; then
echo Running test 8
${snapdir}/snap in/snapspec8.snp
${snapdir}/snapspec in/snapspec8.bin in/snapspec8.cfg out/snapspec8.lst
fi

if [ $test = "t9" -o $test = "all" ]; then
echo Running test 9
${snapdir}/snap in/snapspec9.snp
${snapdir}/snapspec in/snapspec9.bin in/snapspec9.cfg out/snapspec9.lst
fi

if [ $test = "t10" -o $test = "all" ]; then
echo Running test 10
${snapdir}/snap in/snapspec10.snp
${snapdir}/snapspec in/snapspec10.bin in/snapspec10.cfg out/snapspec10.lst
fi


if [ "$2" != "-k" ]; then
rm -f in/*.bin
rm -f in/*.new
fi

perl cleanlist.pl out/*.lst
rm -f out/*.bak 


diff -r -b -B -q out check
