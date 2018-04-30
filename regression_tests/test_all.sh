#!/bin/sh

scriptfile=`readlink -f "$0"`
scriptdir=`dirname $scriptfile`

if [ "$1" = "-r" ]; then
    export SNAPDIR=${scriptdir}/../linux/release/install
fi

if [  -z "$SNAPDIR" ]; then
    export SNAPDIR=${scriptdir}/../linux/debug/install
fi

for f in `ls -d ${scriptdir}/*/check`; do
    if [ -d $f ]; then
        testdir=`dirname $f`
        testname=`basename $testdir`
        echo "Testing $testname"
        if [ -x ${testdir}/test.sh ]; then
            (cd $testdir && ./test.sh) >> /dev/null 2>&1
            checkdir=$f
            if [ -d ${testdir}/check_linux ]; then
                checkdir=${testdir}/check_linux
            fi
            diff -r -b -B -q $checkdir ${testdir}/out
        else
            echo "No executable test.sh for $testname"
        fi
    fi
done
