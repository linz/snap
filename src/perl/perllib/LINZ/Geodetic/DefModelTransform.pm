
#===============================================================================
# Module:             DefModelTransform.pm
#
# Description:       Defines packages:
#                      LINZ::Geodetic::DefModelTransform
#                    This implements a deformation model transformation.
#
# Dependencies:      Uses the following modules:
#                      LINZ::Geodetic::LINZDeformationModel
#                      LINZ::Geodetic::CartesianCrd
#                      LINZ::Geodetic::Util::Vector3
#                      LINZ::Geodetic::Util::RotMat3
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
#   Class:       LINZ::Geodetic::DefModelTransform
#
#   Description: Defines the following methods:
#                  $modelxform = LINZ::Geodetic::DefModelTransform->new($file)
#                  $epoch = $modelxform->RefEpoch
#                  $crd2  = $modelxform->ApplyTo($crd)
#                  $crd2  = $modelxform->ApplyInverseTo($crd)
#                  $modelxform->InstallModel
#
#===============================================================================

package LINZ::Geodetic::DefModelTransform;

our $AUTOLOAD;

#===============================================================================
#
#   Method:       new
#
#   Description:  $modelxform = LINZ::Geodetic::DefModelTransform->new(
#                        $file, $modeltype, $ref_epoch, $ellipsoid, $version
#                    );
#
#   Parameters:   $file       The name of definition model file
#                 $modeltype  The name of the grid file defining the
#                             transformation
#                 $ref_epoch  The definition epoch for deformation model.
#                 $ellipsoid  Parent reference frame ellipsoid object.
#                 $version    The version of deformation model in 
#                             model file to use (default if empty)
#   Returns:
#
#===============================================================================

sub new {
  my( $class, $file, $modeltype, $ref_epoch, $ellipsoid, $version ) = @_;
  die "Cannot open grid transformation file $file\n" if ! -r $file;
  die "Invalid deformation model type $modeltype specified\n"
    if uc($modeltype) ne 'LINZDEF';
  my $self = {
    file      => $file,
    modeltype => $modeltype,
    ellipsoid => $ellipsoid,
    model     => undef,
    ref_epoch => $ref_epoch,
    version   => $version
    };
  return bless $self, $class;
  }

sub AUTOLOAD
{
    my($self)=@_;
    my $function=$AUTOLOAD;
    $function=~ s/.*:://;
    $self->InstallModel() if ! $self->{model};
    return eval "\$self->{model}->$function(\@_)";
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
  my $version = $self->{version};
  require LINZ::Geodetic::Util::RotMat3;
  require LINZ::Geodetic::Util::Vector3;
  require LINZ::Geodetic::CartesianCrd;
  require LINZ::Geodetic::LINZDeformationModel;
  my $model = LINZ::Geodetic::LINZDeformationModel->new($file);
  $model->SetVersion($version) if $version ne '';
  $self->{model} = $model;
  return $model;
  }

#===============================================================================
#
#   Method:       ApplyTo
#
#   Description:  $crd2 = $modelxform->ApplyTo($crd, $epoch)
#                 Applies the deformation to xyz coordinates
#                 coordinates - converts from the reference coordinates
#                 (or the coordinates at a reference epoch if defined)
#                 to the coordinates of the underlying datum.  
#
#   Parameters:   $crd     The coordinates to transform
#                 $epoch   The epoch to propogate the coordinate to.
#
#   Returns:               The converted coordinates as a CartesianCrd
#                          object.
#
#===============================================================================

sub ApplyTo {
  my ($self, $crd) = @_;
  my $crd_epoch = $crd->epoch;
  die "Cannot apply deformation model - coordinate epoch not defined\n"
      if ! $crd_epoch;
  my $ref_epoch = $self->RefEpoch;
  my $result = bless [@$crd], ref($crd);
  my $denu =  $self->CalcDef($result,$ref_epoch,$crd_epoch);
  $result->[0] += $denu->[0];
  $result->[1] += $denu->[1];
  $result->[2] += $denu->[2];
  return $result;
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
  my ($self, $crd) = @_;
  my $crd_epoch = $crd->epoch;
  die "Cannot apply deformation model - coordinate epoch not defined\n"
      if ! $crd_epoch;
  my $ref_epoch = $self->RefEpoch;
  my $result = bless [@$crd], ref($crd);
  my $denu =  $self->CalcDef($result,$crd_epoch,$ref_epoch);
  $result->[0] += $denu->[0];
  $result->[1] += $denu->[1];
  $result->[2] += $denu->[2];
  return $result;
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
#   Returns:              A LINZ::Geodetic::Util::Vector3 object which contains the geocentric
#                         deformation.
#
#===============================================================================

sub CalcDef {
  my ($self, $crd, $src_epoch, $tgt_epoch) = @_;
  my $model = $self->{model};
  my $denu=[0,0,0];
  return $denu if $src_epoch == $tgt_epoch;

  $model = $self->InstallModel if ! $model;
  my $geog = $self->{ellipsoid}->geog($crd);
  my $lon=$geog->lon;
  my $lat=$geog->lat;

  if( $src_epoch )
  {
    my ($e, $n, $u) = $model->Calc($src_epoch, $lon, $lat);
    # printf "DefModel %.2f %.5f %.5f: %.4f %.4f %.4f\n",$src_epoch,$lon,$lat,$e,$n,$u;
    $denu->[0] -= $e;
    $denu->[1] -= $n;
    $denu->[2] -= $u;
  }
  if( $tgt_epoch )
  {
    my ($e, $n, $u) = $model->Calc($tgt_epoch, $lon, $lat);
    # printf "DefModel %.2f %.5f %.5f: %.4f %.4f %.4f\n",$tgt_epoch,$lon,$lat,$e,$n,$u;
    $denu->[0] += $e;
    $denu->[1] += $n;
    $denu->[2] += $u;
  }
  $denu = LINZ::Geodetic::Util::Vector3->new( @$denu );
  my $rtopo = LINZ::Geodetic::Util::RotMat3->new($lon, $lat);
  $denu=$rtopo->ApplyInverseTo($denu);
  return $denu;
  }

1;
