# LINZ::Geodetic::MGRSMapRef
#
# This provides the functions for converting
# between MGRS map references and latitude/longitude coordinates

package LINZ::Geodetic::MGRSMapRef;

use strict;
use LINZ::Geodetic::TMProjection;
use LINZ::Geodetic::Ellipsoid qw/GRS80/;
use POSIX;

use vars qw/$lat_bands $row_ms $col_ms $nrow_ms $ncol_ms $ms $msband %utmproj/;

$lat_bands='CDEFGHJKLMNPQRSTUVWX';
$row_ms='ABCDEFGHJKLMNPQRSTUV';
$nrow_ms=length($row_ms);
$col_ms='ABCDEFGHJKLMNPQRSTUVWXYZ';
$ncol_ms=length($col_ms);
$ms=100000.0;
$msband=$ms*$nrow_ms;
%utmproj=();

sub gzd {
    my ($lon,$lat) = @_;
    die sprintf("Invalid latitude %.5f for MGRS\n",$lat) if $lat < -80.0 || $lat > 84.0;
    my $lb=int(($lat+80)/8.0);
    $lb=20 if $lb > 20;
    $lb=0 if $lb < 0;
    my $band=substr($lat_bands,$lb,1);
    $lon += 180;
    $lon += 360 while $lon < 0;
    my $zone=int($lon/6.0)+1;
    $zone -= 60 while $zone > 60;
    return $zone,$band;
    }

sub utmproj {
    my($zone,$band)=@_;
    my $zb=$zone.$band;
    return $utmproj{$zb} if exists $utmproj{$zb};
    if( ! exists $utmproj{$zb} )
    {
        my $cm=$zone*6.0-183.0;
        my $fn= $band lt 'N' ? 10000000.0 : 0.0;
        $utmproj{$zb} = new LINZ::Geodetic::TMProjection(GRS80,$cm,0.0,0.9996,500000.0,$fn,1.0);
    }
    return $utmproj{$zb};
    }

sub write {
    my($lat,$lon,$ndg)=@_;
    $ndg=5 if $ndg eq '';
    my($zone,$band)=gzd($lon,$lat);
    my $proj=utmproj($zone,$band);
    my ($n,$e)=@{$proj->proj([$lat,$lon])};
    my $ems0=(($zone-1) % 3)*8-1;
    $e /= $ms;
    my $ems=floor($e);
    $e -= $ems;
    $ems=substr($col_ms,($ems+$ems0) % $ncol_ms, 1 );
    $n /= $ms;
    my $nms=floor($n);
    $n -= $nms;
    $nms += 5 if $zone % 2 == 0;
    $nms=substr($row_ms,int($nms) % $nrow_ms,1);
    my $dig='';
    if( $ndg > 0 )
    {
        $ndg=int($ndg);
        $ndg=5 if $ndg > 5;
        my $factor=10**$ndg;
        $dig=sprintf(" %0*d %0*d",$ndg,$e*$factor,$ndg,$n*$factor);
    }
    return $zone.$band.' '.$ems.$nms.$dig;
    }

sub read {
    my($gridref)=@_;
    $gridref=uc($gridref);
    my $valid = $gridref =~ /^\s*
            ([0-6]?\d)([$lat_bands])\s*
            ([$col_ms])([$row_ms])
            (?:\s*(\d{2,10}|\d{1,5}\s+\d{1,5}))?
            \s*$/xi;
    my ($zone,$band,$ems,$nms,$digits)=($1,$2,$3,$4,$5);
    my ($de,$dn)=split(' ',$digits);
    if( $dn eq '' )
    {
        $valid=0 if length($de) % 2 == 1;
        $dn=substr($de,length($de)/2);
        $de=substr($de,0,length($de)/2);
    }
    elsif( length($de) != length($dn) )
    {
        $valid=0;
    }
    $valid=0 if $zone < 1 || $zone > 60;
    die "Invalid MGRS grid reference $gridref\n" if ! $valid;
    my $ndg=length($de);
    my $factor=10**(5-$ndg);
    $de = ($de+0.5)*$factor;
    $dn = ($dn+0.5)*$factor;
    my $proj=utmproj($zone,$band);
    my $cm=$zone*6.0-183.0;
    my $zlat=-76.0+index($lat_bands,uc($band))*8.0;
    # Centre of UTM zone/band.
    my $e0 = 500000.0;
    my $n0=$proj->proj([$zlat,$cm])->[0];
    my $ems0=(($zone-1) % 3)*8-1;
    my $e=(index($col_ms,uc($ems))-$ems0)*100000.0+$de;
    my $n=(index($row_ms,uc($nms))-($zone % 2 == 0 ? 5 : 0))*100000.0+$dn;
    $n += floor(($n0-$n)/$msband+0.5)*$msband;
    my $llh=$proj->geog([$n,$e]);
    return @$llh;
}

1;
