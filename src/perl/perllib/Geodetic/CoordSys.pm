#===============================================================================
# Module:             CoordSys.pm
#
# Description:       Defines packages: 
#                      Geodetic::CoordSys
#
# Dependencies:      Uses the following modules: 
#                      Geodetic::CoordConversion
#                      Geodetic::CartesianCrd
#                      Geodetic::GeodeticCrd
#                      Geodetic::ProjectionCrd  
#
#  $Id: CoordSys.pm,v 1.3 2000/11/14 03:20:26 gdb Exp $
#
#  $Log: CoordSys.pm,v $
#  Revision 1.3  2000/11/14 03:20:26  gdb
#  Modified to ensure that when a coordinate is converted to the input
#  coordinate system it creates a new object, not a copy of the reference.
#
#  Revision 1.2  2000/10/15 18:55:14  ccrook
#  Modification to coordsys functions to allow them to use a distortion grid
#  definition for coordinate conversions.
#
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::CoordSys
#
#   Description: Defines the following routines:
#                Constructor
#                  $cs = new Geodetic::CoordSys($type, $name, $datum, $projection)
#
#                Access functions for components of the coordinate system
#                  $name = $cs->name
#                  $datum = $cs->datum
#                  $ellipsoid = $cs->ellipsoid;
#                  $projection = $cs->projection
#
#                Obtain related coordinate systems
#                  $csgeog = $cs->asgeog
#                  $csxyz = $cs->asxyz
#
#                Create a coordinate in the coordinate system
#                  $crd = $cs->coord( $ord1, $ord2, $ord3 )
#
#                Define a conversion to another coordinate system
#                  $conv = $cs->conversionto($cstarget)
#
#                The coordinate system object is a blessed hash with 
#                the following components:
#
#                 csid        A uniquely defined identifier for the coordinate
#                             system used as a hash key for conversion functions
#                 name        The name of the system
#                 type        The type, one of &Geodetic::CARTESIAN, 
#                             &Geodetic::GEODETIC, &Geodetic::PROJECTION
#                 datum    The datum object used by the 
#                             coordinate system.
#                 projection  The projection used by the projection
#                 _conv       A reference to a hash of conversion functions
#                             from this coordinate system.  The key for the
#                             hash is the target coordinate system id.
#                  
#               The coordinate system may install pointers in the reference
#               frame when it is constructed.  Geocentric coordinate systems
#               install themselves as $datum->{_cscart} and geodetic systems
#               as $datum->{_csgeod}.
#
#===============================================================================

package Geodetic::CoordSys;

require Geodetic::CoordConversion;
require Geodetic::GeodeticCrd;
require Geodetic::CartesianCrd;
require Geodetic::ProjectionCrd;

my $csid = 0;


#===============================================================================
#
#   Method:       new
#
#   Description:  $cs = new Geodetic::CoordSys($type, $name, $datum, $projection)
#
#   Parameters:   $type       The type of the coordinate system - one of
#                             &Geodetic::GEODETIC, &Geodetic::CARTESIAN,
#                             or &Geodetic::PROJECTION
#                 $name       The name of the coordinate system
#                 $datum      The datum object
#                 $projection The projection object (for projection c/s)
#                 $code       Optional code for the coordinate system
#
#   Returns:      $cs         The coordinate system object
#
#===============================================================================

sub new {
  my ($class, $type, $name, $datum, $projection, $code ) = @_;
  $class = ref($class) if ref($class);
 
  # For geocentric and geodetic systems only want one version for
  # the datum - check if it is already defined
  # This does mean that the name may get corrupted :-(
  # Fix up the name if with the supplied name if it is not blank.

  my $self;
  $self = $datum->{_cscart} if $type == &Geodetic::CARTESIAN;
  $self = $datum->{_csgeod} if $type == &Geodetic::GEODETIC;

  if( $self ) {
    $self->{name} = $name if $name;
    return $self;
    }

  # Projection isn't required if not a projection coordinate system

  if( $type == &Geodetic::PROJECTION ) {
     die "Missing projection definition\n" if ! $projection;
     }
  else {
     $projection = undef;
     }

  # Invent a name for geodetic/geocentric systems if none supplied..

  if( ! $name ) {
     $name = $datum->name if $type == &Geodetic::GEODETIC;
     $name = $datum->name." (XYZ)" if $type == &Geodetic::CARTESIAN;
     }

  # Now need to create the projection
  $self = {name=>$name, code=>$code, type=>$type, datum=>$datum,
                  projection=>$projection, csid=>$csid};
  $csid++;
  bless $self, $class;

  # If it is geodetic or geocentric then store in the datum...

  $datum->{_cscart} = $self if $type == &Geodetic::CARTESIAN;
  $datum->{_csgeod} = $self if $type == &Geodetic::GEODETIC;

  return $self;
  }

#===============================================================================
#
#   Subroutine:   name
#                 code
#                 datum
#                 ellipsoid
#                 projection
#                 type
#===============================================================================

sub name { return $_[0]->{name} }
sub code { return $_[0]->{code} }
sub datum { return $_[0]->{datum}}
sub ellipsoid { return $_[0]->datum->ellipsoid; }
sub projection { return $_[0]->{projection} }
sub type { return $_[0]->{type}}

#===============================================================================
#
#   Subroutine:   asgeog
#
#   Description:   $csgeog = $cs->asgeog
#                  Returns the geodetic (lat/lon) coordinate system
#                  related to the specified coordinate system.  This will
#                  either return an existing reference (if one is defined for
#                  the datum), otherwise it will create a new c/s
#
#
#   Parameters:    None
#
#   Returns:       The geographic coordinate system
#
#===============================================================================

sub asgeog { return $_[0]->new( &Geodetic::GEODETIC, undef, $_[0]->datum);}
sub asgeodetic { return $_[0]->new( &Geodetic::GEODETIC, undef, $_[0]->datum);}

#===============================================================================
#
#   Subroutine:   asxyz
#
#   Description:   $csxyz = $cs->asxyz
#                  Returns the geocentric (X, Y, Z) coordinate system
#                  related to the specified coordinate system.  This will
#                  either return an existing reference (if one is defined for
#                  the datum), otherwise it will create a new c/s
#
#
#   Parameters:    None
#
#   Returns:       The geocentric coordinate system
#
#===============================================================================

sub asxyz { return $_[0]->new( &Geodetic::CARTESIAN, undef, $_[0]->datum);}
sub ascartesian { return $_[0]->new( &Geodetic::CARTESIAN, undef, $_[0]->datum);}


#===============================================================================
#
#   Subroutine:   coord
#
#   Description:   Returns a coordinate system defined for the 
#                  coordinate system
#
#   Parameters:    Either a reference to an array, or a list of ordinates
#
#   Returns:       The blessed Geodetic::Coordinate
#
#===============================================================================

sub coord {
   my $self = shift;
   my $crd = $_[0];
   $crd = [$crd, $_[1], $_[2]] if ! ref($crd);
   my $type = $self->{type};
   if( $type == &Geodetic::GEODETIC ) {
      $crd = new Geodetic::GeodeticCrd( $crd );
      }
   elsif( $type == &Geodetic::CARTESIAN ) {
      $crd = new Geodetic::CartesianCrd( $crd );
      }
   else {
      $crd = new Geodetic::ProjectionCrd( $crd );
      }
   $crd->setcs( $self );
   return $crd;
}


#===============================================================================
#
#   Method:       conversionto
#
#   Description:  $conv = $cs->conversionto( $cstarget, $conversion_epoch );
#                 The coordinate system maintains a list of constructed
#                 conversion functions.  If the target system is not in
#                 this list then the function reference is constructed
#                 by creating a definition string that is passed to
#                 eval to build the function.
#
#   Parameters:   $cstarget          The target coordinate system
#                 $conversion_epoch  Optional ref epoch when transformations
#                                    between reference frames are computed
#
#   Returns:      $conv      The blessed coordinate conversion object
#
#===============================================================================

sub conversionto {
   my ($self, $target, $conversion_epoch) = @_;
   my $result = $self->{_conv}->{$target->{csid}};
   return $result if $result && $result->conversion_epoch == $conversion_epoch;

   my $src_datum = $self->datum;
   my $tar_datum = $target->datum;
   die "Cannot convert ".$self->name." to ".$target->name."\n"
      if $src_datum->baseref ne $tar_datum->baseref;

   my $def = 'sub {';
   $def .= 'my $crd = bless [@{$_[0]}], ref($_[0]) if ref($_[0]) ne \'ARRAY\';';
   $def .= 'my $target_epoch = $_[1];';

   my $type = $self->{type};

   if ( $target->{csid} == $self->{csid} && !$src_datum->defmodel ) {
      $def .= 'return $crd;';
      }
   else {
      if( $type == &Geodetic::PROJECTION ) {
         $def .= '$crd = $self->projection->geog($crd);';
         $type = &Geodetic::GEODETIC;
         }

      if( $src_datum ne $tar_datum ) {
         if( $type == &Geodetic::GEODETIC ){
            $def .= '$crd = $self->ellipsoid->xyz($crd);';
            $type = &Geodetic::CARTESIAN;
            }
         $def .= '$crd = $src_datum->transfunc->ApplyTo($crd);';
         $def .= '$crd = $tar_datum->transfunc->ApplyInverseTo($crd);';

         my $src_defmodel  = $src_datum->defmodel;
         my $tar_defmodel  = $tar_datum->defmodel;
         if ( $src_defmodel || $tar_defmodel) {
            if( !$conversion_epoch && $src_defmodel && $tar_defmodel ) {
               if( $src_defmodel->RefEpoch ne $tar_defmodel->RefEpoch ) {
                  die "The transformation epoch between reference frames are "
                     ."not the same\n";
                  }
               }
            $def .= 'if ($crd->epoch || $target_epoch) {';
            if ($src_defmodel) {
               $conversion_epoch = $src_defmodel->RefEpoch
                  unless $conversion_epoch;
               $def .= '$crd->setepoch($src_datum->defmodel->RefEpoch) ';
               }
            else {
               $conversion_epoch = $tar_defmodel->RefEpoch
                  unless $conversion_epoch;
               $def .= '$crd->setepoch($tar_datum->defmodel->RefEpoch) ';
               }
            $def .= 'if !$crd->epoch;';

            if( $src_defmodel ) {
               $def .= '$crd = $src_datum->defmodel';
               $def .= '->ApplyTo($crd, $conversion_epoch);';
               }
            if ( $tar_defmodel ) {
               $def .= '$crd = $tar_datum->defmodel';
               $def .= '->ApplyInverseTo($crd, $conversion_epoch);';
               }

            $def .= '$crd->setepoch($conversion_epoch);';

            if ($tar_defmodel) {
               $def .= '$target_epoch = $tar_datum->defmodel->RefEpoch '
               }
            else {
               $def .= '$target_epoch = $src_datum->defmodel->RefEpoch '
               }
            $def .= 'if !$target_epoch;';

            if ( $tar_defmodel ) {
               $def .= '$crd = $tar_datum->defmodel';
               $def .= '->ApplyTo($crd, $target_epoch);';
               }
            if ( $src_defmodel ) {
               $def .= '$crd = $src_datum->defmodel';
               $def .= '->ApplyInverseTo($crd, $target_epoch);';
               }
            }
            $def .= '}';
         }
      else {
         if( $src_datum->defmodel ) {
            $def .= 'if ($crd->epoch) {';
            $def .= '$target_epoch = $src_datum->defmodel->RefEpoch ';
            $def .= 'if !$target_epoch;';
            $def .= '$crd = $self->datum->defmodel';
            $def .= '->ApplyInverseTo($crd, $target_epoch);';
            $def .= '}';
            }
         }

      if( $type != $target->{type} ) {
         if( $target->{type} == &Geodetic::CARTESIAN ) {
            # If got here then didn't change datums, and
            # $type must be geodetic, so convert to geocentric
            $def .= '$crd = $self->ellipsoid->xyz($crd);';
            }
         else {
            # If type is geocentric then we must want geodetic or
            # projection, so convert to geodetic
            if( $type == &Geodetic::CARTESIAN ) {
               $def .= '$crd = $target->ellipsoid->geog($crd);';
               $type = &Geodetic::GEODETIC;
               }
            if( $target->{type} == &Geodetic::PROJECTION ) {
               $def .= '$crd = $target->projection->proj($crd);';
               }
           }
   $def .= 'return $crd;';
        }
      }
   $def .= '}';

   $result = new Geodetic::CoordConversion(
      $self, $target, eval $def, $conversion_epoch );

   # Store the result indexed by the target coordinate system id to 
   # avoid having to recalculate when requested again.

   $self->{_conv}->{$target->{csid}} = $result;
   return $result;
   } 

         
      
1;
