@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
perl -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl
:WinNT
perl -x -S %0 %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#!perl
#line 15
use strict;
use Archive::Zip qw/ :ERROR_CODES /;
use FindBin;

chdir($FindBin::Bin);

my $target = 'packages';
my @root = ();
my @package = ();
my $package;

while(<DATA>)
{
  next if /^\s*(\#|$)/;
  last if /^\s*end\s*$/;
  if( /^\s*root\s+(\S.*?)\s*$/ ) { push(@root,$1); }
  elsif ( /^\s*package\s+(\S+)\s*$/ ) {
     my $p = { name=>$1, description=>$1, files=>[] };
     $package = $p;
     push(@package,$p);
     }
  elsif( /^\s*description\s+(\S.*?)\s*$/ && $package) { 
     $package->{description} = $1;
     }
  elsif( /^\s*description\s*$/ && $package ) {
     my $d='';
     while(<DATA>) {
         last if /^\s*end_description\s*$/;
         $d .= $_;
         }
     $package->{description} = $d;
     }
  elsif( /^\s*file\s+(\S.*?)\s*$/ && $package) { 
     my $file = $1;
     if( ! -r $file )
     { 
        print "Cannot find file $file of package $package->{name}\n";
        $package->{error} = 1;
     }
     push(@{$package->{files}},$file);
     }
  else {
     print "Invalid package record $_";
     }
}

foreach my $pkg (@package)
{
   next if $pkg->{error};
   my $pkgfile = $target.'/'.$pkg->{name}.'.zip';
   my $description = $pkg->{description};
   my $zip = new Archive::Zip;

   $zip->zipfileComment("SNAP: $description");
   foreach my $source (@{$pkg->{files}})
   {
      my $target = $source;
      foreach my $r (@root)
      {
         my $l = length($r);
         $target = substr($target,$l) if substr($target,0,$l) eq $r;
      }
      $target =~ s/^\///;
      $zip->addFile($source,$target);
   }
   my $status = $zip->writeToFileNamed($pkgfile);
   print "Cannot create package $pkgfile\n" if $status != AZ_OK;
}

__END__

root src/snap_manager
root src/perl
root src/snaplist
root src/snap/config

package devel
description 
Development functions for snap_manager scripting
end_description
file src/snap_manager/scripts/devel_menu.cfg
file src/snap_manager/scripts/test_menu.cfg
file src/snap_manager/scripts/testcrash.txt

package linz
description
LINZ functions -
  * Landonline adjustment DUMPSQL import function
  * Advanced coordinate order calculation options 

Restart SNAP to access this menu.
end_description
file src/perl/lol2snap.bat
file src/perl/lol2snap.bat.refdata
file src/snap_manager/scripts/linz_menu.cfg
file src/snap_manager/scripts/lol_menu.cfg

package snapdynanet
description 
Dynanet XML files import function

Creates a new menu item File | Import | Dynanet XML file which will
read a Dynanet XML files (station, observation, or mixed) and 
generate SNAP command, coordinate and observations files.  

Restart SNAP to access this menu.
end_description 
file src/snap_manager/scripts/dynanet_menu.cfg
file src/perl/DynanetXMLToSNAP.bat
file src/perl/perllib/DynanetXML.pm

package snapnewgan
description
Newgan import function

Creates a new menu item File | Import | Newgan file which will
read a Newgan input file and convert it to SNAP command, coordinate
and observations files.  

This is a "beta" level product .. it has only had limited testing
on a few Newgan input files.

Restart SNAP to access this menu.
end_description
file src/snap_manager/scripts/newgan_menu.cfg
file src/perl/NewganToSnap.bat
file src/perl/perllib/NewGanFile.pm
file src/perl/perllib/Geodetic/CoordSys.pm
file src/perl/perllib/Geodetic/CoordConversion.pm
file src/perl/perllib/Geodetic/ProjectionCrd.pm
file src/perl/perllib/Geodetic/GeodeticCrd.pm
file src/perl/perllib/Geodetic/Datum.pm
file src/perl/perllib/Geodetic/Ellipsoid.pm
file src/perl/perllib/Geodetic/TMProjection.pm

package contractor
description
Menu options for LINZ geodetic contractors

Creates menu items for creating contract data test adjustments
(File | Import | Contract data), for updating contract mark data
files with GDB coordinates (Stations | Update MDFC from GDB), and 
update contract mark data files with calculated coordinates
(Adjust | Update MDFC coord file).

Restart SNAP to access this menu.
end_description
file src/snap_manager/scripts/contractor_menu.cfg
file src/snap_manager/scripts/contractor_command_file.template
file src/snap/config/mdfc1.dtf
file src/snap/config/vecc1.dtf
file src/snaplist/mdfc1.tbf
end

:endofperl
