#===============================================================================
# Module:             HgtCrdSys.pm
#
# Description:       Defines packages: 
#                      Geodetic::HgtCrdSys
#
# Dependencies:      Uses the following modules: 
#
#  $Id: HgtCrdSys.pm,v 1.1 2005/11/27 19:39:30 gdb Exp $
#
#  $Log: HgtCrdSys.pm,v $
#  Revision 1.1  2005/11/27 19:39:30  gdb
#  *** empty log message ***
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::HgtCrdSys
#
#   Description: Defines the following routines:
#                Constructor
#                  $hcs = new Geodetic::HgtCrdSys($name, $hgtref, $offset)
#
#                Access functions for components of the coordinate system
#                  $name = $hcs->name
#                  $hgtref = $hcs->hgtref
#                  $offset = $hcs->offset
#
#===============================================================================

package Geodetic::HgtCrdSys;

my $hcsid = 0;


#===============================================================================
#
#   Method:       new
#
#   Description:  $hcs = new Geodetic::HgtCrdSys($name, $hgtref, $offset, $code)
#
#   Parameters:   $name       The name of the coordinate system
#                 $hgtref     The height reference
#                 $offset     The offset of the height coordinate system relative 
#                             to the reference
#                 $code       Optional code for the system
#
#   Returns:      $hcs         The coordinate system object
#
#===============================================================================

sub new {
  my ($class, $name, $hgtref, $offset, $code ) = @_;
  $class = ref($class) if ref($class);
 
  my $self;

  # Projection isn't required if not a projection coordinate system

  # Now need to create the projection
  $self = {name=>$name, hgtref=>$hgtref, code=>$code,
                  offset=>$offset, csid=>$hcsid};
  $hcsid++;

  bless $self, $class;

  return $self;
  }

#===============================================================================
#
#   Subroutine:   name
#                 code
#                 hgtref
#                 offset
#===============================================================================

sub name { return $_[0]->{name} }
sub code { return $_[0]->{code} }
sub hgtref { return $_[0]->{hgtref}}
sub offset { return $_[0]->{offset} }

#===============================================================================
#
#   Subroutine:   get_orthometric_height
#
#   Description:   $ohgt = $hcs->get_orthometric_height( $crd )
#
#                  Returns the orthometric height for a given point, given its 
#                  ellipsoidal coordinates
#
#   Parameters:    None
#
#   Returns:       The orthometric height
#
#===============================================================================

sub get_orthometric_height {
   my( $self, $crd ) = @_;
   return $self->hgtref->get_orthometric_height( $crd ) + $self->offset;
   }

#===============================================================================
#
#   Subroutine:   set_ellipsoidal_height
#
#   Description:   $crd = $hcs->set_ellipsoidal_height( $crd, $ohgt );
#                  Sets the height of a coordinate given its orthometric
#                  height in the height coordinate system
#
#   Parameters:    None
#
#   Returns:       The geocentric coordinate system
#
#===============================================================================

sub set_ellipsoidal_height {
   my( $self, $crd, $ohgt ) = @_;
   $ohgt -= $self->offset;
   return $self->hgtref->set_ellipsoidal_height( $crd, $ohgt );
   }

#===============================================================================
#
#   Subroutine:   convert_orthometric_height
#
#   Description:   $ohgt = $hcs->convert_orthometric_height( $ohgt, $crd, $hcs );
#                  Converts an orthometric height in this height system to a 
#                  different height system
#
#   Parameters:    None
#
#   Returns:       The geocentric coordinate system
#
#===============================================================================

sub convert_orthometric_height_to {
   my( $self, $hcsnew, $crd, $ohgt ) = @_;
   $ohgt -= $self->offset;
   if( $self->hgtref->id != $hcsnew->hgtref->id ) {
      my $rcrd = $self->hgtref->set_ellipsoidal_height( $crd, $ohgt );
      $ohgt = $hcsnew->hgtref->get_orthometric_height( $rcrd );
      }
   $ohgt += $hcsnew->offset;
   return $ohgt;
   }
   
1;
