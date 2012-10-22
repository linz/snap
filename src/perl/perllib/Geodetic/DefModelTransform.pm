
#===============================================================================
# Module:             DefModelTransform.pm
#
# Description:       Defines packages:
#                      Geodetic::DefModelTransform
#                    This implements a deformation model transformation.
#
# Dependencies:      Uses the following modules:
#                      LINZDeformationModel
#                      Geodetic::CartesianCrd
#                      Vector3
#                      RotMat3
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
#   Class:       Geodetic::DefModelTransform
#
#   Description: Defines the following methods:
#                  $modelxform = Geodetic::DefModelTransform->new($file)
#                  $epoch = $modelxform->RefEpoch
#                  $crd2  = $modelxform->ApplyTo($crd)
#                  $crd2  = $modelxform->ApplyInverseTo($crd)
#                  $modelxform->InstallModel
#
#===============================================================================

package Geodetic::DefModelTransform;

#===============================================================================
#
#   Method:       new
#
#   Description:  $modelxform = Geodetic::DefModelTransform->new(
#                        $file, $modeltype, $ref_epoch, $ellipsoid
#                    );
#
#   Parameters:   $file       The name of definition model file
#                 $modeltype  The name of the grid file defining the
#                             transformation
#                 $ref_epoch  The definition epoch for deformation model.
#                 $ellipsoid  Parent reference frame ellipsoid object.
#   Returns:
#
#===============================================================================

sub new {
  my( $class, $file, $modeltype, $ref_epoch, $ellipsoid ) = @_;
  die "Cannot open grid transformation file $file\n" if ! -r $file;
  die "Invalid deformation model type $modeltype specified\n"
    if uc($modeltype) ne 'LINZDM';
  my $self = {
    file      => $file,
    modeltype => $modeltype,
    ellipsoid => $ellipsoid,
    model     => undef,
    ref_epoch => $ref_epoch
    };
  return bless $self, $class;
  }


#===============================================================================
#
#   Method:       InstallModel
#
#   Description:  $modelxform->InstallModel
#
#   Parameters:   none
#
#   Returns:
#
#===============================================================================

sub InstallModel {
  my ($self) = @_;
  my $file = $self->{file};
  require RotMat3;
  require Vector3;
  require Geodetic::CartesianCrd;
  require LINZDeformationModel;
  my $model = LINZDeformationModel->new($file);
  $self->{model} = $model;
  return $model;
  }

#===============================================================================
#
#   Method:       ApplyTo
#
#   Description:  $crd2 = $modelxform->ApplyTo($crd, $epoch)
#                 Applies the deformation to xyz coordinates
#                 coordinates.
#
#   Parameters:   $crd     The coordinates to transform
#                 $epoch   The epoch to propogate the coordinate to.
#
#   Returns:               The converted coordinates as a CartesianCrd
#                          object.
#
#===============================================================================

sub ApplyTo {
  my ($self, $crd, $conv_epoch) = @_;
  my $crd_epoch = $crd->epoch;
  return bless [@$crd], ref($crd) unless $crd_epoch - $conv_epoch;
  my $dxyz = $self->CalcDef($crd, $conv_epoch);
  return Geodetic::CartesianCrd->new(
    $crd->[0] - $dxyz->[0],
    $crd->[1] - $dxyz->[1],
    $crd->[2] - $dxyz->[2],
    $crd->[3],
    $crd->[4],
    );
  }


#===============================================================================
#
#   Method:       ApplyInverseTo
#
#   Description:  $modelxform->ApplyInverseTo($crd, $epoch)
#                 Applies the inverse deformation to xyz coordinates
#
#   Parameters:   $crd    The coordinate to convert.
#                 $epoch  The epoch to propogate the coordinate to.
#
#   Returns:              The converted coordinate.
#
#===============================================================================

sub ApplyInverseTo {
  my ($self, $crd, $conv_epoch) = @_;
  my $crd_epoch = $crd->epoch;
  return bless [@$crd], ref($crd) unless $crd_epoch - $conv_epoch;
  my $dxyz = $self->CalcDef($crd, $conv_epoch);
  return Geodetic::CartesianCrd->new(
    $crd->[0] + $dxyz->[0],
    $crd->[1] + $dxyz->[1],
    $crd->[2] + $dxyz->[2],
    $crd->[3],
    $crd->[4],
    );
  }

#===============================================================================
#
#   Method:       RefEpoch
#
#   Description:  $modelxform->RefEpoch
#                 The definition epoch for deformation model.
#
#
#   Returns:      The reference epoch
#
#===============================================================================

sub RefEpoch {
  return $_[0]->{ref_epoch};
  }

#===============================================================================
#
#   Method:       CalcDef
#
#   Description:  $modelxform->CalcDef($crd, $epoch)
#                 Calculate deformation for coordinate.
#
#   Parameters:   $crd    The coordinate to calc deformation for.
#                 $epoch  The target epoch to calculate the deformation to.
#
#   Returns:              A Vector3 object which contains the geocentric
#                         deformation.
#
#===============================================================================

sub CalcDef {
  my ($self, $crd, $conv_epoch) = @_;
  my $model = $self->{model};
  $model = $self->InstallModel if ! $model;
  my $geog = $self->{ellipsoid}->geog($crd);
  my ($e, $n, $u) = $model->Calc($conv_epoch, $geog->lon, $geog->lat);
  my ($de, $dn, $du) = $model->Calc($crd->epoch, $geog->lon, $geog->lat);
  my $denu = Vector3->new( $e-$de, $n-$dn, $u-$du );
  my $rtopo = RotMat3->new($geog->lon, $geog->lat);
  return $rtopo->ApplyInverseTo($denu);
  }

1;
