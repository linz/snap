#!/bin/sh
basedir=`dirname $0`
if [ -n "$basedir" ]; then
    cd $basedir
fi
perl build_help_files.pl snaphelp help -x gridfile_c
cp snaphelp.hhc contents.hhc
cp snaphelp.hhk index.hhk
rm -f snaphelp.zip
zip -q -r snaphelp.zip index.hhk contents.hhc snaphelp.hhp snaphelp.hhc snaphelp.hhk help
rm -f contents.hhc
rm -f index.hhk
