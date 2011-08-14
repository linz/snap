#!/usr/local/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Script converts an ASCII representation of grid data
#                      to a binary grid file which can be loaded into CRS for
#                      use by the dbl4_utl_grid.c routines.
#
# PARAMETERS:          def_file   The name of the file defining the grid
#                      grd_file   The name of the binary grid file generated.
#                      
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook          22/06/1999  Created
#===============================================================================

# Script to build a grid binary file from a file of "x y dx dy"
# The file must be organised with a regular rectangular grid of points aligned with the
# x and y axes, and ordered as shown below
#
#    ....
#    n+1 n+2 n+3 ... 2n
#    1    2   3  ... n
#
#  The definition file consists of a set of header records followed by the 
#  data records.  Each record starts with an identifier, then a colon, then
#  some data.  The header records may be in any order.  The data records 
#  must be ordered as specified above.  The records are:
#
#  FORMAT:    One of GRID1B (preferred for CRS), GRID1L, or GEOID
#  HEADER0:   A string of descriptive information
#  HEADER1:   A string of descriptive information
#  HEADER2:   A string of descriptive information
#  CRDSYS:    The coordinate system
#  NGRDX: 61           The number of x values in each row
#  NGRDY: 82           The number of y values (rows)
#  XMIN: 1900000       The minimum x value
#  XMAX: 3100000       The maximum x value
#  YMIN: 5240000       The minimum y value
#  YMAX: 6860000       The maximum y value
#  VRES: 0.000250625   The factor by which grid values are scaled
#  NDIM: 2             The number of values at each grid point
#  LATLON: 0           1 if the grid uses latitude/longitude ordinates
#  VALUES: REAL|INTEGER  If integer then the values are already scaled
#  V1,1: 0 0           The grid data...
#  V2,1: 0 0
#  V3,1: 0 0
#  


# use strict;
use Getopt::Std;
use FindBin;
use lib $FindBin::Bin.'/perllib';
use Packer;

my %opts;
getopts('f:',\%opts);
my $forcefmt = $opts{f};

@ARGV==2 || die "Syntax: [-f format] grid_def_file grid_file\n";

# Required header information

@reqheader = qw/FORMAT HEADER0 HEADER1 HEADER2 CRDSYS NGRDX NGRDY XMIN XMAX YMIN YMAX VRES NDIM VALUES/;

# Defaulted headers

%headers = (NDIM=>1, LATLON=>1, HEADER1=>'', HEADER2=>'', VALUES=>'REAL');

# File signature strings for each format

$siggeoid = "SNAP geoid binary file\r\n\x1A";
$siggrid1l = "SNAP grid binary v1.0 \r\n\x1A";
$siggrid1b = "CRS grid binary v1.0  \r\n\x1A";

%sigs = (
    GEOID=>$siggeoid,
    GRID1L=>$siggrid1l,
    GRID1B=>$siggrid1b );

# Open the input and output file.  The output grid file is in binary mode

open(DEF,$ARGV[0]) || die "Cannot open grid definition file $ARGV[0]\n";
open(GRD,">$ARGV[1]")|| die "Cannot open output grid file $ARGV[1]\n";
binmode(GRD);

# Process the input file until a data record (V#,#:) is reached
# Load into %headers

while($inrec = <DEF>){
   last if $inrec =~ /^\s*v\d+\,\d+\:/i;
   chomp($inrec);
   $headers{uc($1)} = $2 if $inrec =~ /^\s*(\w+)\s*\:\s*(.*?)\s*$/;
   }

# Test for missing header records

@missing = grep {! defined($headers{$_})} @reqheader;
die "Definition file missing records ",join(' ',@missing),"\n" if @missing;

# Extract the oft used values to scalars

($ngrdx, $ngrdy, $vres, $ndim) = @headers{ 'NGRDX', 'NGRDY', 'VRES', 'NDIM' };
$fmt = $forcefmt || $headers{FORMAT};

# Test for invalid input data

die "Invalid format $headers{FORMAT}\n" if ! $sigs{$fmt};
die "Invalid CRDSYS definition $headers{CRDSYS}\n" if $headers{CRDSYS} !~ /^\w+$/;
die "Invalid row or column count\n" if $ngrdx < 1 || $ngrdy < 1;
die "Invalid resolution\n" if $vres <= 0.0;
die "Invalid grid element dimension\n" if $ndim < 1;

# Set up the parameters required to handle endian-ness.  

$filebigendian = $fmt eq 'GRID1B';
my $packer = new Packer($filebigendian);

# Print the grid file header and space for a pointer to the index..

#print  "Creating the grid file $ARGV[1] ... \n";

print GRD $sigs{$fmt};

$indexptrloc = tell(GRD);
print GRD $packer->long(0);

# Write each row...

$integer_values = $headers{VALUES}=~/^int/i;

$vmax = 0x7FFE;
$vmax *= $vres if ! $integer_values;
$vmin = -$vmax;

for $col ( 1 .. $ngrdy ) {
   $rowloc[$col-1] = tell(GRD);
   $buffer = '';
   for $row ( 1 .. $ngrdx ) {
      die "Invalid input data at row $row col $col\n" if 
          $inrec !~ /^\s*v(\d+)\,(\d+)\:\s+/i || $1 ne $row || $2 ne $col;
      @data = split(' ',$');
      die "Missing or extra data at row $row col $col\n" if @data != $ndim;
      foreach $val (@data) {
         if( $val eq '*' ) {
             $data = $packer->short(0x7FFF);
             }
         else {
            die "Data out of range at row $row col $col\n" 
               if $val < $vmin || $val > $vmax;
            # Crude rounding .. but it works
            $val = sprintf("%d",$val/$vres) if ! $integer_values;
            $data = $packer->short($val);
            }
         $buffer .= $data;
         }
      $inrec = <DEF>;
      chomp $inrec;
      }
   print GRD $buffer;
   }

# Note the location of the index

$indexloc = tell(GRD);

# Write the grid index data

print GRD $packer->double( @headers{'YMIN','YMAX','XMIN','XMAX'},$vres );
print GRD $packer->short($ngrdy,$ngrdx );
print GRD $packer->short($ndim,$headers{LATLON} ) if $fmt ne 'GEOID';
print GRD $packer->string(@headers{'HEADER0','HEADER1','HEADER2','CRDSYS'});
print GRD $packer->long(@rowloc);

# Jump back to the index pointer location and write a pointer to the index

seek(GRD,$indexptrloc,0);
print GRD $packer->long($indexloc);
close(GRD);

