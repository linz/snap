#!/bin/perl -pi.bak
BEGIN { @ARGV=map(glob,  @ARGV); }
s/(\s)\-(0\.0+)(?![0-9])/$1 $2/g;
s/\\coordsys/\/coordsys/;
s/\s+$/\n/;
