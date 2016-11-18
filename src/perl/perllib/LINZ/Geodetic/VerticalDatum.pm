#===============================================================================
# Module:             VerticalDatum.pm
#
# Description:       Defines a basic vdatum object.  This is a blessed
#                    hash reference with elements:
#                       name       The vdatum name
#                       ellipsoid  The ellipsoid object defined for the frame
#                       baseref    The base vdatum
#                       transfunc  A function object for converting coordinates
#                                  to and from the base vdatum.
#
#                    The CoordSys module may install additional entries into
#                    this hash, which are
#                       _csgeod    The geodetic (lat/lon) coordinate system
#                                  for the vdatum
#                       _cscart    The cartesian coordinate system for the
#                                  vdatum
#                       
#                    Defines packages: 
#                      LINZ::Geodetic::VerticalDatum
#
# Dependencies:      Uses the following modules:   
#
#  $Id: VerticalDatum.pm,v 1.1 2005/11/27 19:39:30 gdb Exp $
#
#  $Log: VerticalDatum.pm,v $
#  Revision 1.1  2005/11/27 19:39:30  gdb
#  *** empty log message ***
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       LINZ::Geodetic::VerticalDatum::Offset
#
#   Description: Defines the offset of a height reference surface
#
#===============================================================================

package LINZ::Geodetic::VerticalDatum::Offset;

sub new
{
    my($class, $offset)=@_;
    my $self={ offset => $offset };
    return bless $self, $class;
}

sub GeoidHeight
{
    my($self, $crd)=@_;
    return -($self->{offset});
}

# Height in terms of base reference surface
sub EllipsoidalHeight {
  my ($self, $crd, $ohgt ) = @_;
  return $ohgt + $self->GeoidHeight( $crd );
  }

# Height in terms of reference surface
sub OrthometricHeight {
  my ($self, $crd, $ehgt ) = @_;
  return $ehgt - $self->GeoidHeight( $crd );
  }
   
#===============================================================================
#
#   Class:       LINZ::Geodetic::VerticalDatum
#
#   Description: Defines the following routines:
#                 $vdatum = new LINZ::Geodetic::VerticalDatum($name, $refcrdsys, $transfunc, $code )
#
#                  $name = $vdatum->name
#
#===============================================================================

package LINZ::Geodetic::VerticalDatum;

my $id;

#===============================================================================
#
#   Method:       new
#
#   Description:  $vdatum = new LINZ::Geodetic::VerticalDatum($name, $refcrdsys, $transfunc, $code )
#
#   Parameters:   $name       The name of the vdatum
#                 $refvdatum  The reference surface that this is defined in terms of
#                             (if it is not in terms of the refcrdsys)
#                 $refcrdsys  Reference ellipsoidal coordinate system for
#                             conversions
#                 $transfunc  An object providing functions OrthometricHeight
#                             EllipsoidalHeight, and GeoidHeight, which convert 
#                             base reference system heights to and from the 
#                             height reference system 
#                 $code       Optional code identifying the vdatum
#
#   Returns:      $vdatum   The blessed ref frame hash
#
#===============================================================================

sub new {
  my ($class, $name, $basevdatum, $refcrdsys, $transfunc, $code ) = @_;
  my $self = { name=>$name, 
               basevdatum=>$basevdatum,
               refcrdsys=>$refcrdsys,
               transfunc=>$transfunc,
               code=>$code,
               id=>$id };
  $id++;
  return bless $self, $class;
  }


sub name { return $_[0]->{name} }
sub code { return $_[0]->{code} }
sub basevdatum { return $_[0]->{basevdatum}  }
sub refcrdsys { return $_[0]->{refcrdsys}  }
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
   my $ohgt=$rcrd->hgt;
   my $vdatum=$self;
   while( $vdatum )
   {
       $ohgt = $vdatum->transfunc->OrthometricHeight( $rcrd, $ohgt );
       $vdatum=$vdatum->basevdatum;
   }
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
   my $vdatum=$self;
   while( $vdatum )
   {
       $ohgt = $vdatum->transfunc->EllipsoidalHeight( $rcrd, $ohgt );
       $vdatum=$vdatum->basevdatum;
   }
   $rcrd->[2] = $ohgt;
   return $rcrd->as( $crd->coordsys );
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
   my( $self, $vdnew, $crd, $ohgt ) = @_;
   my $rcrd = $self->set_ellipsoidal_height( $crd, $ohgt );
   $ohgt = $vdnew->get_orthometric_height( $rcrd );
   return $ohgt;
   }

1;
