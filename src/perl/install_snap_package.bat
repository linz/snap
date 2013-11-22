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
use Archive::Zip qw/ :ERROR_CODES /;

die "Need zip file parameter\n" if ! $ARGV[0];
my $zip = new Archive::Zip;

my @errors = ();

eval
{

Archive::Zip::setErrorHandler( sub { push(@errors,$_[0]); } );

my $status = $zip->read($ARGV[0]);
die "Cannot open $ARGV[0]\n" if $status != AZ_OK;

my $comment = $zip->zipfileComment();
die "$ARGV[0] is not a SNAP package\n"
  if $comment !~ /^SNAP_PACKAGE_V2\:/;

my $target=$ARGV[1] || $FindBind::Bin."../package";
$target .= "/";

$zip->extractTree('',$target);

$comment =~ s/^SNAP_PACKAGE_V2\:\s+//g;
print "Successfully installed $comment\n";
print "You may need to restart snap to complete the installation\n";
};
if( $@ || @errors)
{
   print "$ARGV[0] could not be installed\n";
   print $@ if $@;
   print @errors if @errors;
}

__END__
:endofperl
