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
#
# Script for using "web service" to locate generate a SNAP coordinate file
# from a file of station codes
#

use strict;

my $lookup_coords_url = 'http://apps.linz.govt.nz/gdb/update-snap-file.aspx';

use Getopt::Std;

my %opts;
getopts("m",\%opts);
my $addmarktype = $opts{m} ? 'addmarktypes' : '';

@ARGV == 2 || die <<'EOD';

Parameters: geodetic_codes_file output_coord_file

Build a SNAP coordinate file from a file containing a list of 
geodetic codes.  The input file should just contain one geodetic code
on each line.  Lines in which the first word is not
a geodetic code are ignored.  

Uses the update_snap_file.cgi web page

EOD

my($crdfile0,$crdfile1) = @ARGV;

open(IN,"<$crdfile0") || die "Invalid input file $crdfile0 specified\n";

my @in = <IN>;
my $in = join('',@in);
close(IN);

use LWP::UserAgent;
use HTTP::Request::Common;

my $ua = LWP::UserAgent->new;
my $response = $ua->request(
       POST $lookup_coords_url,
        { input_file => $in,
          cbMarkType => $addmarktype,
          Go => 'Go'
          }
       );
     
die $response->message."\n" if ! $response->is_success;
                    
die "Cannot read coordinates from GDB\n" if $response->content !~ /^.*\n(:?NZGD2000|RSRGD2000)\n/;
  
open(OUT,">$crdfile1") || die "Cannot create coordinate file $crdfile1\n";
print OUT $response->content;
close(OUT);


__END__
:endofperl
