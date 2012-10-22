#===============================================================================
# Module:             ProjectionCrd.pm
#
# Description:       Defines packages: 
#                      Geodetic::ProjectionCrd
#
# Dependencies:      Uses the following modules: 
#                      Geodetic
#                      Geodetic::Coordinate  
#
#  $Id: ProjectionCrd.pm,v 1.2 2000/09/27 11:39:45 gdb Exp $
#
#  $Log: ProjectionCrd.pm,v $
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
#   Class:       Geodetic::ProjectionCrd
#
#   Description: Defines the following routines:
#                  $prj = new Geodetic::ProjectionCrd( $north, $east, $hgt, $cs, $epoch );
#                  $prj = new Geodetic::ProjectionCrd( [$north, $east, $hgt, $cs, $epoch] );
#
#                  $northing = $prj->northing
#                  $easting = $prj->easting
#                  $hgt = $prj->hgt
#
#                  $sf_conv = $prj->sf_conv;
#
#                  $type = $prj->type
#
#                  $strarray = $prj->asstring($ndp, $sep, $ndpv)
#
#===============================================================================

package Geodetic::ProjectionCrd;

require Geodetic;
require Geodetic::Coordinate;

use vars qw/@ISA/;
@ISA=('Geodetic::Coordinate');


sub northing { return $_[0]->[0]; }

sub easting { return $_[0]->[1]; }

sub hgt { return $_[0]->[2]; }

sub type { return &Geodetic::PROJECTION; }

#===============================================================================
#
#   Method:       sf_conv
#
#   Description:  Calculates the scale factor and convergence.
#                 Requires the coordinate to be defined with a coordinate
#                 system.
#                 $sf_conv = $prj->sf_conv;
#
#   Parameters:   
#
#   Returns:      $sf_conv    An array reference to [scale_factor, convergence]
#
#===============================================================================

sub sf_conv { 
   my ($self) = @_;
   my $result;
   eval { $result = $self->coordsys->projection->sf_conv($self); };
   return @$result if $result && wantarray;
   return $result;
   }


#===============================================================================
#
#   Method:       asstring
#
#   Description:  $strarray = $prj->asstring($ndp, $sep, $ndpv)
#
#   Parameters:   $ndp        The number of decimal places (default 3)
#                 $sep        The thousands separator (default none)
#                 $ndpv       The number of decimal places in height 
#                             (default - same as horizontal)
#
#   Returns:      $strarray   An array reference to an array of strings.
#
#===============================================================================

sub asstring {
  my ($self, $ndp, $sep, $ndpv ) = @_;
  $ndpv = $ndp if ! defined($ndpv);

  my ($n, $e, $hgt ) = @$self;

  if( $n ne '' && $e ne '' ) {
    $ndp = 3 if ! defined($ndp);
    $ndp = 0 if $ndp < 0;
    $ndp = 6 if $ndp > 6;
    $ndp = "%.".$ndp."f";
    $n = sprintf($ndp,$n );
    $e = sprintf($ndp,$e );
    }

  if( $hgt ne '' ) {
    $ndpv = 0 if $ndpv < 0;
    $ndpv = 6 if $ndpv > 6;
    $ndpv = "%.".$ndpv."f";
    $hgt = sprintf($ndpv,$hgt);
    }

  if( $sep ) {
     foreach ($e,$n,$hgt) {
       next if $_ eq '';
       $_ = reverse($_);
       s/((\d+\.)?(\d\d\d))(?=\d+[\+\-]?$)/$1$sep/g;
       $_ = reverse($_);
       }
     }
  
  my $result = [$n,$e,$hgt];
  return wantarray ? @$result : $result;
  }


1;
