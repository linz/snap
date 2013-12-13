#===============================================================================
# Module:             DMS.pm
#
# Description:       Defines packages: 
#                      Geodetic::DMS
#
# Dependencies:      Uses the following modules:   
#
#  $Id: DMS.pm,v 1.4 2002/02/21 21:03:25 gdb Exp $
#
#  $Log: DMS.pm,v $
#  Revision 1.4  2002/02/21 21:03:25  gdb
#  Fixed error in rounding decimal places of a second...
#
#  Revision 1.3  2000/10/24 02:35:50  ccrook
#  Fixed ReadDMS function to complain about minutes or seconds greater than 60.
#
#  Revision 1.2  2000/10/24 02:34:15  ccrook
#  Added ReadDMS function
#
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Package:     Geodetic::DMS
#
#   Description: Package defines a function for converting an angle to a 
#                string in degrees, minutes, seconds format.
#
#                Defines the following routines:
#                  $result = &Geodetic::DMS::FormatDMS($value, $ndp, $hem)
#                  $result = &Geodetic::DMS::ReadDMS($string, $hem)
#
#===============================================================================

package Geodetic::DMS;


#===============================================================================
#
#   Subroutine:   FormatDMS
#
#   Description:  $result = &Geodetic::DMS::FormatDMS($value, $ndp, $hem)
#
#   Parameters:   $value      The value to format (in degrees)
#                 $ndp        The number of decimal places of seconds to show
#                 $hem        The hemisphere indicators characters. A two 
#                             character string of which the first is used
#                             for negative angles (eg 'SN')
#
#   Returns:      The formatted string
#
#===============================================================================

sub FormatDMS {
   my ($value,$ndp,$hem) = @_;
   $hem = substr($hem,($value < 0 ? 0 : 1), 1);
   $value = abs($value);
   # Offset value to avoid rounding seconds up to 60 (ie so that we don't
   # get results like 1 59 60.0 instead of 2 00 00.0)
   my $offset=1/(2*10**$ndp);
   $value += $offset/3600.0;
   my $deg = int($value);
   $value = ($value-$deg)*60;
   my $min = int($value);
   $value = ($value-$min)*60;
   $value = abs($value - $offset);
   my $ndp3 = $ndp + ($ndp ? 3 : 2 );
   return sprintf("%d %02d %0$ndp3.$ndp"."f %s", $deg,$min,$value,$hem);
   }

#===============================================================================
#
#   Subroutine:   ReadDMS
#
#   Description:  Reads an angle in degrees/minutes/seconds format.  The angle
#                 can be preceded or followed by a hemisphere indicator, and
#                 can have just degrees, degrees and minutes, or degrees,
#                 minutes, and seconds.
#
#                 $result = &Geodetic::DMS::ReadDMS($string, $hem)
#
#   Parameters:   $string     The angle expressed as a string
#                 $hem        The hemisphere indicators characters. A two 
#                             character string of which the first is used
#                             for negative angles (eg 'SN')
#
#   Returns:      The angle in degrees
#
#===============================================================================

sub ReadDMS {
   my ($string,$hem) = @_;
   $hem = uc($hem);
   my ($deg,$min,$sec,$hm);
   if( $string =~ /^\s*([$hem])\s*(\d+)(?:\s+(\d+)(?:(\s+\d+\.?\d*))?)?\s*$/i ) {
        ($hm,$deg,$min,$sec) = ($1,$2,$3,$4);
         }
   elsif ( $string =~ /^\s*(\d+)(?:\s+(\d+)(?:(\s+\d+\.?\d*))?)?\s*([$hem])\s*$/i ) {
        ($deg,$min,$sec,$hm) = ($1,$2,$3,$4);
         }
   else {
         die "Invalid angle $string\n";
         }
   die "Invalid minutes in angle\n" if $min > 59;
   die "Invalid seconds in angle\n" if $sec > 60.0001;
   my $value = $deg+$min/60.0+$sec/3600.0;
   $value = -$value if uc(substr($hm,0,1)) eq substr($hem,0,1);
   return $value;
   }


#===============================================================================
#
#   Subroutine:   FormatDM
#
#   Description:  $result = &Geodetic::DMS::FormatDM($value, $ndp, $hem)
#
#   Parameters:   $value      The value to format (in degrees)
#                 $ndp        The number of decimal places of minutes to show
#                 $hem        The hemisphere indicators characters. A two 
#                             character string of which the first is used
#                             for negative angles (eg 'SN')
#
#   Returns:      The formatted string
#
#===============================================================================

sub FormatDM {
   my ($value,$ndp,$hem) = @_;
   $hem = substr($hem,($value < 0 ? 0 : 1), 1);
   $value = abs($value);
   # Offset value to avoid rounding seconds up to 60 (ie so that we don't
   # get results like 1 60.0 instead of 2 00.0)
   $value += 1/(120 * 10**$ndp);
   my $deg = int($value);
   $value = abs(($value-$deg)*60 - (1/(2*10**$ndp))); 
   my $ndp3 = $ndp + ($ndp ? 3 : 2 );
   return sprintf("%d %0$ndp3.$ndp"."f %s", $deg,$value,$hem);
   }

#===============================================================================
#
#   Subroutine:   ReadDM
#
#   Description:  Reads an angle in degrees/minutes format.  The angle
#                 can be preceded or followed by a hemisphere indicator, and
#                 can have just degrees, degrees and minutes, or degrees,
#                 minutes, and seconds.
#
#                 $result = &Geodetic::DMS::ReadDM($string, $hem)
#
#   Parameters:   $string     The angle expressed as a string
#                 $hem        The hemisphere indicators characters. A two 
#                             character string of which the first is used
#                             for negative angles (eg 'SN')
#
#   Returns:      The angle in degrees
#
#===============================================================================

sub ReadDM {
   my ($string,$hem) = @_;
   $hem = uc($hem);
   my ($deg,$min,$hm);
   if( $string =~ /^\s*([$hem])\s*(\d+)(?:(\s+\d+\.?\d*))?\s*$/i ) {
        ($hm,$deg,$min) = ($1,$2,$3);
         }
   elsif ( $string =~ /^\s*(\d+)(?:(\s+\d+\.?\d*))?\s*([$hem])\s*$/i ) {
        ($deg,$min,$hm) = ($1,$2,$3);
         }
   else {
         die "Invalid angle $string\n";
         }
   die "Invalid minutes in angle\n" if $min > 60.0001;
   my $value = $deg+$min/60.0;
   $value = -$value if uc(substr($hm,0,1)) eq substr($hem,0,1);
   return $value;
   }

1;

