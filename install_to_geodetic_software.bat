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
use FindBin;
use File::Copy;

my $gf = '//ad/dfs/Sites/LH/GROUP/NORA/Geodetic/Software/Snap';
my $sf = $FindBin::Bin;
my $pt = $gf.'/packages';
my $ps = $sf.'/packages';

my $si = $sf.'/ms/install/snap/release/snap_install.msi';
my $ti = $gf.'/snap_install.msi';

eval
{
die "$si doesn't exist\n" if ! -e $si;

if( -e $ti )
{
   my $smt = (stat($si))[9];
   my $tmt = (stat($ti))[9];
   die "$ti is already up to date\n" if $tmt >= $smt;
   my ($y,$m,$d) = (localtime($tmt))[5,4,3];
   my $save = $gf.'/old_releases/snap_install_'.
           sprintf("%04d%02d%02d",$y+1900,$m+1,$d).'.msi';
   print "Saving old snap_install.msi\n";
   move($ti,$save);
}
print "Copying new version of snap_install.msi\n";
copy($si,$ti);
};

opendir(my $dir, $ps);

while( my $pkg = readdir($dir))
{
   next if $pkg !~ /\.zip$/;
   print "Copying package $pkg\n";
   copy($ps.'/'.$pkg,$pt.'/'.$pkg); 
}
closedir($dir);

__END__
:endofperl
