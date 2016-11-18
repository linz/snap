#===============================================================================
# Module:             PSProjection.pm
#
# Description:       Implements the polar stereographic projection
#                    Defines packages: 
#                      LINZ::Geodetic::PSProjection
#
# Dependencies:      Uses the following modules: 
#                      LINZ::Geodetic::GeodeticCrd
#                      LINZ::Geodetic::ProjectionCrd  
#
#===============================================================================

use strict;

#===============================================================================
#
#   Class:       LINZ::Geodetic::PSProjection
#
#   Description: Defines the following routines:
#                  $tmprj = new LINZ::Geodetic::PSProjection($ellipse, $cm, $lto, $sf, $fe, $fn, $utom)
#                  $tmprj->type
#                  $tmprj->geog($crd)
#                  $tmprj->proj($crd)
#
#                The following functions are also defined for internal use
#                  $tmprj->meridian_arc($lt)
#                  $tmprj->foot_point_lat($m)
#
#===============================================================================

package LINZ::Geodetic::PSProjection;

require LINZ::Geodetic::ProjectionCrd;
require LINZ::Geodetic::GeodeticCrd;
require LINZ::Geodetic::Isometric;

my $pi = atan2(1,1)*4;
my $twopi = $pi * 2;
my $rad2deg = 180/$pi;


#===============================================================================
#
#   Method:       new
#
#   Description:  $tmprj = new LINZ::Geodetic::PSProjection($ellipse, $cm, $lto, $sf, $fe, $fn, $utom)
#
#   Parameters:   $ellipse    The ellipsoid reference
#                 $ns         North/south ("North" or "South")
#                 $cm         The central meridian (degrees)
#                 $sf         The central meridian scale factor
#                 $fe         The false easting
#                 $fn         The false northing
#
#   Returns:      $tmprj      The projection object
#
#===============================================================================

sub new {
   my ($class, $ellipse, $ns, $cm, $sf, $fe, $fn ) = @_;
   my $a = $ellipse->a;
   my $rf = $ellipse->rf;
   my $f = $rf == 0 ? 0.0 : 1/$rf;
   my $e2 = 2.0*$f - $f*$f;
   my $e = sqrt($e2);
   $ns = $ns =~ /^s(?:outh)?$/i ? -1 : 1;
   $cm /= $rad2deg;
   my $k=2.0*$sf*($a/sqrt(1-$e2))*((1-$e)/(1+$e))**($e/2);
   my $self = [$a,$rf,$f,$e2,$e,$k,$ns,$cm,$sf,$fe,$fn];
   bless $self, $class;
   return $self;
   }

#===============================================================================
#
#   Method:       parameters
#
#   Description:  $params = $proj->parameters;
#                 $params = parameters LINZ::Geodetic::PSProjection;
#
#   Parameters:   none
#
#   Returns:      $params    An array of hashes defining the projection
#                            parameters.  The each parameter has
#                            elements code, name, type, value
#
#===============================================================================

sub parameters {
    my ($self) = @_;
    my $parameters = [
          { code=>'NS', name=>'North/South', type=>'NS', np=>6 },
          { code=>'CM', name=>'Central meridian', type=>'LN', np=>7 },
          { code=>'SF', name=>'Scale factor', type=>'D', np=>8 },
          { code=>'FE', name=>'False easting', type=>'D', np=>9 },
          { code=>'FN', name=>'False northing', type=>'D', np=>10 },
          ];
    if( ref($self) eq 'LINZ::Geodetic::PSProjection' ) {
          foreach (@$parameters) {
               $_->{value} = $self->[$_->{np}];
               if( $_->{type} eq 'LT' || $_->{type} eq 'LN' ) {
                  $_->{value} *= $rad2deg;
                  }
               elsif( $_->{type} eq 'NS' ) {
                  $_->{value} = $_->{value} > 0 ? 'North' : 'South';
                  }
               }
          }
    return $parameters;
    }

#===============================================================================
#
#   Subroutine:   type
#
#   Description:  Simply returns the string "Polar Stereographic Projection"
#
#   Parameters:   None
#
#   Returns:      The type as a string
#
#===============================================================================

sub type {
   return "Polar Stereographic Projection";
   }

#===============================================================================
#
#   Method:       geog
#
#   Description:  $geog = $tmprj->geog($crd)
#
#   Parameters:   $crd        Array reference defining the input coordinates,
#                             (northing, easting, [height])
#
#   Returns:      $geog       The corresponding geodetic coordinate
#
#===============================================================================

sub geodetic { return geog(@_); }
sub geog {
   my($self,$crd) = @_;
   my $ce = $crd->[1];
   my $cn = $crd->[0];
   my ($a,$rf,$f,$e2,$e,$k,$ns,$cm,$sf,$fe,$fn)=@$self;
   $ce -= $fe;
   $cn = $ns*($fn-$cn);

   my $dlam=atan2($ce,$cn);
   my $r=sqrt($ce*$ce+$cn*$cn);
   my $q=log($k/$r);
   my $sphi=LINZ::Geodetic::Isometric::geodetic_from_isometric($q,$e);
   $sphi *= $ns;
   my $slam=$cm+$dlam;
   $slam -= $pi*2 while $slam > $pi;
   $slam += $pi*2 while $slam < -$pi;

   return new LINZ::Geodetic::GeodeticCrd(
               $sphi*$rad2deg,
               $slam*$rad2deg,
               $crd->[2],
               undef,
               $crd->[4] );
   }



#===============================================================================
#
#   Method:       proj
#
#   Description:  $prj = $tmprj->proj($geog)
#
#   Parameters:   $geog       The input geodetic coordinates, and array 
#                             reference containing [lat, lon]
#
#   Returns:      $prj        The corresponding projection coordinates
#                             (array reference containg northing, easting)
#
#===============================================================================

sub projection { return proj(@_); }
sub proj {
   my($self,$crd) = @_;
   my $lt = $crd->[0];
   my $ln = $crd->[1];
   my ($a,$rf,$f,$e2,$e,$k,$ns,$cm,$sf,$fe,$fn)=@$self;

   my $sphi = $lt/$rad2deg;
   my $slam  =  $ln/$rad2deg - $cm;
   
   my ($cn,$ce);
   # If this point is very close to the pole, then just use false easting/northing
   if( abs($sphi-$pi/2) < 0.00015/(3600*$rad2deg))
   {
       $cn=$fn;
       $ce=$fe;
   }
   else
   {
       my $pow=$e/2.0;
       my $a1=$ns*$e*sin($sphi);
       my $a2=$pi/4.0+$ns*$sphi/2.0;
       $a2=sin($a2)/cos($a2);
       my $a3=((1.0-$a1)/(1.0+$a1))**$pow;
       my $r=$k*(1.0/($a2*$a3));
       $ce=$r*sin($slam)+$fe;
       $cn=$fn-$r*cos($slam)*$ns;
   }
   return new LINZ::Geodetic::ProjectionCrd($cn, $ce, $crd->[2], undef, $crd->[4]);
}


#===============================================================================
#
#   Method:       calc_sf_conv
#
#   Description:  $prj = $tmprj->calc_sf_conv($proj)
#
#   Parameters:   $proj       The input projection coordinates, an array 
#                             reference containing [northing, easting]
#
#   Returns:      ($sf,$conv) The scale factor and convergence (in degrees).
#
#===============================================================================

sub sf_conv { return calc_sf_conv(@_); }
sub calc_sf_conv {
   my($self,$crd) = @_;
   die "Scale factor/convergence not define for Polar Stereographic projection\n";
   my $ce = $crd->[1];
   my $cn = $crd->[0];
   my ($a,$rf,$f,$e2,$e,$k,$ns,$cm,$sf,$fe,$fn)=@$self;
   my ($scl,$conv);

   return wantarray ? ($scl,$conv) : [$scl,$conv];
}

1;
