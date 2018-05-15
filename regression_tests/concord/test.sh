#!/bin/sh

if [ -z $SNAPDIR ]; then
SNAPDIR='/home/ccrook/projects_git/snap/linux/debug/install'
fi

if [ "$1" = "-r" ]; then 
    echo ""Testing release version""
    shift
    SNAPDIR='/home/ccrook/projects_git/snap/linux/release/install'
fi

mkdir -p out
rm -f out/*

concord=${SNAPDIR}/concord
export COORDSYSDEF=cstest/coordsys.def

echo "Running ${concord}"

${concord} -Z > out/test_version.out
${concord} -L > out/test_crdsys.out
${concord} -V > out/test_vdatum.out

# Example descriptive outputs 

${concord} -L NZGD2000 > out/test_list_NZGD2000.out 2>&1
${concord} -L NZGD2000_XYZ > out/test_list_NZGD2000_XYZ.out 2>&1

# Invalid definitions 

${concord} -L BAD1 > out/test_list_BAD1.out 2>&1
${concord} -L BAD2 > out/test_list_BAD2.out 2>&1
${concord} -L BAD4 > out/test_list_BAD4.out 2>&1

echo "basic conversion with and without output file"

${concord} -iNZGD2000,NEH,H -oNZGD2000,NEH,H -N6 in/test1.in out/test1.out > out/test1.txt 2>&1
${concord} -iNZGD2000,NEH,H -oNZGD2000,NEH,H -N6 in/test1.in  > out/test2.txt 2>&1

echo "XYZ conversion"
${concord} -iNZGD2000,NEH,H -oNZGD2000_XYZ -N6 in/test1.in out/test3.out > out/test3.txt  2>&1

echo "TM projection"
${concord} -iNZGD2000,NEH,H -oWELLTM2000,NEH -N6 in/test1.in out/test4.out > out/test4.txt  2>&1

echo "NZMG projection"
${concord} -iNZGD1949,NEH,H -oNZMG,NEH -N6 in/test1.in out/test5.out > out/test5.txt  2>&1

echo "LCC projection"
${concord} -iWGS84_D,NEH,H -oST57-60_LCC,NEH -N6 in/test1.in out/test6.out > out/test6.txt  2>&1


echo "PS projection"
${concord} -iWGS84_D,NEH,H -oANT_PS,NEH -N6 in/test1.in out/test7.out > out/test7.txt  2>&1

echo "Geoid calculation"
${concord} -iNZGD2000,NEH,H -oNZGD2000,NEO,H -gcstest/nzgtest09 -N6 in/test1.in out/test8.out > out/test8.txt 2>&1

echo "Default geoid - egm96 in this case"
# (Test no longer valid - just loadng file directly)
${concord} -iNZGD2000,NEH,H -oNZGD2000,NEO,H -gcstest/geoid -N6 in/test1.in out/test8a.out > out/test8a.txt 2>&1

echo "Geoid calculation - invalid geoid"
${concord} -iNZGD2000,NEH,H -oNZGD2000,NEO,H -gNoSuchGeoid -N6 in/test1.in out/test8b.out > out/test8b.txt 2>&1

echo "Height reference surface"
${concord} -iNZGD2000,NEH,H -oNZGD2000/NZVD09,NEO,H -N6 in/test1.in out/test8c.out > out/test8c.txt 2>&1

echo "Height reference surface - ok with O or H"
${concord} -iNZGD2000,NEH,H -oNZGD2000/NZVD09,NEH,H -N6 in/test1.in out/test8d.out > out/test8d.txt 2>&1

echo "Height reference surface - reverse transformation"
${concord} -iNZGD2000/NZVD09,NEH,H -oNZGD2000,NEH,H -N6 in/test1.in out/test8e.out > out/test8e.txt 2>&1

echo "Height reference surface with offset"
${concord} -iNZGD2000,NEH,H -oNZGD2000/OFFSET1,NEH,H -N6 in/test1.in out/test8f.out > out/test8f.txt 2>&1

echo "Height reference surface with offset 2"
${concord} -iNZGD2000,NEH,H -oNZGD2000/OFFSET2,NEH,H -N6 in/test1.in out/test8g.out > out/test8g.txt 2>&1

echo "Height reference surface with grid offset" 
${concord} -iNZGD2000,NEH,H -oNZGD2000/GRIDOFFSET,NEH,H -N6 in/test1.in out/test8h.out > out/test8h.txt 2>&1

echo "Conversion between height systems 1" 
${concord} -iNZGD2000/NZVD09,NEH,H -oNZGD2000/OFFSET1,NEH,H -N6 in/test1.in out/test8i.out > out/test8i.txt 2>&1

echo "Conversion between height systems 2" 
${concord} -iNZGD2000/OFFSET1,NEH,H -oNZGD2000/NZVD09,NEH,H -N6 in/test1.in out/test8j.out > out/test8j.txt 2>&1

echo "Conversion between height systems 3" 
${concord} -iNZGD2000/OFFSET1,NEH,H -oNZGD2000/OFFSET2,NEH,H -N6 in/test1.in out/test8k.out > out/test8k.txt 2>&1

echo "Conversion between height systems 4" 
${concord} -iNZGD2000/OFFSET1,NEH,H -oNZGD2000/OFFSET1,NEH,H -N6 in/test1.in out/test8l.out > out/test8l.txt 2>&1

echo "Different output options"

${concord} -iNZGD2000,NEH,H -oNZGD2000,NEH,M -N6 in/test1.in out/test9.out > out/test9.txt 2>&1

${concord} -iNZGD2000,NEH,H -oNZGD2000,NEH,D -N6 in/test1.in out/test10.out > out/test10.txt 2>&1

${concord} -iNZGD2000,NEH,H -oNZGD2000,ENH -N6 in/test1.in out/test11.out > out/test11.txt  2>&1

${concord} -INZGD2000,ENH,D -oNZGD2000,ENO,H -gcstest/nzgtest09 -p5 in/test.lln out/test12.out > out/test12.txt 2>&1

echo "Reference frame conversions"

echo "Bursa wolf"
${concord} -iNZGD2000,NEH,H -oIERSBW,NEH,D -P8 -N6 in/test1.in out/test13.out > out/test13.txt 2>&1

echo "Grid"
${concord} -iNZGD2000,NEH,H -oNZGD1949,NEH,D -P8 -N6 in/test1.in  out/test14.out > out/test14.txt 2>&1

echo "Reference frame grid deformation"

${concord} -l NZGD2000D@2010.0 > out/test15.txt 2>&1
echo "epoch = 0" >> out/test15.txt
${concord} -iNZGD2000D,NE,D -oNZTM_D,EN -P4 -N6 in/test15.in out/test15a.out >> out/test15.txt 2>&1
echo "epoch = 2000.0" >> out/test15.txt
${concord} -iNZGD2000D,NE,D -oNZTM,EN -P4 -N6 -Y2000 in/test15.in out/test15b.out >> out/test15.txt 2>&1
echo "epoch = 2010.0" >> out/test15.txt
${concord} -iNZGD2000D,NE,D -oNZTM,EN -Y2010 -P4 -N6 in/test15.in out/test15c.out >> out/test15.txt 2>&1
echo "epoch = 2010.0 - same datum" >> out/test15.txt
${concord} -iNZGD2000D,NE,D -oNZTM_D,EN -Y2010 -P4 -N6 in/test15.in out/test15d.out >> out/test15.txt 2>&1
echo "Converting epochs - same datum" >> out/test15.txt
${concord} -iNZGD2000D@2000,NE,D -oNZTM_D@2010,EN -P4 -N6 in/test15.in out/test15e.out >> out/test15.txt 2>&1

echo "Testing Bursa Wolf 14 param"

${concord} -L TESTBW14_XYZ > out/test16.txt 2>&1
echo "Reference XYZ coords" >> out/test16.txt
${concord} -INZGD2000,NEH,H -oNZGD2000_XYZ -P4 -N6 in/test1.in out/test16a.out >> out/test16.txt 2>&1
echo "epoch = 2000.0" >> out/test16.txt
${concord} -INZGD2000,NEH,H -oTESTBW14_XYZ -P4 -N6 -Y2000 in/test1.in out/test16b.out >> out/test16.txt 2>&1
echo "epoch = 2010.0" >> out/test16.txt
${concord} -INZGD2000,NEH,H -oTESTBW14_XYZ -P4 -N6 -Y2010 in/test1.in out/test16c.out >> out/test16.txt 2>&1
echo "no epoch" >> out/test16.txt
${concord} -INZGD2000,NEH,H -oTESTBW14_XYZ -P4 -N6 in/test1.in out/test16d.out >> out/test16.txt 2>&1

echo "Test IERS version of parameters"
${concord} -L IERSBW_XYZ > out/test17.txt
echo "IERS version of ref frame transformation" >> out/test17.txt
${concord} -L IERSBWE_XYZ >> out/test17.txt
echo "IERS version of ref frame transformation" >> out/test17.txt
${concord} -INZGD2000,NEH,H -oIERSBW_XYZ -P4 -N6 in/test1.in out/test17a >> out/test17.txt 2>&1
echo "IERS version of ref frame transformation" >> out/test17.txt
${concord} -INZGD2000,NEH,H -oIERSBWE_XYZ -P4 -N6 in/test1.in out/test17b >> out/test17.txt 2>&1

echo "Testing notes..."
${concord} -iWGS84,NEH,H -oNZGD2000,NEH -F -N6 in/test1.in out/test20.out > out/test20.txt  2>&1
${concord} -iNZGD2000,NEH,H -oWGS84,NEH -F -N6 in/test1.in out/test21.out > out/test21.txt  2>&1
${concord} -iNZGD2000,NEH,H -oNZMG,NE -F -P8 -N6 in/test1.in  out/test22.out > out/test22.txt 2>&1
${concord} -iNZGD1949,NEH,H -oNZGD2000,NE -F -P8 -N6 in/test1.in  out/test23.out > out/test23.txt 2>&1

echo Testing separator"
"
${concord} -INZGD2000,NE,D -oNZGD2000,NE,H -N8 -S, -P8 in/testsep.in out/test30.out >> out/test30.txt 2>&1
${concord} -INZGD2000,NE,D -oNZGD2000,NE,H -E -N8 -S, -P8 in/testsep.in out/test31.out >> out/test31.txt 2>&1

echo Test each coordinate system with official COORDSYSDEF file

unset COORDSYSDEF

for c in `cat crdsyslist.txt`; do
   echo "=======================" >> out/crdsys.txt
   echo "Testing ${c}" >> out/crdsys.txt
   ${concord} -L ${c} > out/crdsys_list_${c}.txt 2>&1
   ${concord} -INZGD2000,NEH,H -o${c} -N6 -P6 in/test1.in out/test_${c}.out >> out/crdsys.txt 2>&1
done


for c in `cat crdsyslist2.txt`; do
   echo "=======================" >> out/crdsys.txt
   echo "Testing ${c}" >> out/crdsys.txt
   ${concord} -L ${c} > out/crdsys_list_${c}.txt 2>&1
   ${concord} -IITRF96,NEH,H -o${c} -Y2000.5 -N6 -P6 in/test1.in out/test_${c}.out >> out/crdsys.txt 2>&1
done

echo "Testing NZGD2000 versions"
for v in `cat in/test_nzgd2000.version`; do
    echo "Testing version $v"
    echo "Testing NZGD2000 version $v" >> out/crdsys.txt 2>&1
    for y in `cat in/test_nzgd2000.year`; do
        ${concord} -iITRF96:ENH:D -oNZGD2000_$v:ENH:D -Y$y -P9 in/test_nzgd2000.in out/testnzgd2000_${v}_${y}.out >> out/crdsys.txt 2>&1
    done
done

echo "Testing height reference surfaces"
for c in `cat hgtreflist.txt`; do
   echo "=======================" >> out/hgtref.txt
   echo "Testing ${c}" >> out/hgtref.txt
   ${concord} -L NZGD2000/${c} > out/hgtref_list_${c}.txt 2>&1
   ${concord} -INZGD2000/${c},ENH,D -oNZGD2000,ENH,D -P4:3 in/nzpoints.in out/testhrs_${c}.out >> out/hgtref.txt 2>&1
done

echo "Testing ITRF systems"
for c in `cat itrf_csys.txt`; do
   echo "========== ${c} ============" >> out/itrf.txt
   ${concord} -L ${c} >> out/itrf.txt 2>&1
   echo "ITRF96 to ${c}" >> out/itrf.txt
   ${concord} -IITRF96_XYZ -o${c}_XYZ -Y2000 -N6 -P4 in/global.xyz out/test_${c}b.out >> out/itrf.txt 2>&1
   ${concord} -IITRF96_XYZ -o${c}_XYZ -Y2010 -N6 -P4 in/global.xyz out/test_${c}c.out >> out/itrf.txt 2>&1
   echo "NZGD2000 to ${c}" >> out/itrf.txt
   ${concord} -INZGD2000,NE,D -o${c},NEH,D -Y2000 -N8 -P8 in/test15.in out/test_${c}d.out >> out/itrf.txt 2>&1
   ${concord} -INZGD2000,NE,D -o${c},NEH,D -Y2010 -N8 -P8 in/test15.in out/test_${c}e.out >> out/itrf.txt 2>&1
done

# Australian coordinate systems

echo "Testing Australian systems"
for c in `cat aus_csys.txt`; do
   echo "========== ${c} ============" >> out/aus.txt
   ${concord} -L ${c} >> out/aus.txt 2>&1
   echo "GDA94 to ${c}" >> out/aus.txt
   ${concord} -IGDA94:NEH:D -o${c}:ENH:D -Y2000 -N8 -P8 in/aus.in out/test_${c}b.out >> out/aus.txt 2>&1
   ${concord} -IGDA94:NEH:D -o${c}:ENH:D -Y2010 -N8 -P8 in/aus.in out/test_${c}c.out >> out/aus.txt 2>&1
done
for c in `cat aus_proj.txt`; do
   echo "========== ${c} ============" >> out/aus.txt
   ${concord} -L ${c} >> out/aus.txt 2>&1
   echo "GDA94 to ${c}" >> out/aus.txt
   ${concord} -IGDA94:NEH:D -o${c}:ENH -Y2000 -N8 -P4 in/aus.in out/test_${c}b.out >> out/aus.txt 2>&1
   ${concord} -IGDA94:NEH:D -o${c}:ENH -Y2010 -N8 -P4 in/aus.in out/test_${c}c.out >> out/aus.txt 2>&1
done
for c in `cat aus_xyz.txt`; do
   echo "========== ${c} ============" >> out/aus.txt
   ${concord} -L ${c} >> out/aus.txt 2>&1
   echo "GDA94 to ${c}" >> out/aus.txt
   ${concord} -IGDA94:NEH:D -o${c} -Y2000 -N8 -P4 in/aus.in out/test_${c}b.out >> out/aus.txt 2>&1
   ${concord} -IGDA94:NEH:D -o${c} -Y2010 -N8 -P4 in/aus.in out/test_${c}c.out >> out/aus.txt 2>&1
done


perl fix_output.pl out/*.*
perl -n -i -e 'print if ! /Electric/' out/*.txt out/*.out
rm -f out/*.bak

diff -r -b -B -q check out


