#===============================================================================
# Module:             TMProjection.pm
#
# Description:       Implements the transverse mercator projection
#                    Defines packages: 
#                      LINZ::Geodetic::TMProjection
#
# Dependencies:      Uses the following modules: 
#                      LINZ::Geodetic::GeodeticCrd
#                      LINZ::Geodetic::ProjectionCrd  
#
#  $Id: TMProjection.pm,v 1.5 2001/10/16 22:14:40 ccrook Exp $
#
#  $Log: TMProjection.pm,v $
#  Revision 1.5  2001/10/16 22:14:40  ccrook
#  Updated Transverse Mercator algorithm to match NZTM documentation
#
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
#   Class:       LINZ::Geodetic::TMProjection
#
#   Description: Defines the following routines:
#                  $tmprj = new LINZ::Geodetic::TMProjection($ellipse, $cm, $lto, $sf, $fe, $fn, $utom)
#                  $tmprj->type
#                  $tmprj->geog($crd)
#                  $tmprj->proj($crd)
#
#                The following functions are also defined for internal use
#                  $tmprj->meridian_arc($lt)
#                  $tmprj->foot_point_lat($m)
#
#===============================================================================

package LINZ::Geodetic::TMProjection;

require LINZ::Geodetic::ProjectionCrd;
require LINZ::Geodetic::GeodeticCrd;

my $pi = atan2(1,1)*4;
my $twopi = $pi * 2;
my $rad2deg = 180/$pi;


#===============================================================================
#
#   Method:       new
#
#   Description:  $tmprj = new LINZ::Geodetic::TMProjection($ellipse, $cm, $lto, $sf, $fe, $fn, $utom)
#
#   Parameters:   $ellipse    The ellipsoid reference
#                 $cm         The central meridian (degrees)
#                 $lto        The origin of latitude (degrees)
#                 $sf         The central meridian scale factor
#                 $fe         The false easting
#                 $fn         The false northing
#                 $utom       The unit to metres conversion (default 1)
#
#   Returns:      $tmprj      The projection object
#
#===============================================================================

sub new {
   my ($class, $ellipse, $cm, $lto, $sf, $fe, $fn, $utom ) = @_;
   $utom = 1 if ! defined ($utom);
   my $a = $ellipse->a;
   my $rf = $ellipse->rf;
   my $f = $rf == 0 ? 0.0 : 1/$rf;
   my $e2 = 2.0*$f - $f*$f;
   my $ep2 = $e2/( 1.0 - $e2 );
   $cm /= $rad2deg;
   $lto /= $rad2deg;
   my $self = [$a,$rf,$f,$e2,$ep2,$cm,$sf,$lto,$fe,$fn,$utom];
   bless $self, $class;
   push( @$self, ($self->meridian_arc( $lto )));
   return $self;
   }

#===============================================================================
#
#   Method:       parameters
#
#   Description:  $params = $proj->parameters;
#                 $params = parameters LINZ::Geodetic::TMProjection;
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
          { code=>'CM', name=>'Central meridian', type=>'LN', np=>5 },
          { code=>'LTO', name=>'Origin of latitude', type=>'LT', np=>7 },
          { code=>'SF', name=>'Central meridian scale factor', type=>'D', np=>6 },
          { code=>'FE', name=>'False easting', type=>'D', np=>8 },
          { code=>'FN', name=>'False northing', type=>'D', np=>9 },
          { code=>'UTOM', name=>'Units to metres conversion', type=>'D', np=>10 },
          ];
    if( ref($self) eq 'LINZ::Geodetic::TMProjection' ) {
          foreach (@$parameters) {
               $_->{value} = $self->[$_->{np}];
               if( $_->{type} eq 'LT' || $_->{type} eq 'LN' ) {
                  $_->{value} *= $rad2deg;
                  }
               }
          }
    return $parameters;
    }

#===============================================================================
#
#   Subroutine:   type
#
#   Description:  Simply returns the string "Transverse Mercator Projection"
#
#   Parameters:   None
#
#   Returns:      The type as a string
#
#===============================================================================

sub type {
   return "Transverse Mercator Projection";
   }


#===============================================================================
#
#   Method:       meridian_arc
#
#   Description:  $m = $tmprj->meridian_arc($lt)
#
#   Parameters:   $lt         The latitude
#
#   Returns:      The "meridian arc"!
#
#===============================================================================

sub meridian_arc {
   my ($self,$lt) = @_;
   my ($a,$rf,$f,$e2,$ep2,$cm,$sf,$lto,$fe,$fn,$utom) = @$self;

   my $e4 = $e2*$e2;
   my $e6 = $e4*$e2;

   my $A0 = 1 - ($e2/4.0) - (3.0*$e4/64.0) - (5.0*$e6/256.0);
   my $A2 = (3.0/8.0) * ($e2+$e4/4.0+15.0*$e6/128.0);
   my $A4 = (15.0/256.0) * ($e4 + 3.0*$e6/4.0);
   my $A6 = 35.0*$e6/3072.0;

   return  $a*($A0*$lt-$A2*sin(2*$lt)+$A4*sin(4*$lt)-$A6*sin(6*$lt));
   }



#===============================================================================
#
#   Method:       foot_point_lat
#
#   Description:  $lt = $tmprj->foot_point_lat($m)
#
#   Parameters:   $m     Converts the meridian arc to a latitude
#
#   Returns:      
#
#===============================================================================

sub foot_point_lat {
   my ($self,$m) = @_;
   my ($a,$rf,$f,$e2,$ep2,$cm,$sf,$lto,$fe,$fn,$utom) = @$self;

   my $n  = $f/(2.0-$f);
   my $n2 = $n*$n;
   my $n3 = $n2*$n;
   my $n4 = $n2*$n2;

   my $g = $a*(1.0-$n)*(1.0-$n2)*(1+9.0*$n2/4.0+225.0*$n4/64.0);
   my $sig = $m/$g;

   my $phio = $sig + (3.0*$n/2.0 - 27.0*$n3/32.0)*sin(2.0*$sig)
                   + (21.0*$n2/16.0 - 55.0*$n4/32.0)*sin(4.0*$sig)
                   + (151.0*$n3/96.0) * sin(6.0*$sig)
                   + (1097.0*$n4/512.0) * sin(8.0*$sig);

   return $phio;
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
   my ($a,$rf,$f,$e2,$ep2,$cm,$sf,$lto,$fe,$fn,$utom,$om) = @$self;

   my $cn1  =  ($cn - $fn)*$utom/$sf + $om;
   my $fphi = $self->foot_point_lat($cn1);
   my $slt = sin($fphi);
   my $clt = cos($fphi);

   my $eslt = (1.0-$e2*$slt*$slt);
   my $eta = $a/sqrt($eslt);
   my $rho = $eta * (1.0-$e2) / $eslt;
   my $psi = $eta/$rho;

   my $E = ($ce-$fe)*$utom;
   my $x = $E/($eta*$sf);
   my $x2 = $x*$x;


   my $t = $slt/$clt;
   my $t2 = $t*$t;
   my $t4 = $t2*$t2;

   my $trm1 = 1.0/2.0;

   my $trm2 = ((-4.0*$psi
                +9.0*(1-$t2))*$psi
                +12.0*$t2)/24.0;

   my $trm3 = ((((8.0*(11.0-24.0*$t2)*$psi
                 -12.0*(21.0-71.0*$t2))*$psi
                 +15.0*((15.0*$t2-98.0)*$t2+15))*$psi
                 +180.0*((-3.0*$t2+5.0)*$t2))*$psi + 360.0*$t4)/720.0;

   my $trm4 = (((1575.0*$t2+4095.0)*$t2+3633.0)*$t2+1385.0)/40320.0;

   my $lt = $fphi+($t*$x*$E/($sf*$rho))*((($trm4*$x2-$trm3)*$x2+$trm2)*$x2-$trm1);

   $trm1 = 1.0;

   $trm2 = ($psi+2.0*$t2)/6.0;

   $trm3 = (((-4.0*(1.0-6.0*$t2)*$psi
              +(9.0-68.0*$t2))*$psi
              +72.0*$t2)*$psi
              +24.0*$t4)/120.0;

   $trm4 = (((720.0*$t2+1320.0)*$t2+662.0)*$t2+61.0)/5040.0;

   my $ln = $cm - ($x/$clt)*((($trm4*$x2-$trm3)*$x2+$trm2)*$x2-$trm1);

   return new LINZ::Geodetic::GeodeticCrd(
               $lt*$rad2deg,
               $ln*$rad2deg,
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
   my ($a,$rf,$f,$e2,$ep2,$cm,$sf,$lto,$fe,$fn,$utom,$om) = @$self;

   $ln /= $rad2deg;
   $lt /= $rad2deg;
   my $dlon  =  $ln - $cm;
   $dlon -= $twopi while $dlon > $pi;
   $dlon += $twopi while $dlon < -$pi;

   my $m = $self->meridian_arc($lt);

   my $slt = sin($lt);

   my $eslt = (1.0-$e2*$slt*$slt);
   my $eta = $a/sqrt($eslt);
   my $rho = $eta * (1.0-$e2) / $eslt;
   my $psi = $eta/$rho;

   my $clt = cos($lt);
   my $w = $dlon;

   my $wc = $clt*$w;
   my $wc2 = $wc*$wc;

   my $t = $slt/$clt;
   my $t2 = $t*$t;
   my $t4 = $t2*$t2;
   my $t6 = $t2*$t4;

   my $trm1 = ($psi-$t2)/6.0;

   my $trm2 = (((4.0*(1.0-6.0*$t2)*$psi 
                 + (1.0+8.0*$t2))*$psi 
                 - 2.0*$t2)*$psi+$t4)/120.0;

   my $trm3 = (61 - 479.0*$t2 + 179.0*$t4 - $t6)/5040.0;

   my $ce = ($sf*$eta*$dlon*$clt)*((($trm3*$wc2+$trm2)*$wc2+$trm1)*$wc2+1.0);
   $ce = $ce/$utom+$fe;

   $trm1 = 1.0/2.0;

   $trm2 = ((4.0*$psi+1)*$psi-$t2)/24.0;

   $trm3 = ((((8.0*(11.0-24.0*$t2)*$psi
               -28.0*(1.0-6.0*$t2))*$psi
               +(1.0-32.0*$t2))*$psi 
               -2.0*$t2)*$psi
               +$t4)/720.0;

   my $trm4 = (1385.0-3111.0*$t2+543.0*$t4-$t6)/40320.0;

   my $cn = ($eta*$t)*(((($trm4*$wc2+$trm3)*$wc2+$trm2)*$wc2+$trm1)*$wc2);
   $cn = ($cn+$m-$om)*$sf/$utom+$fn;

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
   my $ce = $crd->[1];
   my $cn = $crd->[0];
   my ($a,$rf,$f,$e2,$ep2,$cm,$sf,$lto,$fe,$fn,$utom,$om) = @$self;

   my $cn1  =  ($cn - $fn)*$utom/$sf + $om;
   my $fphi = $self->foot_point_lat($cn1);
   my $slt = sin($fphi);
   my $clt = cos($fphi);

   my $eslt = (1.0-$e2*$slt*$slt);
   my $eta = $a/sqrt($eslt);
   my $rho = $eta * (1.0-$e2) / $eslt;
   my $psi = $eta/$rho;

   my $E = ($ce-$fe)*$utom;
   my $x = $E/($eta*$sf);
   my $x2 = $x*$x;

   my $t = $slt/$clt;
   my $t2 = $t*$t;

   my $trm1 = 1.0;
   my $trm2 = ((-2.0*$psi + 3.0)*$psi + $t2)/3.0;
   my $trm3 = (((((11.0-24.0*$t2)*$psi
                  -(24.0-69.0*$t2))*$psi
                  +(15.0-70.0*$t2))*$psi
                  +30.0*$t2)*$psi
                  +3.0*$t2*$t2)/15.0;
   my $trm4 = (((45.0*$t2+105.0)*$t2+77.0)*$t2+17.0)/315.0;

   my $conv = - $t*$x*((($trm4*$x2-$trm3)*$x2+$trm2)*$x2-$trm1);
   $conv *= $rad2deg;

   my $xx = $x * $E/($rho*$sf);

   $trm1 = 1.0;
   $trm2 = 0.5;
   $trm3 = ($psi*(4.0-24.0*$t2)-(3.0-48.0*$t2)-24.0*$t2/$psi)/24.0;
   $trm4 = 1.0/720.0;

   my $scl = $sf*((($trm4*$xx+$trm3)*$xx+$trm2)*$xx+$trm1);

   return wantarray ? ($scl,$conv) : [$scl,$conv];
}

1;
