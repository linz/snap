#===============================================================================
# Module:             LCCProjection.pm
#
# Description:       Implements the lambert conformal conic projection
#                    Defines packages: 
#                      Geodetic::LCCProjection
#
# Dependencies:      Uses the following modules: 
#                      Geodetic::GeodeticCrd
#                      Geodetic::ProjectionCrd  
#
#  $Id: $
#
#
#
#===============================================================================

use strict;

#===============================================================================
#
#   Class:       Geodetic::LCCProjection
#
#   Description: Defines the following routines:
#                  $tmprj = new Geodetic::LCCProjection($ellipse, $cm, $lto, $sf, $fe, $fn, $utom)
#                  $tmprj->type
#                  $tmprj->geog($crd)
#                  $tmprj->proj($crd)
#
#                The following functions are also defined for internal use
#                  $tmprj->meridian_arc($lt)
#                  $tmprj->foot_point_lat($m)
#
#===============================================================================

package Geodetic::LCCProjection;

require Geodetic::ProjectionCrd;
require Geodetic::GeodeticCrd;

my $pi = atan2(1,1)*4;
my $twopi = $pi * 2;
my $rad2deg = 180/$pi;

my $max_it = 10;
my $tolerance  = 1.0e-9;


#===============================================================================
#
#   Method:       new
#
#   Description:  $tmprj = new Geodetic::LCCProjection($ellipse, $cm, $lto, $sf, $fe, $fn, $utom)
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

sub new
{
    my($class,$ellipse,$sp1,$sp2,$lt0,$ln0,$e0,$n0) = @_;

    my $rf = $ellipse->rf;
    my $a = $ellipse->a;

    my $e = $rf == 0.0 ? 0.0 : 1.0/$rf;
    $e = sqrt(2*$e - $e*$e);

    $sp1 /= $rad2deg;
    $sp2 /= $rad2deg;
    $lt0 /= $rad2deg;
    $ln0 /= $rad2deg;

    my $lp = {};
    $lp->{a} = $a;
    $lp->{rf} = $rf;
    $lp->{e} = $e;
    $lp->{sp1} = $sp1;
    $lp->{sp2} = $sp2;
    $lp->{lt0} = $lt0;
    $lp->{ln0} = $ln0;
    $lp->{e0} = $e0;
    $lp->{n0} = $n0;

    my $rev = ($sp1+$sp2) > 0 ? 1 : -1;
    $sp1 *= $rev;
    $sp2 *= $rev;
    if( $rev < 0 ) { my $tmp = $sp1; $sp1 = $sp2; $sp2 = $tmp; }
    $lt0 *= $rev;
    $lp->{rev} = $rev;

    my $m1 = calc_m( $e, $sp1);
    my $m2 = calc_m( $e, $sp2);
    my $t1 = calc_t( $e, $sp1);
    my $t2 = calc_t( $e, $sp2);

    if( abs($sp1-$sp2) > 1.0e-5 ) {
       $lp->{n} = (log($m1)-log($m2))/(log($t1)-log($t2));
       }
    else {
       $lp->{n} = sin($lp->{sp1});
       }
    $lp->{F} = $m1/($lp->{n} * ($t1 ** $lp->{n}));
    $lp->{r0} = $a*$lp->{F}*(calc_t($e,$lt0) ** $lp->{n});

    return bless $lp, $class;
}

#===============================================================================
#
#   Method:       parameters
#
#   Description:  $params = $proj->parameters;
#                 $params = parameters Geodetic::LCCProjection;
#
#   Parameters:   none
#
#   Returns:      $params    An array of hashes defining the projection
#                            parameters.  The each parameter has
#                            elements code, name, type, value
#
#===============================================================================

sub parameters {
    my ($lp) = @_;
    my $parameters = [
          { code=>'SP1', name=>'First standard parallel', type=>'LT', np=>'sp1' },
          { code=>'SP2', name=>'Second standard parallel', type=>'LT', np=>'sp2' },
          { code=>'LTO', name=>'Origin of latitude', type=>'LT', np=>'lt0' },
          { code=>'LNO', name=>'Origin of longitude', type=>'LN', np=>'ln0' },
          { code=>'FE', name=>'False easting', type=>'D', np=>'e0' },
          { code=>'FN', name=>'False northing', type=>'D', np=>'n0' },
          ];
    if( ref($lp) eq 'Geodetic::LCCProjection' ) {
          foreach (@$parameters) {
               $_->{value} = $lp->{$_->{np}};
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
#   Description:  Simply returns the string "Lambert Conformal Conic Projection"
#
#   Parameters:   None
#
#   Returns:      The type as a string
#
#===============================================================================

sub type {
   return "Lambert Conformal Conic Projection";
   }


#===============================================================================
#
#   Method:       calc_m, calc_t
#
#   Description:  Utility functions for calculating the projection
#
#===============================================================================

sub calc_m 
{
    my ( $e, $lt ) = @_;
    my $es = $e*sin($lt);
    return cos($lt)/sqrt(1-$es*$es);
}

sub calc_t
{
    my ( $e, $lt ) = @_;
    my $s = sin($lt);
    my $es = $e * $s;
    return sqrt((1-$s) * ( ((1+$es)/(1-$es)) ** $e) / ( 1 + $s ));
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
sub geog
{
   my($lp,$crd) = @_;
   my $ce = $crd->[1];
   my $cn = $crd->[0];

    $ce -= $lp->{e0};
    $cn = $lp->{r0} - $lp->{rev}*($cn - $lp->{n0});

    my $r = sqrt( $ce*$ce + $cn*$cn );
    $r = -$r if $lp->{n} < 0;

    my $t = (( $r / ($lp->{a} * $lp->{F})) ** (1.0/$lp->{n}));

    my $lt0 = $pi/2.0 - 2.0*atan2($t,1);
    my $lt1;
    my $es;

    my $it = 0;
    while( $it++ < $max_it )
    {
       $es = $lp->{e} * sin($lt0);
       $lt1 = $pi/2 - 2.0*atan2( $t * ( ((1-$es)/(1+$es)) **  ($lp->{e}/2)), 1);
       last if abs($lt0-$lt1) < $tolerance;
       $lt0 = $lt1;
    }


    $es = $lp->{n} > 0 ? 1 : -1;
    my $ln = atan2( $es * $ce, $es * $cn )/$lp->{n} + $lp->{ln0};
    $ln -= $twopi while $ln > $pi;
    $ln += $twopi while $ln < -$pi;

    return new Geodetic::GeodeticCrd(
               $lt1*$rad2deg*$lp->{rev},
               $ln*$rad2deg,
               $crd->[2]);
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
sub proj
{   
   my($lp,$crd) = @_;
   my $lt = $crd->[0]/$rad2deg*$lp->{rev};
   my $ln = $crd->[1]/$rad2deg;
  
    my $r = $lp->{a}*$lp->{F}*(calc_t($lp->{e}, $lt) ** $lp->{n});
    my $th = $ln-$lp->{ln0};
    $th += $twopi while $th < -$pi;
    $th -= $twopi while $th > $pi;
    $th *= $lp->{n};
    my $ce = $r * sin($th) + $lp->{e0};
    my $cn = $lp->{rev}*($lp->{r0} - $r * cos($th)) + $lp->{n0};
    return new Geodetic::ProjectionCrd($cn, $ce, $crd->[2]);
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
sub calc_sf_conv
{
   my($lp,$crd) = @_;
   my $ce = $crd->[1];
   my $cn = $crd->[0];

   # Note: this could be tidied up somewhat .. at present converts to lat/lon,
   # then does calculation back to e/n, then calculates sf and conv (minimal
   # adaptation of existing code.

    $ce -= $lp->{e0};
    $cn = $lp->{r0} - $lp->{rev}*($cn - $lp->{n0});

    my $r = sqrt( $ce*$ce + $cn*$cn );
    $r = -$r if $lp->{n} < 0;

    my $t = ( ($r / ($lp->{a} * $lp->{F})) ** (1.0/$lp->{n}));

    my $lt0 = $pi/2.0 - 2.0*atan2($t,1);
    my $lt1;
    my $es;

    my $it = 0;
    while( $it++ < $max_it )
    {
       $es = $lp->{e} * sin($lt0);
       $lt1 = $pi/2 - 2.0*atan2( $t * ( ((1-$es)/(1+$es)) ** ($lp->{e}/2) ), 1);
       last if abs($lt0-$lt1) < $tolerance;
       $lt0 = $lt1;
    }


    $es = $lp->{n} > 0 ? 1 : -1;
    my $ln = atan2( $es * $ce, $es * $cn )/$lp->{n} + $lp->{ln0};

    $r = $lp->{a}*$lp->{F}*(calc_t($lp->{e}, $lt1) ** $lp->{n});
    my $th = $ln-$lp->{ln0};
    $th += $twopi while $th < -$pi;
    $th -= $twopi while $th > $pi;
    $th *= $lp->{n};
    $ce = $r * sin($th) + $lp->{e0};
    $cn = $lp->{r0} - $r * cos($th) + $lp->{n0};
 
    $es = $lp->{e} * sin($lt1);
    my $sf = $lp->{n}*$r*sqrt(1-$es*$es)/($lp->{a}*cos($lt1));
    my $cnv = -$th;

    my $result = [$sf,$cnv*$rad2deg*$lp->{rev}];
    return wantarray ? @$result : $result;
}


1;
