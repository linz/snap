#===============================================================================
# Module:             Projection.pm
#
# Description:       This package manages projection definitions from
#                    the coordinate system definition file.  The definition
#                    consists of a code followed by a list of parameters.  
#                    The module holds the definition, but doesn't implement
#                    it until projection is actually required.  At that time
#                    it looks for a package corresponding to the code, ie
#                    Geodetic::@@Projection.pm where @@ is the code.  The
#                    package is loaded (with require), and then the new
#                    function called to instantiate the projection.  The 
#                    remainder of the definition (after the code) is split
#                    on whitespace and used as input to the constructor.
#
#                    The Projection object is a hash reference with the
#                    following elements
#                      def          the definition string
#                      ellipsoid    the ellipsoid
#                      proj         the installed projection object
#                      testprojcrd  sub reference for testing proj coordinate
#                                   against valid range
#                      testgeogcrd  sub reference for testing geog coordinate
#
#                    Defines packages: 
#                      Geodetic::Projection
#
# Dependencies:      Uses the following modules:   
#
#  $Id: Projection.pm,v 1.3 2000/10/24 02:39:09 ccrook Exp $
#
#  $Log: Projection.pm,v $
#  Revision 1.3  2000/10/24 02:39:09  ccrook
#  Added definition of projection parameters to projection interface.
#
#  Revision 1.2  2000/09/27 11:39:45  gdb
#  Added function to calculate scale factor and convergence.
#
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::Projection
#
#   Description: Defines the following routines:
#                  $proj = new Geodetic::Projection($def, $ellipsoid)
#                  $type = $proj->type
#                  $geog = $proj->geog( $projcrd )
#                  $projcrd = $proj->proj( $projcrd );
#
#                Also implements the internal routine
#                $proj->install which converts the projection
#                definition to a real projection by installing
#                the package and instantiating the object.
#
#===============================================================================

package Geodetic::Projection;


#===============================================================================
#
#   Method:       new
#
#   Description:  $proj = new Geodetic::Projection($def, $ellipsoid)
#
#   Parameters:   $def        The text definition of the projection and params
#                 $ellipsoid  A reference to the ellipsoid used.
#
#   Returns:      $proj       The blessed projection hash reference.
#
#===============================================================================

sub new {
   my ($class, $def, $ellipsoid ) = @_;
   my $self = { def=>$def, ellipsoid=>$ellipsoid };
   return bless $self, $class;
   }


#===============================================================================
#
#   Subroutine:   install
#
#   Description:  Installs the projection object by 'require'ing the 
#                 the corresponding package and instantiating an object.
#                 Also sets up the valid coordinate ranges, both in 
#                 projection coordinates as supplied, and the equivalent
#                 in lat/lon coordinates.
#
#   Parameters:   None
#
#   Returns:      None - dies if fails to create the object
#
#===============================================================================

sub install {
   my $self = shift;
   my $def = $self->{def};
   my $range;
   if( $def =~ /\s*RANGE\s*/ ) {
      $def = $`;
      $range = $';
      }
   my @def = split(' ',$def);
   my $type = uc(shift(@def));
   
   eval 'require Geodetic::'.$type.'Projection';
   die "Invalid projection code $type\n" if $@;

   my $elp = $self->{ellipsoid};
   my $proj = eval 'new Geodetic::'.$type.'Projection($elp,@def)';
   die $@ if $@;
   $self->{proj} = $proj;

   # Test whether we have a valid range definition, and if so determine
   # the corresponding range in lat/lon and set up range test functions.

   my @range = split(' ',$range);
   if( @range == 4 ) {
     my( $emin, $nmin, $emax, $nmax ) = @range;
     my ($lt1,$ln1) = @{$proj->geog([$nmin,$emin])};
     my ($lt2,$ln2) = @{$proj->geog([$nmax,$emin])};
     my ($lt3,$ln3) = @{$proj->geog([$nmin,$emax])};
     my ($lt4,$ln4) = @{$proj->geog([$nmax,$emax])};
     # Assume the valid longitude range is not more than 180 degrees..
     my $lnmin = $ln1 - 180;
     my $lnmax = $ln1 + 180;
     foreach ($ln2, $ln3, $ln4 ) {
        $_ += 360 while $_ < $lnmin;
        $_ -= 360 while $_ > $lnmax;
        }
     ($lnmin,$lnmax) = (sort {$a<=>$b} ($ln1, $ln2, $ln3, $ln4 ))[0,3];
     my ($ltmin,$ltmax) = (sort {$a<=>$b} ($lt1, $lt2, $lt3, $lt4 ))[0,3];
     $self->{testprojcrd} = 
          sub { my $crd = $_[0];
                ($crd->[0] >= $nmin && $crd->[0] <= $nmax &&
                 $crd->[1] >= $emin && $crd->[1] <= $emax ) ||
                 die "Projection coordinates out of valid range\n";
               };
     $self->{testgeogcrd} = 
          sub { my $crd = $_[0];
                my $ln = $crd->[1];
                $ln += 360 while $ln < $lnmin;
                $ln -= 360 while $ln > $lnmax;
                ($ln >= $lnmin &&
                 $crd->[0] >= $ltmin && $crd->[0] <= $ltmax ) ||
                 die "Geodetic coordinates out of valid range\n";
               };
               
     }
   }


#===============================================================================
#
#   Subroutine:   type
#
#   Description:  Returns a text description of the type of projection,
#                 eg "Transverse Mercator"
#
#   Parameters:   None
#
#   Returns:      The type string
#
#===============================================================================

sub type {
   my $self = shift;
   $self->{proj} || $self->install;
   return $self->{proj}->type(@_);
   }


#===============================================================================
#
#   Subroutine:   parameters
#
#   Description:  Returns an array of hashes defining the parameters of the
#                 projection.
#                   $params = $proj->parameters
#
#   Parameters:   none
#
#   Returns:      $params  Array reference for returned parameters
#
#===============================================================================

sub parameters {
   my $self = shift;
   $self->{proj} || $self->install;
   my $params;
   eval {$params = $self->{proj}->parameters};
   return $params;
   }


#===============================================================================
#
#   Subroutine:   geog
#
#   Description:  Converts projection coordinates to lat/lon
#                  $geogcrd = $proj->geog( [north, east] );
#
#   Parameters:   $crd     Array reference for coords
#
#   Returns:      $geog    Array reference for return coordinates
#
#===============================================================================

sub geodetic { return geog(@_); }
sub geog {
   my $self = shift;
   $self->{proj} || $self->install;
   &{$self->{testprojcrd}}(@_) if exists $self->{testprojcrd};
   return $self->{proj}->geog(@_);
   }


#===============================================================================
#
#   Subroutine:   proj
#
#   Description:  Converts lat/lon coordinates to projection coordinates
#                  $projcrd = $proj->proj( [$lat, $lon] );
#
#   Parameters:   $crd    Array reference for lat/lon coords
#
#   Returns:      $prj    Array reference for projection coords
#
#===============================================================================

sub projection { return proj(@_); }
sub proj {
   my $self = shift;
   $self->{proj} || $self->install;
   &{$self->{testgeogcrd}}(@_) if exists $self->{testgeogcrd};
   return $self->{proj}->proj(@_);
   }
  

#===============================================================================
#
#   Subroutine:   sf_conv
#
#   Description:  Calculates the scale factor and convergence for the 
#                 coordinate.
#                  $sfconv = $proj->sf_conv( [north, east] );
#
#   Parameters:   $crd     Array reference for coords
#
#   Returns:      $sfconv  Array reference for [scale_factor, convergence]
#
#===============================================================================

sub sf_conv {
   my $self = shift;
   $self->{proj} || $self->install;
   &{$self->{testprojcrd}}(@_) if exists $self->{testprojcrd};
   my $result;
   eval {
     my($sf,$conv) = $self->{proj}->calc_sf_conv(@_); 
     $result = [$sf,$conv];
     };
   return wantarray ? @$result : $result;
   }

1;
