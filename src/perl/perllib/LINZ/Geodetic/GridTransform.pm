
#===============================================================================
# Module:             GridTransform.pm
#
# Description:       Defines packages: 
#                      LINZ::Geodetic::GridTransform
#                    This implements a grid based transformation to convert
#                    between different datums.  Assumes that the two and
#                    from coordinates are very similar, so that the inverse
#                    transformation can be done by calculating the offset
#                    from the input coordinates and applying in the opposite
#                    sense (with one iteration for safety).  Also assumes that
#                    coordinates to be converted are latitude/longitude
#                    coordinates.
#
# Dependencies:      Uses the following modules: 
#                      LINZ::Geodetic::Util::GridFile
#                      LINZ::Geodetic::GeodeticCrd  
#
#  $Id: GridTransform.pm,v 1.2 2000/10/15 18:55:14 ccrook Exp $
#
#  $Log: GridTransform.pm,v $
#  Revision 1.2  2000/10/15 18:55:14  ccrook
#  Modification to coordsys functions to allow them to use a distortion grid
#  definition for coordinate conversions.
#
#  Revision 1.1  2000/08/29 00:35:20  gdb
#  Implementation of version using SQL data source based on Landonline
#  extract.
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       LINZ::Geodetic::GridTransform
#
#   Description: Defines the following methods:
#                  $bw = new LINZ::Geodetic::GridTransform($gridfile)
#                  $crd2 = $bw->ApplyTo($crd)
#                  $crd2 = $bw->ApplyInverseTo($crd)
#
#===============================================================================

package LINZ::Geodetic::GridTransform;

require LINZ::Geodetic::GeodeticCrd;

my $sec2rad = atan2(1,1)/(45.0*3600.0);


#===============================================================================
#
#   Method:       new
#
#   Description:  $grdtfm = new LINZ::Geodetic::GridTransform($gridfile)
#
#   Parameters:   $gridfile  The name of the grid file defining the
#                            transformation
#
#   Returns:      
#
#===============================================================================

sub new {
  my( $class, $gridfile, $gridtype, $ellipsoid, $bwtfm ) = @_;
  die "Cannot open grid transformation file $gridfile\n" if ! -r $gridfile;;
  die "Invalid grid type $gridtype specified\n" if uc($gridtype) ne 'SNAP2D';
  my $self = { 
          gridfile=>$gridfile,
          gridtype=>$gridtype,
          ellipsoid=>$ellipsoid,
          bwtfm=>$bwtfm
          };
  return bless $self, $class;
  }

#===============================================================================
#
#   Method:       needepoch
#
#   Description:  $grdtfm->needepoch()
#                 Returns true if the conversion needs an epoch
#
#   Returns:      true if an epoch is needed
#
#===============================================================================

sub needepoch
{
    my($self)=@_;
    return $self->{bwtfm}->needepoch();
}


#===============================================================================
#
#   Method:       InstallGrid
#
#   Description:  $grdtfm->InstallGrid
#
#   Parameters:   none
#
#   Returns:      
#
#===============================================================================

sub InstallGrid {
  my ($self) = @_;
  my $gridfile = $self->{gridfile};
  require LINZ::Geodetic::Util::GridFile;
  my $grid = new LINZ::Geodetic::Util::GridFile $gridfile;
  die "Cannot open grid transformation file $gridfile\n" if ! $grid;
  die "Invalid grid transformation file $gridfile\n" 
      if $grid->Dimension < 2;

  $self->{grid} = $grid;
  return $grid;
  }


#===============================================================================
#
#   Method:       ApplyTo
#
#   Description:  $crd2 = $grdtfm->ApplyTo($crd)
#                 Applies the grid transformation to lat/lon coordinates
#                 coordinates.
#
#   Parameters:   $crd     The coordinates to transform
#
#   Returns:               The converted coordinates as a GeodeticCrd
#                          object.
#
#===============================================================================

sub ApplyTo {
  my ($self, $crd ) = @_;
  my $grid = $self->{grid};
  $grid = $self->InstallGrid if ! $grid;
  $crd = $self->{ellipsoid}->geog($crd);
  my ($lat,$lon,$hgt) = @$crd;
  my ($dln,$dlt,$dhgt) = $grid->CalcLinear($lon,$lat);
  $crd->[0] += $dlt;
  $crd->[1] += $dln;
  $crd = $self->{ellipsoid}->xyz($crd);
  $crd = $self->{bwtfm}->ApplyTo($crd);
  return $crd;
  }
   


#===============================================================================
#
#   Method:       ApplyInverseTo
#
#   Description:  $grdtfm->ApplyInverseTo($crd)
#                 Applies the inverse of the Grid transformation
#                 to a lat/lon coordinate.
#
#   Parameters:   $crd    The coordinate to convert.       
#
#   Returns:              The converted coordinate.
#
#===============================================================================

sub ApplyInverseTo {
  my ($self, $crd ) = @_;
  $crd = $self->{bwtfm}->ApplyInverseTo($crd);
  my $grid = $self->{grid};
  $grid = $self->InstallGrid if ! $grid;
  $crd = $self->{ellipsoid}->geog($crd);
  my ($lat,$lon,$hgt) = @$crd;
  my ($dln,$dlt,$dhgt) = $grid->CalcLinear($lon,$lat);
  ($dln,$dlt,$dhgt) = $grid->CalcLinear($lon-$dln,$lat-$dlt);
  $crd->[0] -= $dlt;
  $crd->[1] -= $dln;
  $crd = $self->{ellipsoid}->xyz($crd);
  return $crd;
  }


1;
