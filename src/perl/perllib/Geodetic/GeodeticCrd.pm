#===============================================================================
# Module:             GeodeticCrd.pm
#
# Description:       Defines packages: 
#                      Geodetic::GeodeticCrd
#
# Dependencies:      Uses the following modules: 
#                      Geodetic
#                      Geodetic::Coordinate
#                      Geodetic::DMS  
#
#  $Id: GeodeticCrd.pm,v 1.2 2000/08/29 00:35:20 gdb Exp $
#
#  $Log: GeodeticCrd.pm,v $
#  Revision 1.2  2000/08/29 00:35:20  gdb
#  Implementation of version using SQL data source based on Landonline
#  extract.
#
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::GeodeticCrd
#
#   Description: Defines the following routines:
#                  $llh = new Geodetic::GeodeticCrd( $lat, $lon, $hgt, $cs, $epoch );
#                  $llh = new Geodetic::GeodeticCrd( [$lat, $lon, $hgt, $cs, $epoch] );
#
#                  $lat = $llh->lat
#                  $lon = $llh->lon
#                  $hgt = $llh->hgt
#
#                  $type = $llh->type
#
#                  $strarray = $llh->asstring
#
#===============================================================================

package Geodetic::GeodeticCrd;

require Geodetic;
require Geodetic::Coordinate;

use vars qw/@ISA/;
@ISA=('Geodetic::Coordinate');


sub lat { $_[0]->[0]; }
sub latitude { $_[0]->[0]; }

sub lon { $_[0]->[1]; }
sub longitude { $_[0]->[1]; }

sub hgt { $_[0]->[2]; }

#===============================================================================
#
#   Subroutine:   type
#
#   Description:  Returns the type id of the coordinate, always
#                 &Geodetic::GEODETIC
#                   $type = $llh->type
#
#   Parameters:   None
#
#   Returns:      The type id
#
#===============================================================================

sub type { return &Geodetic::GEODETIC; }


#===============================================================================
#
#   Method:       asstring
#
#   Description:  Creates a string representation of the coordinate in
#                 degrees, minutes, and seconds format.
#                     $strarray = $llh->asstring($ndp, $ndpv)
#
#   Parameters:   $ndp        The number of decimal places of seconds (default 
#                             5)
#                 $ndpv       The number of decimal places of metres for the
#                             height ordinate (default 3)
#
#   Returns:      $strarray   An array reference to an array of strings for
#                             the latitude, longitude, and height
#
#===============================================================================

sub asstring {
   my( $self, $ndp, $ndpv ) = @_;
   $ndp = 5 if $ndp !~ /^\d$/;
   $ndp += 0;
   $ndp = 0 if $ndp < 0;
   $ndp = 9 if $ndp > 9;
   $ndpv = $ndp-3 if ! $ndpv !~ /^\$d$/;
   $ndpv = 0 if $ndpv < 0;
   $ndpv = 9 if $ndpv > 9;
   my ($lat, $lon, $hgt);
   if( $self->[0] ne '' && $self->[1] ne '' ) {
      require Geodetic::DMS;
      $lat = &Geodetic::DMS::FormatDMS( $self->[0], $ndp, 'SN' );
      $lon = &Geodetic::DMS::FormatDMS( $self->[1], $ndp, 'WE' );
      }
   if( $self->[2] ne '' ) {
      $hgt = sprintf("%.$ndpv"."f",$self->[2]);
       }
   my $result = [$lat,$lon,$hgt];
   return wantarray ? @$result : $result;
   } 
1;
