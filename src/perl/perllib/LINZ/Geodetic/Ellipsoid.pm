#===============================================================================
# Module:             Ellipsoid.pm
#
# Description:       Defines packages: 
#                      LINZ::Geodetic::Ellipsoid
#
# Dependencies:      Uses the following modules: 
#                      LINZ::Geodetic::CartesianCrd
#                      LINZ::Geodetic::GeodeticCrd  
#
#  $Id: Ellipsoid.pm,v 1.1 1999/09/09 21:09:36 ccrook Exp $
#
#  $Log: Ellipsoid.pm,v $
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       LINZ::Geodetic::Ellipsoid
#
#   Description: The ellipsoid is defined as an array reference with elements
#                the semi-major axis, the reciprocal flattening, the semi-minor
#                axis, a*a, b*b, a*a-b*b, and the name.
#
#                Defines the following routines:
#                Constructor function
#                  $ellipsoid = new LINZ::Geodetic::Ellipsoid($a, $rf, $name)
#                  $name = $ellipsoid->name
#
#                Access functions
#                  $a = $ellipsoid->a
#                  $b = $ellipsoid->b
#                  $rf = $ellipsoid->rf
#
#                Coordinate conversion functions (XYZ <=> lat,lon,hgt)
#                  $xyz  = $ellipsoid->xyz($geog)
#                  $geog = $ellipsoid->geog($xyz)
#
#===============================================================================

package LINZ::Geodetic::Ellipsoid;
require Exporter;
our @ISA=qw(Exporter);
our @EXPORT_OK=qw(GRS80);

use constant
{
    GRS80_name=>"Geodetic Reference System 1980",
    GRS80_a=>6378137.0,
    GRS80_rf=>298.257222101
};

require LINZ::Geodetic::CartesianCrd;
require LINZ::Geodetic::GeodeticCrd;

my $rad2deg = 45/atan2(1,1);
my $convergence = 1.0e-10;


#===============================================================================
#
#   Method:       new
#
#   Description:  $object = new LINZ::Geodetic::Ellipsoid($a, $rf, $name)
#
#   Parameters:   $a          The semi major axis
#                 $rf         The reciprocal of the flattening
#                 $name       The name of the ellipsoid
#                 $code       Optional code identifying the ellpsoid
#
#   Returns:      The ellipsoid object
#
#===============================================================================

sub new {
   my($class,$a,$rf,$name,$code)=@_;
   my $b = $rf != 0.0 ? $a - $a/$rf : $a;
   my $a2 = $a*$a;
   my $b2 = $b*$b;
   my $a2b2 = $a2 - $b2;
   my $self = [$a,$rf,$b,$a2,$b2,$a2b2,$name,$code];
   return bless $self;
   }


#===============================================================================
#
#   Access functions
#
#===============================================================================

sub a { return $_[0]->[0]; }
sub rf { return $_[0]->[1]; }
sub b { return $_[0]->[2]; }
sub name { return $_[0]->[6]; }
sub code { return $_[0]->[7]; }


#===============================================================================
#
#   Method:       xyz
#
#   Description:  $xyz = $ellipsoid->geog($crd)
#
#   Parameters:   $geog       The coordinates to convert (an array reference
#                             containing lat/lon/height)
#
#   Returns:      $xyz        The returned X,Y,Z coordinates as an array
#                             reference (actually a LINZ::Geodetic::CartesianCrd)
#
#===============================================================================

sub cartesian { return xyz(@_); }
sub xyz {
   my($self,$crd) = @_;
   my $lt = $crd->[0];
   my $ln = $crd->[1];
   my $h = $crd->[2];
   my($a,$rf,$b,$a2,$b2,$a2b2) = @$self;
   my ($clt, $slt, $cln, $sln ) =  (
      cos($lt/$rad2deg),
      sin($lt/$rad2deg),
      cos($ln/$rad2deg),
      sin($ln/$rad2deg));
   my ($t1,$t2) = ($b*$slt, $a*$clt);
   my $bsac = sqrt($t1*$t1+$t2*$t2);
   my $p = $a2*$clt/$bsac + $h*$clt;
   return new LINZ::Geodetic::CartesianCrd( 
             $p*$cln, 
             $p*$sln, 
             $b2*$slt/$bsac + $h*$slt, 
             undef, 
             $crd->[4] );
   }


#===============================================================================
#
#   Method:       geog
#
#   Description:  $geog = $ellipsoid->geog($xyz)
#
#   Parameters:   $xyz        The input coordinates as an array reference.
#
#   Returns:      $geog       The lat/lon/height as an array reference, blessed
#                             as a LINZ::Geodetic::GeodeticCrd.
#
#===============================================================================

sub geodetic { return geog(@_); }
sub geog {
   my($self,$crd)=@_;
   my $x = $crd->[0];
   my $y = $crd->[1];
   my $z = $crd->[2];
   my($a,$rf,$b,$a2,$b2,$a2b2) = @$self;
   my $ln = atan2($y,$x);
   my $p = sqrt($x*$x+$y*$y);
   my $lt = atan2($a2*$z, $b2*$p);
   my $i = 10;
   my ($clt, $slt, $bsac);
   while( $i-- ) {
      my $lt0 = $lt;
      $slt = sin($lt);
      $clt = cos($lt);
      my ($t1,$t2) = ($b*$slt, $a*$clt );
      $bsac = sqrt( $t1*$t1 + $t2*$t2 );
      $lt = atan2( $z+$slt*$a2b2/$bsac, $p );
      last if $lt-$lt0 < $convergence && $lt0-$lt < $convergence;
      }
   my $h = $p*$clt + $z*$slt - $bsac;
   return new LINZ::Geodetic::GeodeticCrd(
             $lt*$rad2deg, 
             $ln*$rad2deg, 
             $h, 
             undef, 
             $crd->[4] );
   }

sub GRS80
{
    return new LINZ::Geodetic::Ellipsoid(GRS80_a,GRS80_rf,GRS80_name);
}

1;
