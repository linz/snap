#!/bin/bash

if [ -z "$DISTRIBUTION" ]; then
    DISTRIBUTION=noble
fi

scriptfile=`realpath $0`
scriptdir=`dirname $scriptfile`
basedir=`dirname $scriptdir`
rootdir=`dirname $basedir`
packagedir=$basedir/debs
pkgname=linz-snap
buildimage=snap-builder:$DISTRIBUTION

uid=`id -u`
gid=`id -g`
docker run --rm  \
    -v "$rootdir:/home" -u $uid:$gid \
    $buildimage \
    bash -c "cd /home/snap && python3 build.py release package --build-dir /tmp/buildsnap"

# Crude copy to desired location
debfile=`find $rootdir -maxdepth 1 -name $pkgname_*.deb -mmin -1`; 
if [ -n "$debfile" -a -f "$debfile" ]; then
  debname=`basename $debfile .deb`
  mkdir -p $packagedir/$DISTRIBUTION
  mv $debfile $packagedir/$DISTRIBUTION/
  rm $rootdir/$debname*
  echo "Package file at $packagedir/$DISTRIBUTION/$debname.deb"
else
  echo "Package not built"
fi

