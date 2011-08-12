#===============================================================================
# Module:             Coordinate.pm
#
# Description:       Defines packages: 
#                      Geodetic::Coordinate
#
# Dependencies:      Uses the following modules:   
#
#  $Id: Coordinate.pm,v 1.1 1999/09/09 21:09:36 ccrook Exp $
#
#  $Log: Coordinate.pm,v $
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::Coordinate
#
#   Description: A base class for coordinates.  Provides service functions
#                and coordinate conversion functions.  The coordinate is 
#                defined as an array reference with three ordinate elements
#                followed optionally by a coordinate system reference.
# 
#                Defines the following routines:
#                  $crd = new Geodetic::Coordinate($crd)
#                  $crd = new Geodetic::Coordinate($ord1, $ord2, $ord3, $cs)
#
#                  $crd->setcs($cs)
#                  $cs = $crd->coordsys
#
#                  $crd2 = $crd->as($cstarget)
#
#===============================================================================

package Geodetic::Coordinate;


#===============================================================================
#
#   Method:       new
#
#   Description:  $crd = new Geodetic::Coordinate($crd)
#
#   Parameters:   $crd    Either an existing ordinate or array reference,
#                         or a list of ordinates
#
#   Returns:      
#
#===============================================================================

sub new {
   my ($class, $crd ) = @_;
   $crd = [@_[1..4]] if ! ref($crd);
   return bless $crd, $class;
   }


#===============================================================================
#
#   Subroutine:   setcs
#
#   Description:   $crd->setcs($cs)
#                  Associates a coordinate system with a coordinate
#
#   Parameters:    $cs       The coordinate system
#
#   Returns:       The coordinate with the coordinate system defined
#
#===============================================================================

sub setcs {
   $_[0]->[3] = $_[1];
   return $_[0];
   }


#===============================================================================
#
#   Subroutine:   coordsys
#
#   Description:   $cs = $crd->coordsys
#                  Returns the coordinate system of a coordinate
#
#   Parameters:    None
#
#   Returns:       $cs    The coordinate system object
#
#===============================================================================

sub coordsys {
   return $_[0]->[3];
   }


#===============================================================================
#
#   Method:       as
#
#   Description:  $crd2 = $crd->as($cstarget)
#
#   Parameters:   $cstarget    The target coordinate system
#
#   Returns:      $crd2        The coordinate converted to the target
#                              coordinate system
#
#===============================================================================

sub as {
   my ($self,$target) = @_;
   return $self->coordsys->conversionto($target)->convert($self);
   }

1;
