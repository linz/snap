package LINZ::Geodetic::Isometric;

our $pi=atan2(1,1)*4;
our $tol=4.848132e-11;

sub geodetic_from_isometric
{
    my($q,$e)=@_;

    my $it = 0;
    my $zz=exp($q);
    my $lat=2.0*(atan2($zz,1.0)-$pi/4.0);
    my $diff=0;
    do
    {
        my $slt2 = $e*sin($lat);
        my $ww=((1.0-$slt2)/(1.0+$slt2))**($e/2.0);
        $slt2 *= $slt2;
        $slt2 = 1.0-$slt2;
        my $top=$pi/4.0+$lat/2.0;
        $top=log(sin($top)*$ww/cos($top))-$q;
        my $bot=(1.0-$e*$e)/($slt2*cos($lat));
        my $phi1=$lat-$top/$bot;
        $diff=abs($phi1-$lat);
        $lat = $phi1;
    }
    while ($diff>=$tol && $it++ < 10);
    return $lat;
}

sub isometric_from_geodetic
{
    our($lat,$e)=@_;

    my $xx=$pi/4.0+$lat/2.0;
    $xx=sin($xx)/cos($xx);
    my $slt = sin($lat);
    $yy=((1.0-$e*$slt)/(1.0+$e*$slt))**($e/2.0);
    return log($xx*$yy);
}

1;
