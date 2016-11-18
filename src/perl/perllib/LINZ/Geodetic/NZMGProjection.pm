#===============================================================================
# Module:             NZMGProjection.pm
#
# Description:       Defines packages: 
#                      Complex
#                      Complex::Polynomial
#                      LINZ::Geodetic::NZMGProjection
#                      Polynomial
#
# Dependencies:      Uses the following modules: 
#                      LINZ::Geodetic::GeodeticCrd
#                      LINZ::Geodetic::ProjectionCrd  
#
#  $Id: NZMGProjection.pm,v 1.2 2000/11/09 23:03:43 gdb Exp $
#
#  $Log: NZMGProjection.pm,v $
#  Revision 1.2  2000/11/09 23:03:43  gdb
#  Modified to handle longitudes wrapping around.
#
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Package:     Polynomial
#
#   Description: Defines the following routines:
#                  $poly = new Polynomial( $a0, $a1, $a2 ... )
#
#                  $result = $poly->evaluate($value)
#                  $result = $poly->evaluateDifferential($value)
#
#===============================================================================

package Polynomial;


#===============================================================================
#
#   Subroutine:   new
#
#   Description:   Create a polynomial from a list of coefficients
#                  $poly = new Polynomial( $a0, $a1, $a2 ... )
#
#   Parameters:    $a0, ...       The list of coefficients
#
#   Returns:       $poly          The polynomial reference
#
#===============================================================================

sub new {
  my $class = shift;
  my @poly = @_;
  return bless \@poly;
  }


#===============================================================================
#
#   Subroutine:   evaluate
#
#   Description:  $result = $poly->evaluate($value)
#
#   Parameters:   $value    The point at which the polynomial is to be
#                           evaluated
#
#   Returns:      $result   The evaluated polynomial
#
#===============================================================================

sub evaluate {
  my ($poly,$value) = @_;
  my $i = $#{$poly};
  my $result = $poly->[$i];
  while($i--) {
     $result = ($result * $value + $poly->[$i]);
     }
   return $result;
   }


#===============================================================================
#
#   Subroutine:   evaluateDifferential
#
#   Description:  $result = $poly->evaluateDifferential($value)
#
#   Parameters:   $value    The point at which the differential is to be
#                           evaluated
#
#   Returns:      $result   The evaluated polynomial
#
#===============================================================================

sub evaluateDifferential {
  my ($poly,$value) = @_;
  my $i = $#{$poly};
  my $result = $poly->[$i] * $i;
  while(--$i > 0) {
     $result = ($result * $value + $poly->[$i] * $i );
     }
   return $result;
   }

#===============================================================================
#
#   Class:       Complex
#
#   Description: A basic Complex class.
#                Defines the following routines:
#                  $c = new Complex( $real, $imag );
#
#                  $real = $c->real
#                  $imag = $c->imag
#
#                  $c3 = $c1->add($c2)
#                  $c2 = $c1->scaledBy( $factor )
#                  $c2 = $c1->reciprocal
#                  $c3 = $c1->multiply($c2)
#                  $c3 = $c1->divide($c2)
#
#===============================================================================

package Complex;

sub new {
   my $self = bless [$_[1], $_[2]];
   bless $self;
   }

sub real { return $_[0]->[0]; }

sub imag { return $_[0]->[1]; }

sub add {
   my $sum = new Complex(0,0);
   foreach (@_) { $sum->[0] += $_->[0]; $sum->[1] += $_->[1]; }
   return $sum;
   }

sub scaledBy {
   my( $self, $factor) = @_;
   return new Complex($self->[0]*$factor, $self->[1]*$factor);
   }

sub reciprocal {
   my $self = shift;
   my ($r,$i) = @$self;
   my $len = $r*$r+$i*$i;
   return new Complex( $r/$len, -$i/$len);
   }

sub multiply {
   my ($c1, $c2) = @_;
   return new Complex( $c1->[0]*$c2->[0]-$c1->[1]*$c2->[1], $c1->[0]*$c2->[1]+$c1->[1]*$c2->[0] );
   }

sub divide {
   my ($c1, $c2) = @_;
   return multiply($c1,reciprocal($c2));
   }

#===============================================================================
#
#   Package:     Complex::Polynomial
#
#   Description: Defines a complex polynomial object as follows:
#                  $cpoly = new ComplexPolynomial( [r0,i0], [r1,i1], ... )
#                  $result = $cpoly->evaluate( $cvalue );
#
#===============================================================================

package Complex::Polynomial;

sub new {
   my $class = shift;
   my @poly = @_;
   return bless \@poly;
   }

sub evaluate {
   my($poly, $value) = @_;
   my $i = $#{$poly};
   my $result = $poly->[$i];
   while($i--) {
      $result = Complex::add( $poly->[$i],Complex::multiply($result,$value));
      }
   return bless $result, 'Complex';
   }

   
#===============================================================================
#
#   Class:       LINZ::Geodetic::NZMGProjection
#
#   Description: Defines the following routines:
#                  $nzproj = new LINZ::Geodetic::NZMGProjection;
#                  $type = $nzproj->type
#
#                  $geog = $nzproj->geog($crd)
#                  $crd = $nzproj->proj($geog)
#
#===============================================================================

package LINZ::Geodetic::NZMGProjection;

require LINZ::Geodetic::ProjectionCrd;
require LINZ::Geodetic::GeodeticCrd;

use vars qw/
  $a
  $n0
  $e0
  $lt0
  $ln0
  $rad2deg
  $cfi
  $cfl
  $cfb1
  $cfb2
  $cfb1n
  $cfb1d
  /;

BEGIN {
$a =  6378388.0;
$n0 = 6023150.0;
$e0 = 2510000.0;
$lt0 = -41.0;
$ln0 = 173.0;

$rad2deg = 57.29577951308232;

$cfi = new Polynomial (
                          0.6399175073,
                         -0.1358797613,
                           0.063294409,
                           -0.02526853,
                             0.0117879,
                            -0.0055161,
                             0.0026906,
                             -0.001333,
                               0.00067,
                              -0.00034 );

$cfl = new Polynomial (
                          1.5627014243,
                          0.5185406398,
                           -0.03333098,
                            -0.1052906,
                            -0.0368594,
                              0.007317,
                               0.01220,
                               0.00394,
                               -0.0013 );

$cfb1 = new Complex::Polynomial (
                          [0.7557853228,           0.0],
                          [ 0.249204646,   0.003371507],
                          [-0.001541739,   0.041058560],
                          [ -0.10162907,    0.01727609],
                          [ -0.26623489,   -0.36249218],
                          [  -0.6870983,    -1.1651967] );

$cfb2 = new Complex::Polynomial (
                          [0.0,                    0.0],
                          [1.3231270439,           0.0],
                          [-0.577245789,  -0.007809598],
                          [ 0.508307513,  -0.112208952],
                          [ -0.15094762,    0.18200602],
                          [  1.01418179,    1.64497696],
                          [   1.9660549,     2.5127645] );

$cfb1n = new Complex::Polynomial ( @{$cfb1} );
$cfb1d = new Complex::Polynomial ( @{$cfb1} );
my $i;
for $i (0..5) {
   $cfb1n->[$i] = Complex::scaledBy( $cfb1n->[$i], $i );
   $cfb1d->[$i] = Complex::scaledBy( $cfb1d->[$i], $i+1 );
   }
unshift(@$cfb1n, [0,0] );

}


#===============================================================================
#
#   Subroutine:   new
#
#   Description:   Create the NZMG projection object ... nothing much to this!
#                   $nzmgproj = new LINZ::Geodetic::NZMGProjection
#
#   Parameters:    None
#
#   Returns:      
#
#===============================================================================

sub new {
   my $self = 'NZMG';
   return bless \$self;
   }

#===============================================================================
#
#   Subroutine:   type
#
#   Description:   Returns the projection type (ie New Zealand Map Grid)
#                  $type = $nzproj->type
#
#   Parameters:    None
#
#   Returns:       The projection type as a string
#
#===============================================================================

sub type {
  return "New Zealand Map Grid";
  }

sub parameters {
  return [];
  }

#===============================================================================
#
#   Method:       geog
#
#   Description:  $geog = $nzproj->geog($crd)
#                 Converts coordinates from easting, northing, height to
#                 lat, lon, height.
#
#   Parameters:   $crd       The definition of the input coordinates as a
#                            array reference
#
#   Returns:      $geog      The returned coordinates as a
#                            LINZ::Geodetic::GeodeticCrd blessed reference
#
#===============================================================================

sub geodetic { return geog(@_); }
sub geog {
   my ($self,$crd) = @_;
   my $e = $crd->[1];
   my $n = $crd->[0];
   my $z0 = new Complex(($n-$n0)/$a,($e-$e0)/$a);
   my $z1 = $cfb2->evaluate( $z0 );
   foreach (0..1) {
      my $zn = $cfb1n->evaluate($z1);
      $zn = Complex::add( $z0, $zn );
      my $zd = $cfb1d->evaluate($z1);
      $z1 = Complex::divide( $zn, $zd );
      }
   my $ln = $ln0 + $z1->imag * $rad2deg;
   my $lt = $lt0 + $cfl->evaluate($z1->real) * $z1->real / 0.036;
   return new LINZ::Geodetic::GeodeticCrd( $lt, $ln, $crd->[2], undef, $crd->[4] );
   }


#===============================================================================
#
#   Method:       proj
#
#   Description:  $crd = $nzproj->proj($geog)
#
#   Parameters:   $geog    The lat/lon/height coordinate as an array reference
#
#   Returns:      $crd     The return easting, northing, height as a
#                          LINZ::Geodetic::ProjectionCrd blessed array reference.
#
#===============================================================================

sub projection { return proj(@_); }
sub proj {
   my ($self,$crd)=@_;
   my $lt = ($crd->[0] - $lt0)*0.036;
   $lt = $cfi->evaluate($lt) * $lt;
   my $ln = $crd->[1] - $ln0;
   $ln += 360 while $ln < -180;
   $ln -= 360 while $ln > 180;
   my $z1 = new Complex($lt,$ln/$rad2deg);
   my $z0 = Complex::multiply( $cfb1->evaluate($z1), $z1 );
   return new LINZ::Geodetic::ProjectionCrd(
             $n0 + $z0->real * $a,
             $e0 + $z0->imag * $a, 
             $crd->[2],
             undef, 
             $crd->[4] );
   }

1;
