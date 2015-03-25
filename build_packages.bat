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
#!/usr/bin/perl
#line 15
use strict;
use Archive::Zip qw/ :ERROR_CODES /;
use FindBin;
use File::Find;

chdir($FindBin::Bin);

my $source = 'src/packages';
my $target = 'packages';
my @package = ();
my $package;


my $pos=length($source)+1;
foreach my $pkgd ( glob("$source/*") )
{
    next if ! -d $pkgd;
    next if ! -f "$pkgd/ABOUT";
    my $pkg = substr($pkgd,$pos);
    print "===========================\n";
    print "Building package $pkg\n";

    my @files=();
    find(sub { push(@files,$File::Find::name) if -f $_ } , $pkgd );

    open(my $df,"$pkgd/ABOUT");
    my $description=join('',<$df>);
    close($df);

    my $pkgfile="$target/$pkg.zip";
    
    my $zip=new Archive::Zip;

   $zip->zipfileComment("SNAP_PACKAGE_V2: $description");

   print "Description: $description";
   foreach my $sourcefile (@files)
   {
      my $target = substr($sourcefile,$pos);
      $target =~ s/^\///;
      print "Adding $sourcefile as $target\n";
      $zip->addFile($sourcefile,$target);
   }
   my $status = $zip->writeToFileNamed($pkgfile);
   print "Cannot create package $pkgfile\n" if $status != AZ_OK;
}

__END__
:endofperl
