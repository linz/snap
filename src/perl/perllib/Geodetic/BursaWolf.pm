#===============================================================================
# Module:             BursaWolf.pm
#
# Description:       Defines packages: 
#                      Geodetic::BursaWolf
#                    This implements the Bursa Wolf 7 parameter transformation 
#                    to convert between different datums.
#
# Dependencies:      Uses the following modules: 
#                      Geodetic::CartesianCrd  
#
#  $Id: BursaWolf.pm,v 1.1 1999/09/09 21:09:36 ccrook Exp $
#
#  $Log: BursaWolf.pm,v $
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::BursaWolf
#
#   Description: Defines the following methods:
#                  $bw = new Geodetic::BursaWolf($tx,$ty,$tz,$rx,$ry,$rz,$ppm)
#                  $crd2 = $bw->ApplyTo($crd)
#                  $crd2 = $bw->ApplyInverseTo($crd)
#
#===============================================================================

package Geodetic::BursaWolf;

require Geodetic::CartesianCrd;

my $sec2rad = atan2(1,1)/(45.0*3600.0);


#===============================================================================
#
#   Method:       new
#
#   Description:  $bw = new Geodetic::BursaWolf($tx, $ty, $tz, $rx, $ry, $rz, $ppm)
#
#   Parameters:   $tx,$ty,$tz  The X, Y, and Z translation components in metres
#                 $rx,$ry,$rz  The X, Y, and Z rotations in arc seconds
#                 $ppm         The scale factor, part per million.
#
#   Returns:      
#
#===============================================================================

sub new {
   my( $class, $tx, $ty, $tz, $rx, $ry, $rz, $ppm ) = @_;
   my $R;
   # If the rotation components are not all zero then calculate
   # a rotation matrix.  
   if( $rx || $ry || $rz ) {
     my $cx = $rx*$sec2rad; 
     my $sx = sin($cx);
     $cx = cos($cx);
     my $cy = $ry*$sec2rad; 
     my $sy = sin($cy);
     $cy = cos($cy);
     my $cz = $rz*$sec2rad; 
     my $sz = sin($cz);
     $cz = cos($cz);
     $R = [ [ $cz*$cy,   $cz*$sx*$sy+$sz*$cx,   $sx*$sz-$cz*$cx*$sy ],
            [ -$sz*$cy,  $cz*$cx-$sz*$sx*$sy,   $sz*$cx*$sy+$cz*$sx],
            [ $sy,      -$sx*$cy,               $cx*$cy             ]];
     }
  # Construct the object.
  my $self = { Txyz=>[$tx, $ty, $tz],
               Rxyz=>[$rx, $ry, $rz],
               SF=>$ppm,
               R=>$R,
               sf=>(1.0 + $ppm*1.0e-6 )
             };
  return bless $self, $class;
  }



#===============================================================================
#
#   Method:       ApplyTo
#
#   Description:  $crd2 = $bw->ApplyTo($crd)
#                 Applies the Bursa-Wolf transformation to geocentric
#                 (XYZ) coordinates.
#
#   Parameters:   $crd     The coordinates to transform
#
#   Returns:               The converted coordinates as a CartesianCrd
#                          object.
#
#===============================================================================

sub ApplyTo {
  my ($self, $crd ) = @_;
  my $x = $crd->[0];
  my $y = $crd->[1];
  my $z = $crd->[2];
  my $sf = $self->{sf};
  my $R = $self->{R};
  if( $R ) {
    ($x, $y, $z ) = ( $R->[0]->[0]*$x + $R->[0]->[1]*$y + $R->[0]->[2]*$z,
                      $R->[1]->[0]*$x + $R->[1]->[1]*$y + $R->[1]->[2]*$z,
                      $R->[2]->[0]*$x + $R->[2]->[1]*$y + $R->[2]->[2]*$z );
    }
  my $Txyz = $self->{Txyz};

  return new Geodetic::CartesianCrd ( $Txyz->[0] + $sf * $x,
                                       $Txyz->[1] + $sf * $y,
                                       $Txyz->[2] + $sf * $z );
 
  }
   


#===============================================================================
#
#   Method:       ApplyInverseTo
#
#   Description:  $bw->ApplyInverseTo($crd)
#                 Applies the inverse of the Bursa-Wolf transformation
#                 to a geocentric (XYZ) coordinate.
#
#   Parameters:   $crd    The coordinate to convert.       
#
#   Returns:              The converted coordinate.
#
#===============================================================================

sub ApplyInverseTo {
  my ($self, $crd ) = @_;
  my $x = $crd->[0];
  my $y = $crd->[1];
  my $z = $crd->[2];
  my $sf = $self->{sf};
  my $Txyz = $self->{Txyz};

  ($x, $y, $z) = ( ($x - $Txyz->[0])/$sf,
                   ($y - $Txyz->[1])/$sf,
                   ($z - $Txyz->[2])/$sf );
  my $R = $self->{R};
  if( $R ) {
    ($x, $y, $z ) = ( $R->[0]->[0]*$x + $R->[1]->[0]*$y + $R->[2]->[0]*$z,
                      $R->[0]->[1]*$x + $R->[1]->[1]*$y + $R->[2]->[1]*$z,
                      $R->[0]->[2]*$x + $R->[1]->[2]*$y + $R->[2]->[2]*$z );
    }

  return new Geodetic::CartesianCrd ( $x, $y, $z );
  }

1;
