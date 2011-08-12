#===============================================================================
# Module:           RotMat3.pm
#
# Version:           $Id: RotMat3.pm,v 1.3 2000/07/04 00:28:14 cms Exp $  
#
# Description:       Manages a 3d rotation matrix. Defined as reference to an
#                    array of references to arrays.
#
# Defines            Packages: 
#                      RotMat3
#
# Dependencies:      Uses the following modules:   
#                      Vector3
#
# History
# $Log: RotMat3.pm,v $
# Revision 1.3  2000/07/04 00:28:14  cms
# Added missing degrees to radians conversion
#
# Revision 1.2  2000/03/29 21:52:41  cms
# Fixed errors resulting from extracting modules from main con_crs script.
#
# Revision 1.1  2000/03/29 03:13:13  cms
# Building lib module
#
#===============================================================================
   
#===============================================================================
#
#   Class:       RotMat3
#
#   Description: Defines the following routines:
#                  $object = new RotMat3($lon, $lat)
#                  $object->ApplyTo($vec)
#                  $object->ApplyInverseTo($vec)
#
#===============================================================================

package RotMat3;

use Vector3;

my $deg2rad = atan2(1,1)/45.0;

#===============================================================================
#
#   SUBROUTINE:   new
#
#   DESCRIPTION:  Creates a 3d rotation matrix representing the conversion
#                 from geocentric axes to topocentric axes at a given
#                 lat/lon
#                 Usage: $rtopo = new RotMat3($lon,$lat)
#
#   PARAMETERS:   $lon    The longitude in degrees
#                 $lat    The latitude in degrees
#
#   RETURNS:      $rtopo  The blessed vector reference
#
#   GLOBALS:
#
#===============================================================================

sub new {
  my( $class, $lon, $lat ) = @_;
  my $clt = cos($lat*$deg2rad);
  my $slt = sin($lat*$deg2rad);
  my $cln = cos($lon*$deg2rad);
  my $sln = sin($lon*$deg2rad);
  my $rtopo = [ [-$sln,       $cln,          0.0], 
                [-$slt*$cln, -$slt*$sln,    $clt], 
                [$clt*$cln,   $clt*$sln,    $slt] ];
  return bless $rtopo, $class;
  }


#===============================================================================
#
#   SUBROUTINE:   ApplyTo
#
#   DESCRIPTION:  Applies to rotation matrix to a vector
#                 Usage:  $rvec = $roto->ApplyTo($vec);
#
#   PARAMETERS:   $vec    The vector to apply the rotation to (a Vector3)
#
#   RETURNS:      $rvec   The rotated vector (a Vector3)
#
#   GLOBALS:
#
#===============================================================================

sub ApplyTo {
  my ($self, $vec) = @_;
  return new Vector3 (
            $self->[0]->[0]*$vec->[0] + $self->[0]->[1]*$vec->[1] + $self->[0]->[2]*$vec->[2],
            $self->[1]->[0]*$vec->[0] + $self->[1]->[1]*$vec->[1] + $self->[1]->[2]*$vec->[2],
            $self->[2]->[0]*$vec->[0] + $self->[2]->[1]*$vec->[1] + $self->[2]->[2]*$vec->[2]
            );
  }

#===============================================================================
#
#   SUBROUTINE:   ApplyInverseTo
#
#   DESCRIPTION:  Applies the inverse of the  rotation matrix to a vector
#                 Usage:  $rvec = $roto->ApplyInverseTo($vec);
#
#
#   PARAMETERS:   $vec    The vector to apply the rotation to (a Vector3)
#
#   RETURNS:      $rvec   The rotated vector (a Vector3)
#
#   GLOBALS:
#
#===============================================================================

sub ApplyInverseTo {
  my ($self, $vec) = @_;
  return new Vector3 (
            $self->[0]->[0]*$vec->[0] + $self->[1]->[0]*$vec->[1] + $self->[2]->[0]*$vec->[2],
            $self->[0]->[1]*$vec->[0] + $self->[1]->[1]*$vec->[1] + $self->[2]->[1]*$vec->[2],
            $self->[0]->[2]*$vec->[0] + $self->[1]->[2]*$vec->[1] + $self->[2]->[2]*$vec->[2]
            );
  }

1;
