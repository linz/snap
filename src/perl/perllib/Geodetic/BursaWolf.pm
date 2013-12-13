#===============================================================================
# Module:             BursaWolf.pm
#
# Description:       Defines packages: 
#                      Geodetic::BursaWolf
#                    This implements the Bursa Wolf 7 parameter transformation 
#                    to convert between different datums.
#
# Dependencies:      Uses the following modules: 
#                      Geodetic::CartesianCrd  
#
#  $Id: BursaWolf.pm,v 1.1 1999/09/09 21:09:36 ccrook Exp $
#
#  $Log: BursaWolf.pm,v $
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::BursaWolf
#
#   Description: Defines the following methods:
#                  $bw = new Geodetic::BursaWolf($tx,$ty,$tz,$rx,$ry,$rz,$ppm)
#                  $crd2 = $bw->ApplyTo($crd)
#                  $crd2 = $bw->ApplyInverseTo($crd)
#
#===============================================================================

package Geodetic::BursaWolf;

require Geodetic::CartesianCrd;

my $sec2rad = atan2(1,1)/(45.0*3600.0);


#===============================================================================
#
#   Method:       new
#
#   Description:  $bw = new Geodetic::BursaWolf($tx, $ty, $tz, $rx, $ry, $rz, $ppm,
#                                             $refy,$dtx,$dty,$dtz,$drx,$dry,$drz,
#                                             $dppm,$iers)
#
#   Parameters:   $tx,$ty,$tz     The X, Y, and Z translation components in metres
#                 $rx,$ry,$rz     The X, Y, and Z rotations in arc seconds
#                 $ppm            The scale factor, part per million
#                 $refy,          The reference year for 14 param transformations 
#                 $dtx,$dty,$dtz  The X, Y, and Z translation chnge components in metres/year
#                 $drx,$dry,$drz  The X, Y, and Z rotation change in arc seconds/year
#                 $dppm           The scale factor change in  part per million/year
#                 $iers           If true then parameters are formatted using IERS conventions
#                                 (Units are 1/1000 default (mm,mas,ppb) and rotations have opposite sign)
#
#   Returns:      
#
#===============================================================================

sub new {
   my( $class, $tx, $ty, $tz, $rx, $ry, $rz, $ppm,
               $refy, 
               $dtx, $dty, $dtz, $drx, $dry, $drz, $dppm,
               $iers ) = @_;

   $refy=2000.0 if ! $refy;
   if( $iers )
   {
       foreach ($tx, $ty, $tz, $rx, $ry, $rz, $ppm, $dtx, $dty, $dtz, $drx, $dry, $drz, $dppm )
       {
           $_ /= 1000.0;
       }
       foreach ($rx, $ry, $rz, $drx, $dry, $drz )
       {
           $_ *= -1;
       }

   }

  # Check if we have time dependent components

  my $needepoch=(
      $dtx != 0.0 || $dty != 0.0 || $dtz != 0.0 ||
      $drx != 0.0 || $dry != 0.0 || $drz != 0.0 ||
      $dppm != 0.0);

  # Construct the object.
  my $self = { Txyz=>[$tx, $ty, $tz],
               Rxyz=>[$rx, $ry, $rz],
               SF=>$ppm,
               dTxyz=>[$dtx, $dty, $dtz],
               dRxyz=>[$drx, $dry, $drz],
               dSF=>$dppm,
               year0=>$refy,
               iers=>$iers,
               needepoch=>$needepoch,
               setepoch=>0.0
             };
  return bless $self, $class;
  }


#===============================================================================
#
#   Method:       SetEpoch
#
#   Description:  $bw->SetEpoch($epoch)
#                 Prepares the transformation parameters that apply at a 
#                 specific epoch.
#
#   Parameters:   $epoch   The epoch for the transformation parameters (years)
#
#   Returns:      
#
#===============================================================================

sub SetEpoch
{
    my ($self,$epoch) = @_;
    return if $self->{setepoch} && $epoch == $self->{setepoch};
    if( $self->{needepoch} || ! $self->{setepoch} )
    {
       $epoch = $epoch || $self->{year0};
       my $dy = $epoch - $self->{year0};
       my $tx = $self->{Txyz}->[0]+$dy*$self->{dTxyz}->[0];
       my $ty = $self->{Txyz}->[1]+$dy*$self->{dTxyz}->[1];
       my $tz = $self->{Txyz}->[2]+$dy*$self->{dTxyz}->[2];
       my $rx = $self->{Rxyz}->[0]+$dy*$self->{dRxyz}->[0];
       my $ry = $self->{Rxyz}->[1]+$dy*$self->{dRxyz}->[1];
       my $rz = $self->{Rxyz}->[2]+$dy*$self->{dRxyz}->[2];
       my $xlat=[$tx,$ty,$tz];
       my $sf = 1.0+($self->{SF}+$dy*$self->{dSF})*1.0e-6;
       my $R;
       if( $rx || $ry || $rz ) {
         my $cx = $rx*$sec2rad; 
         my $sx = sin($cx);
         $cx = cos($cx);
         my $cy = $ry*$sec2rad; 
         my $sy = sin($cy);
         $cy = cos($cy);
         my $cz = $rz*$sec2rad; 
         my $sz = sin($cz);
         $cz = cos($cz);
         $R = [ [ $cz*$cy,   $cz*$sx*$sy+$sz*$cx,   $sx*$sz-$cz*$cx*$sy ],
                [ -$sz*$cy,  $cz*$cx-$sz*$sx*$sy,   $sz*$cx*$sy+$cz*$sx],
                [ $sy,      -$sx*$cy,               $cx*$cy             ]];
         }
       $self->{xlat}=$xlat;
       $self->{R}=$R;
       $self->{sf}=$sf;
    }
    $self->{setepoch} = $epoch;
}

#===============================================================================
#
#   Method:       ApplyTo
#
#   Description:  $crd2 = $bw->ApplyTo($crd)
#                 Applies the Bursa-Wolf transformation to geocentric
#                 (XYZ) coordinates.
#
#   Parameters:   $crd     The coordinates to transform
#
#   Returns:               The converted coordinates as a CartesianCrd
#                          object.
#
#===============================================================================

sub ApplyTo {
  my ($self, $crd ) = @_;
  die "Transformation needs a coordinate epoch\n" 
     if $self->{needepoch} && ! $crd->[4];
  $self->SetEpoch($crd->[4]);
  my $x = $crd->[0];
  my $y = $crd->[1];
  my $z = $crd->[2];
  my $sf = $self->{sf};
  my $R = $self->{R};
  if( $R ) {
    ($x, $y, $z ) = ( $R->[0]->[0]*$x + $R->[0]->[1]*$y + $R->[0]->[2]*$z,
                      $R->[1]->[0]*$x + $R->[1]->[1]*$y + $R->[1]->[2]*$z,
                      $R->[2]->[0]*$x + $R->[2]->[1]*$y + $R->[2]->[2]*$z );
    }
  my $xlat = $self->{xlat};

  return new Geodetic::CartesianCrd ( $xlat->[0] + $sf * $x,
                                       $xlat->[1] + $sf * $y,
                                       $xlat->[2] + $sf * $z,
                                       undef, $crd->[4] );

  }
   


#===============================================================================
#
#   Method:       ApplyInverseTo
#
#   Description:  $bw->ApplyInverseTo($crd)
#                 Applies the inverse of the Bursa-Wolf transformation
#                 to a geocentric (XYZ) coordinate.
#
#   Parameters:   $crd    The coordinate to convert.       
#
#   Returns:              The converted coordinate.
#
#===============================================================================

sub ApplyInverseTo {
  my ($self, $crd ) = @_;
  die "Transformation needs a coordinate epoch\n" 
     if $self->{needepoch} && ! $crd->[4];
  $self->SetEpoch($crd->[4]);
  my $x = $crd->[0];
  my $y = $crd->[1];
  my $z = $crd->[2];
  my $sf = $self->{sf};
  my $xlat = $self->{xlat};

  ($x, $y, $z) = ( ($x - $xlat->[0])/$sf,
                   ($y - $xlat->[1])/$sf,
                   ($z - $xlat->[2])/$sf );
  my $R = $self->{R};
  if( $R ) {
    ($x, $y, $z ) = ( $R->[0]->[0]*$x + $R->[1]->[0]*$y + $R->[2]->[0]*$z,
                      $R->[0]->[1]*$x + $R->[1]->[1]*$y + $R->[2]->[1]*$z,
                      $R->[0]->[2]*$x + $R->[1]->[2]*$y + $R->[2]->[2]*$z );
    }

  return new Geodetic::CartesianCrd ( $x, $y, $z, undef, $crd->[4] );
  }

1;
