#===============================================================================
# Module:             Coordinate.pm
#
# Description:       Defines packages: 
#                      LINZ::Geodetic::Coordinate
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
#   Class:       LINZ::Geodetic::Coordinate
#
#   Description: A base class for coordinates.  Provides service functions
#                and coordinate conversion functions.  The coordinate is 
#                defined as an array reference with three ordinate elements
#                followed optionally by a coordinate system reference and then
#                a epoch
#
#                Defines the following routines:
#                  $crd = new LINZ::Geodetic::Coordinate($crd)
#                  $crd = new LINZ::Geodetic::Coordinate($ord1, $ord2, $ord3, $cs, $epoch)
#
#                  $crd->setcs($cs)
#                  $cs = $crd->coordsys
#
#                  $crd2 = $crd->as($cstarget)
#
#===============================================================================

package LINZ::Geodetic::Coordinate;


#===============================================================================
#
#   Method:       new
#
#   Description:  $crd = new LINZ::Geodetic::Coordinate($crd)
#
#   Parameters:   $crd    Either an existing ordinate or array reference,
#                         or a list of ordinates
#
#   Returns:      
#
#===============================================================================

sub new {
   my ($class, $crd ) = @_;
   $crd = [@_[1..5]] if ! ref($crd);
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
#   Subroutine:   epoch
#
#   Description:   $cs = $crd->epoch
#                  Returns the epoch for the coordinate
#
#   Parameters:    None
#
#   Returns:       $cs    The epoch of the coordinate in decimal years
#
#===============================================================================

sub epoch {
   return $_[0]->[4];
   }

#===============================================================================
#
#   Subroutine:   setepoch
#
#   Description:   $crd->setepoch($epoch)
#                  Set the epoch for the coordinate
#
#   Parameters:    $epoch       The coordinaste epoch
#
#   Returns:       The coordinate
#
#===============================================================================

sub setepoch {
   $_[0]->[4] = $_[1];
   return $_[0];
   }

#===============================================================================
#
#   Method:       as
#
#   Description:  $crd2 = $crd->as($cstarget, $target_epoch, $conversion_epoch)
#
#   Parameters:   $cstarget         The target coordinate system
#                 $target_epoch     Optional target epoch for the output
#                                   coordinate system's coordinates
#                 $conversion_epoch Optional ref epoch when transformations
#                                   between reference frames are computed
#
#   Returns:      $crd2          The coordinate converted to the target
#                                coordinate system
#
#===============================================================================

sub as {
   my ($self,$target,$target_epoch,$conversion_epoch) = @_;
   return $self->coordsys
      ->conversionto($target, $conversion_epoch)->convert($self, $target_epoch);
   }

1;
