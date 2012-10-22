
#===============================================================================
# Module:             BursaWolfVelocity.pm
#
# Description:       Defines packages:
#                      Geodetic::BursaWolfVelocity
#                    This implements BursaWolf transformations using 7 parameter
#                    velocities. An BursaWolfVelocity object should be used in
#                    conjunction with parent 7 parameter BursaWolf
#                    transformation.
#
# Dependencies:      Uses the following modules:
#                      Geodetic::BursaWolf
#
#  $Id$
#
#  $Log$
#
#
#===============================================================================

use strict;

#===============================================================================
#
#   Class:       Geodetic::BursaWolfVelocity
#
#   Description: Defines the following methods:
#                  $bwv = new Geodetic::BursaWolfVelocity(
#                    $ref_epoch, $vtx, $vty, $vtz, $vrx, $vry, $vrz, $vppm
#                    );
#                  $crd2 = $bwv->ApplyTo($crd, $epoch)
#                  $crd2 = $bwv->ApplyInverseTo($crd, $epoch)
#
#===============================================================================

package Geodetic::BursaWolfVelocity;

sub REF_EPOCH    () { 0 }
sub TRANSFORM    () { 8 }
sub TRANS_PERIOD () { 9 }

#===============================================================================
#
#   Method:       new
#
#   Description:  $bwv = Geodetic::BursaWolfVelocity->new(
#                   $ref_epoch, $vtx, $vty, $vtz, $vrx, $vry, $vrz, $vppm
#                   );
#
#   Parameters:   $ref_epoch      The definition epoch for Bursa-Wolf velocity
#                                 parameters
#                 $vtx,$vty,$vtz  The X, Y, and Z translation velocity
#                                 components in metres
#                 $vrx,$vry,$vrz  The X, Y, and Z velocity rotations in arc
#                                 seconds
#                 $vppm           The scale factor velocity, part per million.
#
#   Returns:
#
#===============================================================================

sub new {
  my( $class, $ref_epoch, $vtx, $vty, $vtz, $vrx, $vry, $vrz, $vppm ) = @_;
  require Geodetic::BursaWolf;
  my $self
    = [$ref_epoch, $vtx, $vty, $vtz, $vrx, $vry, $vrz, $vppm, undef, undef];
  return bless $self, $class;
  }

#===============================================================================
#
#   Method:       SetupTrans
#
#   Description:  $bwv->SetupTrans($epoch)
#
#   Parameters:   $epoch  The epoch to propogate the velocity parameters to.
#
#   Returns:
#
#===============================================================================

sub SetupTrans {
  my $self   = shift;
  my $period = shift;
  $self->[TRANSFORM] = Geodetic::BursaWolf->new(
    $period * $self->[1],
    $period * $self->[2],
    $period * $self->[3],
    $period * $self->[4],
    $period * $self->[5],
    $period * $self->[6],
    $period * $self->[7],
    );
  $self->[TRANS_PERIOD] = $period;
  }

#===============================================================================
#
#   Method:       ApplyTo
#
#   Description:  $crd2 = $bwv->ApplyTo($crd, $epoch)
#                 Applies the Bursa-Wolf transformation to geocentric
#                 (XYZ) coordinates using propogated velocity parameters
#
#   Parameters:   $crd     The coordinates to transform
#                 $epoch   The epoch to propogate the coordinate to.
#
#   Returns:               The converted coordinates as a GeodeticCrd
#                          object.
#
#===============================================================================

sub ApplyTo {
  my ($self, $crd, $conv_epoch ) = @_;
  my $period = $crd->epoch - $conv_epoch;
  return bless [@$crd], ref($crd) unless $period;
  $self->SetupTrans($period)
    if !$self->[TRANSFORM] || $self->[TRANS_PERIOD] ne $period;
  $crd = $self->[TRANSFORM]->ApplyTo($crd);
  return $crd;
  }

#===============================================================================
#
#   Method:       ApplyInverseTo
#
#   Description:  $crd2 = $bwv->ApplyInverseTo($crd, $epoch)
#                 Applies the inverse Bursa-Wolf transformation to geocentric
#                 (XYZ) coordinates using propogated velocity parameters
#
#   Parameters:   $crd     The coordinates to transform
#                 $epoch   The epoch to propogate the coordinate to.
#
#   Returns:               The converted coordinates as a GeodeticCrd
#                          object.
#
#===============================================================================

sub ApplyInverseTo {
  my ($self, $crd, $conv_epoch) = @_;
  my $period = $conv_epoch - $crd->epoch;
  return bless [@$crd], ref($crd) unless $period;
  $self->SetupTrans($period)
    if !$self->[TRANSFORM] || $self->[TRANS_PERIOD] ne $period;
  $crd = $self->[TRANSFORM]->ApplyInverseTo($crd);
  return $crd;
  }

#===============================================================================
#
#   Method:       RefEpoch
#
#   Description:  $bwv->RefEpoch
#                 The definition epoch for Bursa-Wolf velocity parameters
#
#   Returns:      The reference epoch
#
#===============================================================================

sub RefEpoch {
  return $_[0]->[REF_EPOCH];
  }

1;
