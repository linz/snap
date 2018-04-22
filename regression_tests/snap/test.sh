#!/bin/sh

if [ -z $SNAPDIR ]; then
SNAPDIR='../../../unix/debug/install'
fi

if [ "$1" = "-r" ]; then 
    echo "Testing release version"
    shift
    SNAPDIR='../../../unix/release/install'
fi

# SET t=%1
# IF "%t%" == "" SET t=test*
t='*'
o='/dev/null'

g=''
if [ "$1" = "-g" ] ; then
    echo "Debugging"
    shift
    g='gdb --args '
    o='/dev/stdout'
fi

domake=1
if [ "$1" = "-f" ] ; then
    shift
    domake=0
fi

docheck=1
if [ "$1" != "" ]; then
    t=test${1}
    docheck=0
fi

typedir=`dirname $SNAPDIR`
typedir=`basename $typedir`

if [ -n $typedir -a $domake = 1 ]; then
    echo "Checking snap build"
    (cd ../../unix && make type=$typedir snap_cmd)
fi

mkdir -p out
rm -f out/*
cd in

echo "Using ${SNAPDIR}"

for f in ${t}.snp; do
    base=`basename $f .snp`
    param=""
    if [ $base = "testmt" ]; then
        param="-t 4"
    fi
    if [ $base = "test1z" ]; then
        param="-z"
    fi
    echo "Running ${base} ${param}"
    rm -f ../out/${base}.* ../out/${base}-*
    ${g}${SNAPDIR}/snap $param $base > ${o}
    #> /dev/null
    for of in ${base}.lst ${base}.err ${base}-*.csv ${base}*.json ${base}.snx *.cvr ${base}.newcrd; do
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

if [ ${docheck} -gt 0 ]; then
    cd ..
    diff -q -r -B -b out check_unix
fi
