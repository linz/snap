#===============================================================================
# Module:            CoordConversion.pm
#
# Description:       Defines packages: 
#                      Geodetic::CoordConversion
#                    This is basically a service class for the CoordSys class
#                    an encapsulates conversions between different coordinate
#                    systems.
#
# Dependencies:      Uses the following modules:   
#
#  $Id: CoordConversion.pm,v 1.1 1999/09/09 21:09:36 ccrook Exp $
#
#  $Log: CoordConversion.pm,v $
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::CoordConversion
#
#   Description: Encapsulates a coordinate conversion function in an array
#                reference with three elements - the source coordinate 
#                system, the target coordinate system, and a reference to
#                the conversion function.
#
#                Defines the following routines:
#                  $conv = new Geodetic::CoordConversion($from, $to, $funcref)
#                  $crd2 = $conv->convert($crd);
#                  $csfrom = $conv->from
#                  $csto = $conv->to
#
#===============================================================================

package Geodetic::CoordConversion;


#===============================================================================
#
#   Method:       new
#
#   Description:  $conv = new Geodetic::CoordConversion($from, $to, $funcref)
#
#   Parameters:   $from       The source coordinate system
#                 $to         The target coordinate system
#                 $funcref    Reference to the function to do the conversion
#
#   Returns:      $conv       The blessed conversion object
#
#===============================================================================

sub new {
  my( $class, $from, $to, $funcref ) = @_;
  my $self = [$from, $to, $funcref];
  return bless $self, $class;
  }


#===============================================================================
#
#   Subroutine:   convert
#
#   Description:   $crd2 = $conv->convert($crd)
#                  Applies the coordinate conversion to a coordinate and
#                  returns the resulting coordinate.
#
#   Parameters:    $crd      The coordinate to convert
#
#   Returns:                 The converted coordinate
#
#===============================================================================

sub convert {
  my $self=shift;
  return $self->[2]->( @_ )->setcs($self->[1]);
  }


#===============================================================================
#
#   Subroutine:   from
#
#   Description:   $csfrom = $conv->from
#                  Returns the source coordinate system for the conversion
#
#   Parameters:    None
#
#   Returns:       The coordinate system reference.
#
#===============================================================================

sub from {
  return $_[0]->[0];
  }


#===============================================================================
#
#   Subroutine:   to
#
#   Description:   $csto = $conv->to
#                  Returns the target coordinate system for the conversion
#
#   Parameters:    None
#
#   Returns:       The coordinate system reference
#
#===============================================================================

sub to {
  return $_[0]->[1];
  }

1;

