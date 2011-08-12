
use strict;

package GPS;

use Time::JulianDay;

sub WeekToDate {
   my( $gw, $gs ) = @_;
   my( $yr,$mo,$dy,$hr,$mn,$sc);
   $mn = int($gs/60);
   $sc = $gs-$mn*60;
   $hr = int($mn/60);
   $mn -= $hr * 60;
   $dy = int($hr/24);
   $hr -= $dy*24;
   $dy += $gw*7+2444245;
   ($yr,$mo,$dy)=inverse_julian_day($dy);
   return sprintf("%d/%02d/%04d %02d:%02d:%05.2f",$dy,$mo,$yr,$hr,$mn,$sc);
   }

sub DateToDayNumber {
   my ($date) = @_;
   my ($day,$month,$year) = $date =~ /(\d+)/g;
   my $dayno = julian_day($year,$month,$day)-julian_day($year,1,1)+1;
   return sprintf("%d.%03d",$year,$dayno);
   }

1;
