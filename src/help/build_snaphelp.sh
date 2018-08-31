#!/bin/sh
cp snaphelp.hhc contents.hhc
cp snaphelp.hhk index.hhk
zip -q -r snaphelp.zip index.hhk contents.hhc snaphelp.hhp snaphelp.hhc snaphelp.hhk help
rm contents.hhc
rm index.hhk
