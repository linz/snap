#===============================================================================
# Module:             HgtRef.pm
#
# Description:       Defines a basic hgtref object.  This is a blessed
#                    hash reference with elements:
#                       name       The hgtref name
#                       ellipsoid  The ellipsoid object defined for the frame
#                       baseref    The base hgtref
#                       transfunc  A function object for converting coordinates
#                                  to and from the base hgtref.
#
#                    The CoordSys module may install additional entries into
#                    this hash, which are
#                       _csgeod    The geodetic (lat/lon) coordinate system
#                                  for the hgtref
#                       _cscart    The cartesian coordinate system for the
#                                  hgtref
#                       
#                    Defines packages: 
#                      Geodetic::HgtRef
#
# Dependencies:      Uses the following modules:   
#
#  $Id: HgtRef.pm,v 1.1 2005/11/27 19:39:30 gdb Exp $
#
#  $Log: HgtRef.pm,v $
#  Revision 1.1  2005/11/27 19:39:30  gdb
#  *** empty log message ***
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::HgtRef
#
#   Description: Defines the following routines:
#                 $hgtref = new Geodetic::HgtRef($name, $refcrdsys, $transfunc, $code )
#
#                  $name = $hgtref->name
#
#===============================================================================

package Geodetic::HgtRef;

my $id;

#===============================================================================
#
#   Method:       new
#
#   Description:  $hgtref = new Geodetic::HgtRef($name, $refcrdsys, $transfunc, $code )
#
#   Parameters:   $name       The name of the hgtref
#                 $refcrdsys  Reference ellipsoidal coordinate system for
#                             conversions
#                 $transfunc  An object providing functions OrthToEll,
#                             EllToAuth, and GeoidHeight, which convert 
#                             ellipsoidal heights to and from the 
#                             "orthometric" heights and compute the geoid height
#                 $code       Optional code identifying the hgtref
#
#   Returns:      $hgtref   The blessed ref frame hash
#
#===============================================================================

sub new {
  my ($class, $name, $refcrdsys, $transfunc, $code ) = @_;
  my $self = { name=>$name, 
               refcrdsys=>$refcrdsys,
               transfunc=>$transfunc,
               code=>$code,
               id=>$id };
  $id++;
  return bless $self, $class;
  }


sub name { return $_[0]->{name} }
sub code { return $_[0]->{code} }
sub refcrdsys { return $_[0]->{refcrdsys} }
sub transfunc { return $_[0]->{transfunc} }
sub id { return $_[0]->{id} }

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
   my $rcrd = $crd->as( $self->refcrdsys );
   my $ohgt = $self->transfunc->OrthometricHeight( $rcrd, $rcrd->hgt );
   return $ohgt;
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
   my $rcrd = $crd->as( $self->refcrdsys );
   my $ehgt = $self->transfunc->EllipsoidalHeight( $rcrd, $ohgt );
   $rcrd->[2] = $ehgt;
   return $rcrd->as( $crd->coordsys );
   }

1;
