#!/usr/bin/perl
use strict;

my ($y,$m,$d) = (localtime)[5,4,3];
$y += 1900;
$m++;
my $version = sprintf("%04d%02d%02d",$y,$m,$d);
my $zip = "backup/snapwin$version.zip";
unlink $zip;
system("c:\\bin\\zip",
   "-r",
   "$zip",
   "src",
   "ms/projects",
   "regression_tests",
   "-x","*.ncb",
   "-x","*LZ101651*",
   "-x","ms/projects/wxtest/debug/*"
   );
