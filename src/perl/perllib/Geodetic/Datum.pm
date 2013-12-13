#===============================================================================
# Module:             Datum.pm
#
# Description:       Defines a basic datum object.  This is a blessed
#                    hash reference with elements:
#                       name       The datum name
#                       ellipsoid  The ellipsoid object defined for the frame
#                       baseref    The base datum
#                       transfunc  A function object for converting coordinates
#                                  to and from the base datum.
#
#                    The CoordSys module may install additional entries into
#                    this hash, which are
#                       _csgeod    The geodetic (lat/lon) coordinate system
#                                  for the datum
#                       _cscart    The cartesian coordinate system for the
#                                  datum
#                       
#                    Defines packages: 
#                      Geodetic::Datum
#
# Dependencies:      Uses the following modules:   
#
#  $Id: Datum.pm,v 1.3 2000/10/15 18:55:14 ccrook Exp $
#
#  $Log: Datum.pm,v $
#  Revision 1.3  2000/10/15 18:55:14  ccrook
#  Modification to coordsys functions to allow them to use a distortion grid
#  definition for coordinate conversions.
#
#  Revision 1.2  1999/09/26 21:17:21  ccrook
#  Added an id to the datum .. used to test if two datums are the same.
#
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

   
#===============================================================================
#
#   Class:       Geodetic::Datum
#
#   Description: Defines the following routines:
#                  $datum = new Geodetic::Datum(
#                    $name, $ellipsoid, $baseref, $transfunc, $code, $defmodel
#                    );
#
#                  $name = $datum->name
#                  $ellipsoid = $datum->ellipsoid
#
#===============================================================================

package Geodetic::Datum;

my $id;

#===============================================================================
#
#   Method:       new
#
#   Description:  $datum = new Geodetic::Datum(
#                    $name, $ellipsoid, $baseref, $transfunc
#                 )
#
#   Parameters:   $name       The name of the datum
#                 $ellipsoid  The ellipsoid object used by the frame
#                 $baseref    The code for the base datum used for
#                             coordinate conversion between datums
#                             or a datum object to which the 
#                             transformtion applies.
#                 $transfunc  An object providing function ApplyTo and 
#                             ApplyInverseTo, which convert 
#                             cartesian coordinates to and from the 
#                             base datum.
#                 $code       Optional code identifying the datum
#                 $defmodel   Optional deformation transform object for the
#                             datum
#
#   Returns:      $datum   The blessed ref frame hash
#
#===============================================================================

sub new {
  my ($class, $name, $ellipsoid, $baseref, $transfunc, $code, $defmodel, $refepoch )
    = @_;
  my $self = { name=>$name, 
               ellipsoid=>$ellipsoid, 
               baseref=>$baseref,
               transfunc=>$transfunc,
               code=>$code,
               defmodel=>$defmodel,
               refpoch=>$defmodel ? $refepoch : 0,
               id=>$id,
           };
  $id++;
  return bless $self, $class;
  }


sub name { return $_[0]->{name} }
sub code { return $_[0]->{code} }
sub id { return $_[0]->{id} }
sub ellipsoid { return $_[0]->{ellipsoid}}
sub baseref { return $_[0]->{baseref}}
sub transfunc { return $_[0]->{transfunc}}
sub defmodel { return $_[0]->{defmodel} }
sub refepoch { return $_[0]->{refepoch} }

1;
