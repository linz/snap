#!/usr/bin/perl
# Script to convert AusGeoid09 text files to SNAP ASCII grid file format,
# ready for conversion to binary.

use strict;

@ARGV==1 || die "Missing parameter: Need to specify state (eg NSW_ACT, VIC)\n";

my $agfile="AUSGeoid09_GDA94_V1.01_{state}_CLIP.txt";
my $snpfile="ausgeoid09_101_{state}.gtf";
my $header=join('',<DATA>);

my $state=uc($ARGV[0]);
foreach ($agfile,$snpfile,$header) { s/\{state\}/$state/g; }
$snpfile=lc($snpfile);

open(my $inf, "<", $agfile ) || die "Cannot open AusGeoid file $agfile\n";
my $xmin=99999.0;
my $xmax=-99999.0;
my $ymin=99999.0;
my $ymax=-99999.0;
my $vres=0.001;

my $data={};
my $lasty;

# Skip header
my $skip=<$inf>;

my $lastlt;
my $lastln;
my $currow;
my $ngridx=0;
my $ngridy=0;

while( my $line=<$inf> )
{
    next if $line !~ /^GEO([ 0-9\-]{5}\.\d\d\d)\sS(\d\d)\s(.\d)\s(.\d\.\d\d\d)\sE(\d\d\d)\s(.\d)\s(.\d\.\d\d\d)/;
    my ($gh,$lt,$ln)=($1,-($2+$3/60.0+$4/3600.0),$5+$6/60.0+$7/3600.0);
    $xmin=$ln if $ln < $xmin;
    $xmax=$ln if $ln > $xmax;
    if( $lt != $lastlt )
    {
        $ymin=$lt if $lt < $ymin;
        $ymax=$lt if $lt > $ymax;
        die "Irregular grid at $ln $lt\n" if $lastln && $lastln != $xmax;
        die "Irregular row at $ln $lt\n" if $ngridx && scalar(@$currow) != $ngridx;
        $ngridx=scalar(@$currow) if $currow;
        $ngridy++;
        $currow=[];
        $data->{$lt}=$currow;
        $lastlt=$lt;
    }
    $lastln=$ln;
    $gh=sprintf("%.0f",$gh/$vres);
    push(@$currow,$gh);
}
close($inf);

$header =~ s/\{ngridx\}/$ngridx/g;
$header =~ s/\{ngridy\}/$ngridy/g;
$header =~ s/\{xmin\}/$xmin/g;
$header =~ s/\{xmax\}/$xmax/g;
$header =~ s/\{ymin\}/$ymin/g;
$header =~ s/\{ymax\}/$ymax/g;
$header =~ s/\{vres\}/$vres/g;

open( my $outf, ">", $snpfile ) || die "Cannot open output file $snpfile\n";
print $outf $header;

my $nlt=0;
foreach my $lt (sort { $a <=> $b } keys %$data )
{
    $nlt++;
    my $nln=0;
    foreach my $gh (@{$data->{$lt}})
    {
        $nln++;
        print $outf "V$nln,$nlt: $gh\n";
    }
}
close($outf);

__DATA__
FORMAT: GRID2L
HEADER0: AUSGeoid09 V1.01 - {state}
HEADER1: Referenced to GDA94
HEADER2: Source: ftp://ftp.ga.gov.au/geodesy-outgoing/gravity/ausgeoid/
CRDSYS: GDA94
NGRDX: {ngridx}
NGRDY: {ngridy}
XMIN: {xmin}
XMAX: {xmax}
YMIN: {ymin}
YMAX: {ymax}
VRES: {vres}
NDIM: 1
LATLON: 1
VALUES: INTEGER
