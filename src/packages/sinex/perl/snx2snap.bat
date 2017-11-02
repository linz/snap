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
#!/bin/perl
#line 15
#
#  Script to convert a SINEX file to a SNAP data file.

use FindBin;
use lib $FindBin::Bin.'/perllib';
use LINZ::GNSS::SinexFile;
use Getopt::Std;

my $syntax=<<EOD;

Syntax: snx2snap.pl [-t tolerance] sinex_file snap_data_file

Convert a SINEX file to a SNAP format file. 

Options:

   -t tol   The maximum variation of observation epoch time
            tolerated in the SINEX file in hours (default 1)

EOD

my %opts;
getopts('t:',\%opts);
my $tolerance=$opts{t} || 1;
$tolerance *= 3600.0;

@ARGV==2 || die $syntax;

my( $snxfile, $datfile) = @ARGV;

die "Cannot open input SINEX file $snxfile\n" if ! -r $snxfile;

my $sf = new LINZ::GNSS::SinexFile($snxfile,full_covariance=>1);

my @stations=$sf->stations;

# Check the SINEX file is valid - must have only one solution for each
# station, with each solution at the same time...

my $minepoch=0;
my $maxepoch=0;
my %codes=();
my %dupcodes=();

foreach my $stn (@stations)
{
    my $code=uc($stn->{code});
    my $epoch=$stn->{epoch};
    $dupcodes{$code}=1 if exists $codes{$code};
    $codes{$code}=1;
    $minepoch = $epoch if $minepoch == 0 || $epoch < $minepoch;
    $maxepoch = $epoch if $epoch > $maxepoch;
}

if( %dupcodes )
{
    my $codes = join(', ',sort keys %dupcodes);
    die "Cannot handle multiple solutions for a mark in the SINEX file\n".
        "Multiple solutions for $codes\n";
}

if( $maxepoch - $minepoch > $tolerance )
{
    my ($dmin,$tmin)=date_time($minepoch);
    my ($dmax,$tmax)=date_time($maxepoch);
    die "Cannot handle solutions at different epochs in the SINEX file\n".
        "Solutions from $dmin $tmin to $dmax $tmax\n";
}

my $sfn=$snxfile;
$sfn =~ s/.*[\\\/]//;
my ($date,$time)=date_time(($minepoch+$maxepoch)/2.0);

open( my $df, ">$datfile") || die "Cannot open SNAP data file $datfile\n";
print $df "Data from SINEX File $sfn\n\n";
print $df "#date $date\n#time $time\n\n";
print $df "#gps_error_type full\n";
print $df "#data GX error no_heights grouped\n\n";

foreach my $stn (@stations)
{
    my $code = $stn->{code};
    my ($x,$y,$z) = @{$stn->{xyz}};
    print $df "$code $x $y $z\n";
}
print $df "#end_set\n";
my $covar=$sf->covar();

foreach my $row (@{$covar})
{
    foreach my $i (0..$#$row)
    {
        print $df "\n" if  $i%3 == 0; 
        printf $df " %-14s",$row->[$i];
    }
    print $df "\n";
}
close($df);

sub date_time
{
    my ($epoch) = @_;
    my ($sec,$min,$hour,$day,$mon,$year) = localtime($epoch);
    $year += 1900;
    $mon = (qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec))[$mon];
    $date=sprintf("%02d %3s %04d",$day,$mon,$year);
    $time=sprintf("%02d:%02d:%02d",$hour,$min,$sec);
    return $date, $time;
}

__END__
:endofperl
