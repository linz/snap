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
#o='> /dev/null'

if [ "$1" != "" ]; then
    t=test${1}
fi

mkdir -p valgrind
rm -f valgrind/*

for f in in/${t}.snp; do
    base=`basename $f .snp`
    echo "Running ${base}"
    (valgrind --track-origins=yes ${SNAPDIR}/snap 2>&1 ) | grep '^=='  > valgrind/${base}-valgrind.log 2>&1
done

