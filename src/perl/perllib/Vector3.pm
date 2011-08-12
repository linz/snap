#===============================================================================
# Module:            Vector3.pm
#
# Version:           $Id: Vector3.pm,v 1.2 2000/03/29 21:52:41 cms Exp $  
#
# Description:       Defines packages: 
#                      Vector3
#
# Dependencies:      Uses the following modules:   
#
# History
# $Log: Vector3.pm,v $
# Revision 1.2  2000/03/29 21:52:41  cms
# Fixed errors resulting from extracting modules from main con_crs script.
#
# Revision 1.1  2000/03/29 03:13:13  cms
# Building lib module
#
#===============================================================================


#===============================================================================
#
#   Class:       Vector3
#
#   Description: Provides some simple routines for managing a 3d vector
#                Defines the following routines:
#                  $vector = new Vector3($x, $y, $z);
#                  $vecdiff = $vector->VectorTo($trgt);
#                  ($r,$az,$zd) = $vector->AsPolar;
#                  $r = $vector->Length;
#
#===============================================================================

package Vector3;

my $deg2rad = atan2(1,1)/45.0;

#===============================================================================
#
#   SUBROUTINE:   new
#
#   DESCRIPTION:  Bless an array reference to Vector3
#
#   PARAMETERS:   $x,$y,$z     The vector components
#
#   RETURNS:      
#
#   GLOBALS:
#
#===============================================================================

sub new {
  my ($class, $x, $y, $z ) = @_;
  my $self = [$x, $y, $z];
  return bless $self, $class;
  }


#===============================================================================
#
#   SUBROUTINE:   VectorTo
#
#   DESCRIPTION:  Calculates the vector difference between two vectors.
#                 Usage:  $vecdif = $vec1->VectorTo($vec2);
#
#   PARAMETERS:   $trgt    The target vector
#
#   RETURNS:      $vecdif  A vector 3 defining the difference
#
#   GLOBALS:
#
#===============================================================================

sub VectorTo {
  my ($self, $trgt ) = @_;
  return new Vector3($trgt->[0]-$self->[0],$trgt->[1]-$self->[1],$trgt->[2]-$self->[2]);
  }



#===============================================================================
#
#   SUBROUTINE:   AsPolar
#
#   DESCRIPTION:  Converts a vector to polar representation
#                 Usage: ($r,$az,$zd) = $vec->AsPolar
#
#   PARAMETERS:   None  
#
#   RETURNS:      $r    The vector length
#                 $az   The azimuth measured from the X axis to the 
#                       Y axis in degrees
#                 $va   The vertical angle measured from the equator
#                       to the z axis in degrees.
#
#   GLOBALS:
#
#===============================================================================

sub AsPolar {
  my $self = shift;
  my ($x, $y, $z) = @$self;
  my $p = ($x*$x+$y*$y);
  my $r = sqrt($p+$z*$z);
  my ($va, $az) = (0.0,0.0);
  $va = atan2( $z, sqrt($p) ) / $deg2rad if $r > 0.0;
  $az = atan2( $y, $x ) / $deg2rad if $p > 0.0;
  return ($r, $az, $va );
  }
     

#===============================================================================
#
#   SUBROUTINE:   Length
#
#   DESCRIPTION:  Returns the length of a vector
#                 Usage: $r = $vec->Length
#
#   PARAMETERS:   None  
#
#   RETURNS:      $r    The vector length
#
#   GLOBALS:
#
#===============================================================================

sub Length {
  my $self = shift;
  my ($x, $y, $z) = @$self;
  return sqrt($x*$x+$y*$y+$z*$z);
  }
     
1;
