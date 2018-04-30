#!/bin/sh

if [ -z $SNAPDIR ]; then
SNAPDIR='/home/ccrook/projects/snap/linux/debug/install'
fi

if [ "$1" = "-r" ]; then 
    echo "Testing release version"
    shift
    SNAPDIR='/home/ccrook/projects/snap/linux/release/install'
fi

# SET t=%1
# IF "%t%" == "" SET t=test*
t='*'
g=''
o='/dev/null'
if [ "$1" = "-g" ] ; then
    echo "Debugging"
    shift
    g='gdb --args '
    o='/dev/stdout'
fi
if [ "$1" = "-v" ] ; then
    shift
    o='/dev/stdout'
fi

docheck=1
if [ "$1" != "" ]; then
    t=test${1}
    docheck=0
fi

mkdir -p out
rm -f out/*
cd in

echo "Using ${SNAPDIR}"

for f in ${t}.snp; do
    base=`basename $f .snp`
    echo "Running ${base}"
    rm -f ../in/*.new ../in/*.lst
    ${g}${SNAPDIR}/dat2site $base > ${o}
    cp ../in/*.new ../out/${base}.new
    cp ../in/*.lst ../out/
done

if [ ${docheck} -gt 0 ]; then
    cd ..
    diff -q -r -B -b out check
fi
