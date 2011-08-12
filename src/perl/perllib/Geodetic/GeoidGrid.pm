#===============================================================================
# Module:             GeoidGrid.pm
#
# Description:       Defines packages: 
#                      Geodetic::GeoidGrid
#                    This implements a grid based geoid height conversion
#
# Dependencies:      Uses the following modules: 
#
#  $Id: GeoidGrid.pm,v 1.1 2005/11/27 19:39:29 gdb Exp $
#
#  $Log: GeoidGrid.pm,v $
#  Revision 1.1  2005/11/27 19:39:29  gdb
#  *** empty log message ***
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::GeoidGrid
#
#   Description: Defines the following methods:
#                  $gg = new Geodetic::GeoidGrid($tx,$ty,$tz,$rx,$ry,$rz,$ppm)
#                  $ghgt = $gg->GeoidHeight($crd)
#                  $ohgt = $gg->OrthometricHeight($crd,$ehgt)
#                  $ehgt = $gg->EllToOrth($crd,$oght)
#
#===============================================================================

package Geodetic::GeoidGrid;

use GridFile;


#===============================================================================
#
#   Method:       new
#
#   Description:  $gg = new Geodetic::GeoidGrid($gridname)
#
#   Parameters:   $gridname    The name of the geoid grid file
#
#   Returns:      
#
#===============================================================================

sub new {
  my( $class, $name ) = @_;
  my $grid = new GridFile $name;
  die "Invalid geoid grid $name - wrong data dimension\n" if $grid->Dimension != 1;
  my $cscode = $grid->CrdSysCode;
  my @titles = $grid->Title;
  my $self = { 
        filename=>$name,
        titles=>\@titles,
        required_cscode=>$cscode,
        grid=>$grid,
        };
  return bless $self, $class;
  }


sub filename { return $_[0]->{filename} }
sub titles { return $_[0]->{titles} }
sub required_cscode { return $_[0]->{required_cscode} }

#===============================================================================
#
#   Method:       GeoidHeight
#
#   Description:  $ghgt = $gg->GeoidHeight($crd)
#                 Calculates the geoid height at a point from the grid
#
#   Parameters:   $crd     The coordinates to calculate the height at
#
#   Returns:               The geoid height
#
#===============================================================================

sub GeoidHeight {
  my ($self, $crd ) = @_;
  my $x = $crd->lon;
  my $y = $crd->lat;
  my $cscode = $crd->coordsys->code;
  my $mycscode = $self->required_cscode;
  die "Coordinates for geoid height calculation in $cscode - but need to be $mycscode\n" 
     if $cscode ne $mycscode;
  my $ghgt;
  eval {
     ($ghgt) = $self->{grid}->CalcLinear($x,$y);
     };
  if( $@ ) {
     die "Coordinates out of range for calculating geoid height\n";
     }
  return $ghgt;
  }
   


#===============================================================================
#
#   Method:       EllipsoidalHeight
#
#   Description:  $ehgt = $gg->EllipsoidalHeight( $crd, $ohgt );
#                 Converts the orthometric height to an ellipsoidal height
#
#   Parameters:   $crd    The coordinate of the point to convert.       
#                 $ohgt   The orthometric height
#
#   Returns:              The ellipsoidal height
#
#===============================================================================

sub EllipsoidalHeight {
  my ($self, $crd, $ohgt ) = @_;
  return $ohgt + $self->GeoidHeight( $crd );
  }

#===============================================================================
#
#   Method:       OrthometricHeight
#
#   Description:  $ohgt = $gg->OrthometricHeight( $crd, $ehgt );
#                 Converts the ellipsoidal height to an orthometric height
#
#   Parameters:   $crd    The coordinate of the point to convert.       
#                 $ehgt   The ellipsoidal height
#
#   Returns:              The orthometric height
#
#===============================================================================

sub OrthometricHeight {
  my ($self, $crd, $ehgt ) = @_;
  return $ehgt - $self->GeoidHeight( $crd );
  }

1;
