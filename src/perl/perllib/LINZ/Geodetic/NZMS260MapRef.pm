# Working version of NZMS260MapRef
#
# This version is for immediate convenience pending a better implementation
# of coordinate representation.  This provides the functions for converting
# between NZMS260 map references and NZMG coordinates.
#
# $Id: NZMS260MapRef.pm,v 1.4 2006/04/17 21:34:02 gdb Exp $
#
# $Log: NZMS260MapRef.pm,v $
# Revision 1.4  2006/04/17 21:34:02  gdb
# Implementation of 6 digit map references rather than non-standard 8 digit references
#
# Revision 1.3  2005/11/27 19:39:30  gdb
# *** empty log message ***
#
# Revision 1.1  2001/05/22 21:09:03  gdb
# Added functions for calculating map references
#
#

use strict;

package LINZ::Geodetic::NZMS260MapRef;

sub sheet {
   my( $n, $e ) = @_;
   die "Coordinates of range for NZMS260 map reference\n" if
      $n > 6790000 || $n < 5290000 || $e < 1970000 || $e > 3010000;
   my $ns = int( (6790000-$n)/30000 ); $ns = 50 if $ns > 50;
   my $es = int( ($e-1970000)/40000 ); $es = 25 if $es > 25;
   return chr(ord('A')+$es) . sprintf("%02d",$ns+1)
   }

sub write {
   my( $n, $e ) = @_;
   my $sheet = sheet($n,$e);

   my $nr = substr(int(($n+50)/100),-3);
   my $er = substr(int(($e+50)/100),-3);
   my $ref = $sheet.' '.$er.' '.$nr;
   return ($ref);
   };

sub read {
   my( $ref ) = @_;
   $ref = uc($ref);
   $ref =~ /^([A-Z])([0-4]\d|50)\W+(\d\d\d)\W*(\d\d\d)$/
     || $ref =~ /^([A-Z])([0-4]\d|50)\W+(\d\d\d\d)\W*(\d\d\d\d)$/
     || die "Invalid map reference $ref\n";
   my( $maplet, $mapnum, $er, $nr ) = ($1,$2,$3,$4);
   if( length($er) == 3 ) { $er *= 10; $nr *= 10; };
   $er *= 10; $nr *= 10;
   my $n0 = 6790000-30000*$mapnum+15000;
   my $e0 = 1970000+40000*(ord($maplet)-ord('A'))+20000;

   $nr = int(($n0 - $nr)/100000+0.5)*100000 + $nr;
   $er = int(($e0 - $er)/100000+0.5)*100000 + $er;
   return ($nr, $er);
   }

1;
