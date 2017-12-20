#===============================================================================
# Module:             LINZ::Geodetic
#
# Description:       Defines packages: 
#                      Geodetic
#                    This is of very little value - it mainly defines a
#                    few variables.
#
# Dependencies:
#
#  $Id: Geodetic.pm,v 1.1 1999/09/09 21:22:15 ccrook Exp $
#
#  $Log: Geodetic.pm,v $
#  Revision 1.1  1999/09/09 21:22:15  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Package:     LINZ::Geodetic
#
#   Description: Defines the following routines:
#
#===============================================================================

package LINZ::Geodetic;
require Exporter;
our @ISA=qw(Exporter);
our @EXPORT_OK=qw(CARTESIAN GEODETIC PROJECTION);

our $VERSION=2.0;

use constant
{
    CARTESIAN=>0,
    GEODETIC=>1,
    PROJECTION=>2,
};


1;

=head1 NAME

LINZ::Geodetic - modules for geodetic calculations

=head1 SYNOPSIS

    use LINZ::Geodetic::CoordSysList

    # If $coordsysdeffile is not used, then will use $ENV{COORDSYSDEF}
    my $cslist = LINZ::Geodetic::CoordSysList->newFromCoordSysDef( $coordsysdeffile );
    my $cs = $cslist->coordsys('NZGD2000');

    # or..
    use LINZ::Geodetic::CoordSysList qw/GetCoordSys/;
    my $cs=GetCoordSys('NZGD2000');

    print $cslist->coordsysname('NZMG');
    my $crd = $cs->coord( $lat, $lon, $hgt );
    my $crdtm = $cstm->coord( $northing, $easting, $hgt );
    my $newcs = $cslist->coordsys('NZTM');
    my $newcrd = $crd->as($newcs);
    printf "Coord is (%f,%f,%f)\n",$newcrd->northing, $newcrd->easting, $newcrd->hgt;
    my ($sf,$conv) = @{$newcrd->sf_conv()};

    my @coordsy_codes = $cslist->coordsys();
    my @datum_codes = $cslist->datum();
    my @ellipsoid_codes = $cslist->ellipsoid();

    my $nzgd2000dtm = $clist->datum('NZGD2000');
    my $grs80elp = $cslist->ellipsoid('GRS80');

    # Ellipsoid functions

    my $elp = new LINZ::Geodetic::Ellipsoid($a,$rf,$name,$code);
    print $elp->a,"\n";
    print $elp->b,"\n";
    print $elp->rf,"\n";
    print $elp->name,"\n";
    print $elp->code,"\n";
    my $xyz = $elp->cartesian([$lat,$lon,$hgt]);
    my $xyz = $elp->xyz([$lat,$lon,$hgt]);
    my $llh = $elp->geodetic([$x,$y,$z]);
    my $llh = $elp->geog([$x,$y,$z]);
    my ($dndlt,$dedln) = $elp->metres_per_degree($lat,$lon);

    # GRS80 is specifically supported ...
    use LINZ::Geodetic::Ellipsoid qw/GRS80/;
    GRS80->geodetic([$x,$y,$z]);

    # Datum functions

    my $dtm = new LINZ::Geodetic::Datum($name,$ellipsoid,$baseref,$transfunc,$code);
    print $dtm->name,"\n";
    print $dtm->code,"\n";
    print $dtm->ellipsoid,"\n";
    print $dtm->baseref,"\n";
    print $dtm->transfunc,"\n";

    # Projection functions
    
    use LINZ::Geodetic::Projection;
    my $prj = new LINZ::Geodetic::Projection($def,$ellipsoid);
    # $def is a string definition of a projection - details below
    print $prj->type,"\n"; # eg Transverse Mercator
    my $llh = $prj->geog([$e, $n, $h]);
    my $llh = $prj->geodetic([$e, $n, $h]);
    my $enh = $prj->proj([$lt, $ln, $h]);
    my $enh = $prj->projection([$lt, $ln, $h]);
    my ($scalefactor,$convergence) = $prj->sf_conv;

    use LINZ::Geodetic::TMProjection;
    my $tm = new LINZ::Geodetic::TMProjection($ellipse,$cm,$lto,$sf,$fe,$fn,$utom);
    # $utom is optional units to metres conversion
    use LINZ::Geodetic::NZMGProjection;
    my $nzmg = new LINZ::Geodetic::NZMGProjection;
    use LINZ::Geodetic::LCCProjection;
    my $lcc = new LINZ::Geodetic::LCCProjection($ellipse,$sp1,$sp2,$lt0,$ln0,$e0,$n0);

    # Coordinate system functions

    my $cs = new LINZ::Geodetic::CoordSys($type,$name,$datum,$projection,$code);
    # Type is one of LINZ::Geodetic::GEODETIC, LINZ::Geodetic::CARTESIAN, LINZ::Geodetic::PROJECTION
    print $cs->name,"\n";
    print $cs->code,"\n";
    
    my $dtm = $cs->datum;
    my $elp = $cs->ellipsoid;
    my $prj = $cs->projection;
    my $type = $cs->type;

    my $geodcs = $cs->asgeog;
    my $geodcs = $cs->asgeodetic;
    my $cartcs = $cs->asxyz;
    my $cartcs = $cs->ascartesian;

    my $coord = $cs->coord($ord1,$ord2,$ord3);
    my $conv = $cs->conversionto($newcs);
    # For conversions requiring an epoch (eg ITRF to NZGD2000)
    # Epoch is defined in decimal years
    my $conv = $cs->conversionto($newcs,$epoch);
    my $cs = $conv->from;
    my $newcs = $conv->to;
    my $crd = $conv->convert([$ord1,$ord2,$ord3]); # I think!

    # Methods for all coordinates

    $coord->setcs($cs); # Define the coordinate system of existing coords
    my $crdcs = $coord->coordsys(); # Retrieve coordinate system
    my $crdtm = $coord->as($newcs); # Transform to different coord system
    my $crdtm = $coord->as($newcs,$epoch); # Transformation at specified epoch
    $crd->setepoch($epoch);   # Set the coordinate epoch in decimal years
    $crd->epoch;

    # For lat/lon coords

    use LINZ::Geodetic::GeodeticCrd;
    my $coord = new GeodeticCrd($lat,$lon,$hgt);
    my $string = $coord->asstring($ndpsec,$ndphgt); # Decimal places of seconds/hgts
    
    print join(" ",@$string),"\n";
    my($lat,$lon,$hgt) = ($coord->lat, $coord->lon, $coord->hgt );

    # For projection coords
    
    use LINZ::Geodetic::ProjectionCrd;
    my $coord = new ProjectionCrd($north,$east,$hgt);
    my $string = $coord->asstring($ndp,$sep,$ndpv);
    # ndp = no decimal places, $sep = thousands separator, $ndpv = hgt ndp
    my($easting,$northing,$hgt) = ($coord->easting, $coord->northing, $coord->hgt );

    # For cartesian coords

    use LINZ::Geodetic::CartesianCrd;
    my $coord = new CartesianCrd($x, $y, $z);
    my $string = $coord->asstring($ndp); # ndp = no decimal places
    my($x,$y,$z) = ($coord->X, $coord->Y, $coord->Z );

    # Vertical datums define transformation from ellipsoidal heights
    # to orthometric heights in a reference coordinate system.  
    # Note: the use of "orthometric" here is not technically correct!  
    # These are ellipsoidal heights calculated relative to a 
    # gridded reference surface.
    
    my $vd=$cslist->vdatum($vdatumcode);
    my $vdname=$cslist->vdatumname($vdatumcode);
    my $ohgt=$vd->get_orthometric_height($crd);
    my $crd2=$vd->set_ellipsoidal_height($crd,$ohgt);
    $vd->convert_orthometric_height($vdnew,$crd,$ohgt);


