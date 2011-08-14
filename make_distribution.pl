#!/usr/bin/perl
use strict;

my $zipexe = "c:\\bin\\zip.exe";
die "Cannot find zip program\n" if ! -e $zipexe;
mkdir 'distribution' if ! -d 'distribution';
my ($y,$m,$d) = (localtime)[5,4,3];
$y += 1900;
$m++;
my $version = sprintf("%04d%02d%02d",$y,$m,$d);
my $zip = "distribution/snap$version.zip";
unlink $zip;
system($zipexe,
   "-j",
   "$zip",
   "ms/install/snap/release/snap_install.msi",
   );
$zip = "distribution/concord$version.zip";
system($zipexe,
   "-j",
   "$zip",
   "ms/built/release/concord.exe",
   "src/coordsys/coordsys.def",
   "src/coordsys/def492kt.grd",
   "src/coordsys/egm96.grd",
   "src/coordsys/nzgeoid09.grd",
   "src/perl/makegrid.pl",
   "src/perl/dumpgrid.pl",
   "src/help/concord.chm",
   );
chdir('src/perl');
system($zipexe,
   "../../$zip",
   "perllib/Packer.pm"
   );
