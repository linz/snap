#!/bin/sh

if [ -z $SNAPDIR ]; then
SNAPDIR='../../../unix/debug/install'
fi

# SET t=%1
# IF "%t%" == "" SET t=test*
t='*'
g=''
#o='> /dev/null'
if [ "$1" = "-g" ] ; then
    echo "Debugging"
    shift
    g='gdb --args '
    o=''
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
    rm -f ../out/${base}.*1
    ${g}${SNAPDIR}/snap $base ${o}
    #> /dev/null
    for of in ${base}.lst ${base}.err ${base}-*.csv; do
        if [ -e ${of} ]; then
            perl ../clean_snap_listing.pl ${of} > ../out/${of}
        fi
        rm $of
    done
done
