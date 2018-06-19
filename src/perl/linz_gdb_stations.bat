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
getopts("mv",\%opts);
my $addmarktype = $opts{m} ? 'addmarktypes' : '';
my $verbose = $opts{v};

my $maxperrequest=1000;

@ARGV == 2 || die <<"EOD";

Syntax  
   linz_gdb_stations [-v] [-m] geodetic_codes_file output_coord_file

Build a SNAP coordinate file from a file containing a list of 
geodetic codes.  The input file should just contain one geodetic code
on each line.  Lines in which the first word is not
a geodetic code are ignored.  

Uses the web page $lookup_coords_url

Options are:
   -m    Include mark type in the output file
   -v    Verbose output

EOD

my($crdfile0,$crdfile1) = @ARGV;

open(IN,"<$crdfile0") || die "Invalid input file $crdfile0 specified\n";
my $header=<IN>;
my $coordsys='';
my $line='\n';

if( $header !~ /^.*\bORDV1\b.*\bORDV2\b.*\bORDV3\b/i )
{
    while($coordsys eq '')
    {
        $line=<IN>;
        last if ! $line;
        next if $line =~ /^\s*(\!|$)/;
        $coordsys=$line;
    }
}

my @codes=();
while( $line )
{
    push(@codes,uc($1)) if $line =~ /^\s*(\w{4})\W/;
    $line=<IN>;
}

close(IN);

die "No geodetic codes found in $crdfile0\n" if ! @codes;

# Stop buffering of standard output so that updates can be seen
$|=1 if $verbose;

my $ncodes=scalar(@codes);

use LWP::UserAgent;
use HTTP::Request::Common;

my $ua = LWP::UserAgent->new;
$ua->env_proxy;

my $crddata='';
my $crdheader='';

for( my $i0=0; $i0 < $ncodes; $i0 += $maxperrequest )
{
    my @subset=grep /\w/,@codes[$i0 .. $i0+$maxperrequest-1];
    if( $verbose )
    {
        print "Requesting ".scalar(@subset)." stations from GDB - ".($i0+1)." ...\n";
    }

    my $in=join("\n",$header,$coordsys,@subset);

    my $response = $ua->request(
           POST $lookup_coords_url,
            { input_file => $in,
              cbMarkType => $addmarktype,
              Go => 'Go'
              }
           );
         
    die $response->message."\n" if ! $response->is_success;
                        
    my $output=$response->content;
    # Snap format
    if( $output =~ /^.*\n(:?NZGD2000|RSRGD2000)\n/ )
    {
        $output =~s/^((.*\n){4})//;
        $crdheader=$1 if ! $crdheader 
    }
    elsif( $output =~ /^.*\bORDV1\b.*\bORDV2\b.*\bORDV3\b/i )
    {
        $output=~s/^(.*\n)//;
        $crdheader=$1 if ! $crdheader 
    }
    else
    {
        die "Cannot read coordinates from GDB\n$output" 
    }
    my %outputcrd={};
    foreach my $crd (split(/\n/,$output))
    {
        $outputcrd{uc($2)}=$crd."\n" if $crd=~/^\!?\s*(\"?)(\w{4})\1\W/;
    }

    foreach my $code (@subset)
    {
        $crddata .= $outputcrd{$code} if exists $outputcrd{$code};
    }
}
      
open(OUT,">$crdfile1") || die "Cannot create coordinate file $crdfile1\n";
print OUT $crdheader,$crddata;
close(OUT);

print "Coordinates successfully updated from LINZ GDB\n";


__END__
:endofperl
