#!/usr/local/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Script converts a binary grid file to an ASCII 
#                      representation of the data.  The corresponding script
#                      makegrid.pl can convert the file back to a binary
#                      grid file.
#
# PARAMETERS:          grid_file  The name of the grid file to dump
#                      dump_file  The name of the file created (default is
#                                 to dump to standard output)
#                      -h         Optional switch - if present only header data
#                                 is listed.
#                      -i         Values are represented as integers rather than
#                                 double values
#                      
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook          22/06/1999  Created
#===============================================================================

# Read the optional switch

use Getopt::Std;

getopts("iIhH",\%opts);
$header_only = $opts{'h'} || $opts{'H'};
$integer_values = $opts{'i'} || $opts{'I'};

# Test that the file name is present (ie at least one remaining argument)

@ARGV || die <<EOD;
Parameters: [-h] [-i] grid_file dump_file

-h specifies dump header only
-i specifies values are dumped as integers

EOD

# Test the endian-ness of the host computer

$bigendian = pack("n",1) eq pack("s",1);

# Each possible grid file format has a specified header record.  Valid records
# are listed here.  The %formats hash converts these to a format definition
# string - one of GEOID (old SNAP geoid format), GRID1L (little endian version
# 1 grid format), GRID1B (big endian version 1 grid format).

$siggeoid = "SNAP geoid binary file\r\n\x1A";
$siggrid1l = "SNAP grid binary v1.0 \r\n\x1A";
$siggrid1b = "CRS grid binary v1.0  \r\n\x1A";
$siglen = length($siggeoid);

%formats = (
    $siggeoid => 'GEOID',
    $siggrid1l => 'GRID1L',
    $siggrid1b => 'GRID1B' );

# Open the  grid file in binary mode.

open(GRD,$ARGV[0]) || die;
binmode(GRD);

# Open the output file .. if none is specified use "-" to access
# standard output.

$outfile = $ARGV[1];
$outfile = '-' if ! $outfile;
open(DEF,">$outfile") || die "Cannot open output file $outfile\n";

# Read the grid file signature and ensure that it is valid

read(GRD,$testsig,$siglen);

$fmt = $formats{$testsig};

die "Not a valid grid file - file signature incorrect\n" if ! $fmt;

# Set up the parameters required to handle endian-ness.  
# $swapbytes specifies the file endian-ness is opposite that of the
# host (ie bytes must be reversed for numeric types).  $shortcode and
# $longcode are the pack/unpack codes for unsigned short and long integers.

$filebigendian = $fmt eq 'GRID1B';

if( $filebigendian ) {
   $swapbytes = ! $bigendian;
   $longcode = 'N';
   $shortcode = 'n';
   }
else {
   $swapbytes = $bigendian;
   $longcode = 'V';
   $shortcode = 'v';
   }

# Read location in the file of the grid index data

read(GRD,$buffer,4);
($loc)=unpack($longcode,$buffer);
die "Grid file not completed\n" if ! $loc;

# Jump to the index and  read the grid data

seek(GRD,$loc,0);

$ymn = &ReadDouble;
$ymx = &ReadDouble;
$xmn = &ReadDouble;
$xmx = &ReadDouble;
$vres = &ReadDouble;

read(GRD,$buffer,4);
($ny,$nx)=unpack($shortcode."2",$buffer);

# If the format is 'GEOID'  then two fields are not in the file, they
# are set to default values.

if( $fmt ne 'GEOID' ) {
   read(GRD,$buffer,4);
   ($dim,$latlon)=unpack($shortcode."2",$buffer);
   }
else {
   $dim = 1;
   $latlon = 1;
   }

# Read the grid descriptive data

$s1 = &ReadString;
$s2 = &ReadString;
$s3 = &ReadString;
$s4 = &ReadString;

# Write the header data

print DEF "FORMAT: $fmt\n";
print DEF "HEADER0: $s1\n";
print DEF "HEADER1: $s2\n";
print DEF "HEADER2: $s3\n";
print DEF "CRDSYS: $s4\n";
print DEF "NGRDX: $nx\n";
print DEF "NGRDY: $ny\n";
print DEF "XMIN: $xmn\n";
print DEF "XMAX: $xmx\n";
print DEF "YMIN: $ymn\n";
print DEF "YMAX: $ymx\n";
print DEF "VRES: $vres\n";
print DEF "NDIM: $dim\n";
print DEF "LATLON: $latlon\n";
print DEF "VALUES: INTEGER\n" if $integer_values;

exit if $header_only;

# Read the file location of each row of data in the file..

read(GRD,$buffer,$ny*4);
(@locs)=unpack($longcode."*",$buffer);

# Calculate the size of a buffer representing an entire row

$rowsize = $nx*$dim*2;

# Read each row in turn

for $row (1..$ny) {
   seek(GRD,$locs[$row-1],0);
   read(GRD,$buffer,$rowsize);
   # Note: reverse and unpack with s rather than use $shortcode,
   # as otherwise we don't correctly translate signed values.
   $buffer = reverse($buffer) if $swapbytes;
   @vals = unpack("s*",$buffer);
   @vals = reverse @vals if $swapbytes;
   $v = 0;
   # For each column, print out the $dim values.
   for $col (1..$nx) {
      print DEF "V$col,$row:";
      for (1..$dim) {
         $valv = $vals[$v];
         if( $valv == 0x7FFF ) {
           print DEF " *";
           }
         else {
           $valv *= $vres if ! $integer_values;
           print DEF " ",$valv;
           }
         $v++;
         }
      print DEF "\n";
      }
   }

close(GRD);


#===============================================================================
#
#   SUBROUTINE:   ReadDouble
#
#   DESCRIPTION:  Reads a double precision value from GRD, taking account
#                 of endian-ness
#
#   PARAMETERS:
#
#   RETURNS:      The value read
#
#   GLOBALS:      GRD          The grid file
#                 $swapbytes   If true then file endian-ness is opposite the
#                              computer.
#
#===============================================================================


sub ReadDouble {
  my $buf;
  read(GRD,$buf,8);
  $buf = reverse $buf if $swapbytes;
  return unpack("d",$buf);
}


#===============================================================================
#
#   SUBROUTINE:   ReadString
#
#   DESCRIPTION:  Reads a string from GRD, where the string consists of a
#                 a length followed by that many bytes.  Null characters within
#                 the string are deleted.
#
#   PARAMETERS:
#
#   RETURNS:      The string read
#
#   GLOBALS:      GRD          The grid file
#                 $shortcode   The unpack code for reading an unsigned short
#                              from the file.
#
#===============================================================================

sub ReadString {
  my ($l, $s);
  read(GRD,$l,2);
  ($l) = unpack($shortcode,$l);
  read(GRD,$s,$l);
  $s =~ s/\0//g;
  return $s;
}


