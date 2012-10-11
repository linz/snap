#===============================================================================
# Module:             Geodetic.pm
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
#   Package:     Geodetic
#
#   Description: Defines the following routines:
#
#===============================================================================

package Geodetic;

# Coordinate types


#===============================================================================
#
#   Subroutine:   CARTESIAN
#
#   Description:  
#
#   Parameters:   
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub CARTESIAN { return 0; }

#===============================================================================
#
#   Subroutine:   GEODETIC
#
#   Description:  
#
#   Parameters:   
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub GEODETIC { return 1; }

#===============================================================================
#
#   Subroutine:   PROJECTION
#
#   Description:  
#
#   Parameters:   
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub PROJECTION { return 2; }

1;

=head1 NAME

Geodetic - modules for geodetic calculations

=head1 SYNOPSIS

    use Geodetic::CoordSysList


    my $cslist = Geodetic::CoordSysList->newFromCoordSysDef( $coordsysdeffile );
    my $cs = $cslist->coordsys('NZGD2000');
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

    my $elp = new Geodetic::Ellipsoid($a,$rf,$name,$code);
    print $elp->a,"\n";
    print $elp->b,"\n";
    print $elp->rf,"\n";
    print $elp->name,"\n";
    print $elp->code,"\n";
    my $xyz = $elp->cartesian([$lat,$lon,$hgt]);
    my $xyz = $elp->geog([$lat,$lon,$hgt]);
    my $llh = $elp->geodetic([$x,$y,$z]);
    my $llh = $elp->geog([$x,$y,$z]);

    # Datum functions

    my $dtm = new Geodetic::Datum($name,$ellipsoid,$baseref,$transfunc,$code);
    print $dtm->name,"\n";
    print $dtm->code,"\n";
    print $dtm->ellipsoid,"\n";
    print $dtm->baseref,"\n";
    print $dtm->transfunc,"\n";

    # Projection functions
    
    use Geodetic::Projection;
    my $prj = new Geodetic::Projection($def,$ellipsoid);
    # $def is a string definition of a projection - details below
    print $prj->type,"\n"; # eg Transverse Mercator
    my $llh = $prj->geog([$e, $n, $h]);
    my $llh = $prj->geodetic([$e, $n, $h]);
    my $enh = $prj->proj([$lt, $ln, $h]);
    my $enh = $prj->projection([$lt, $ln, $h]);
    my ($scalefactor,$convergence) = $prj->sf_conv;

    use Geodetic::TMProjection;
    my $tm = new Geodetic::TMProjection($ellipse,$cm,$lto,$sf,$fe,$fn,$utom);
    # $utom is optional units to metres conversion
    use Geodetic::NZMGProjection;
    my $nzmg = new Geodetic::NZMGProjection;
    use Geodetic::LCCProjection;
    my $lcc = new Geodetic::LCCProjection($ellipse,$sp1,$sp2,$lt0,$ln0,$e0,$n0);

    # Coordinate system functions

    my $cs = new Geodetic::CoordSys($type,$name,$datum,$projection,$code);
    # Type is one of Geodetic::GEODETIC, Geodetic::CARTESIAN, Geodetic::PROJECTION
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
    my $cs = $conv->from;
    my $newcs = $conv->to;
    my $crd = $conv->convert([$ord1,$ord2,$ord3]); # I think!

    # Methods for all coordinates

    $coord->setcs($cs); # Define the coordinate system of existing coords
    my $crdcs = $coord->coordsys(); # Retrieve coordinate system
    my $crdtm = $coord->as($newcs); # Transform to different coord system


    # For lat/lon coords

    use Geodetic::GeodeticCrd;
    my $coord = new GeodeticCrd($lat,$lon,$hgt);
    my $string = $coord->asstring($ndpsec,$ndphgt); # Decimal places of seconds/hgts
    
    print join(" ",@$string),"\n";
    my($lat,$lon,$hgt) = ($coord->lat, $coord->lon, $coord->hgt );

    # For projection coords
    
    use Geodetic::ProjectionCrd;
    my $coord = new ProjectionCrd($north,$east,$hgt);
    my $string = $coord->asstring($ndp,$sep,$ndpv);
    # ndp = no decimal places, $sep = thousands separator, $ndpv = hgt ndp
    my($easting,$northing,$hgt) = ($coord->easting, $coord->northing, $coord->hgt );

    # For cartesian coords

    use Geodetic::CartesianCrd;
    my $coord = new CartesianCrd($x, $y, $z);
    my $string = $coord->asstring($ndp); # ndp = no decimal places
    my($x,$y,$z) = ($coord->X, $coord->Y, $coord->Z );


    # Also undocumented stuff about heights!

