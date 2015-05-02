#!/bin/sh

if [ -z $SNAPDIR ]; then
SNAPDIR='/home/ccrook/projects/snap/unix/debug/install'
fi

if [ "$1" = "-r" ]; then 
    echo "Testing release version"
    shift
    SNAPDIR='/home/ccrook/projects/snap/unix/release/install'
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

if [ "$1" != "" ]; then
    t=test${1}
fi

mkdir -p out
rm -f out/*
cd in

for f in ${t}.snp; do
    base=`basename $f .snp`
    echo "Running ${base}"
    rm -f ../out/${base}.* ../out/${base}-*
    ${g}${SNAPDIR}/snap -q $base > ${o}
    #> /dev/null
    for of in ${base}.lst ${base}.err ${base}-*.csv ${base}*.json ${base}.snx *.cvr; do
        if [ -e ${of} ]; then
            perl ../clean_snap_listing.pl ${of} > ../out/${of}
            rm $of
        fi
    done
    for of in ${base}.bin; do
        if [ -e ${of} ]; then
            rm $of
        fi
    done
done

cd ..
diff -q -r -B -b out check_unix
