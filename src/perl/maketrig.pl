#!/usr/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Script converts an ASCII representation of triangulated
#                      data to a binary triangulation file.
#
# PARAMETERS:          def_file   The name of the file defining the grid
#                      grd_file   The name of the binary grid file generated.
#                      
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook           3/02/2004  Created
#===============================================================================
#
# Script to generate a triangle data binary file from a simple definition
# Triangles are defined by a set of numbered points at which data values
# are defined, and a set of triangles defined by the three points that 
# they comprise.
#
# The file has the following headers
#    FORMAT   either TRIG2L (little endian) or TRIG2B (big endian)
#    HEADER0  three text header records (arbitrary content)
#    HEADER1
#    HEADER2
#    CRDSYS   a code for the coordinate system in which the triangluation is
#             defined
#    NDIM     the number of data values at each triangle grid point
#
# The point data is entered as 
#  P id x y v1 v2 ...
#
# The triangle data is entered as 
#  T id1 id2 id3
#

use strict;
use Getopt::Std;
use FindBin;
use lib $FindBin::Bin.'/perllib'; 
use Packer;

use vars qw/$small/;

# Used to identify redundant triangle edges (ie zero length).

$small = 0.0;

my $syntax = <<EOD;

Syntax: [options] input_file output_file

EOD

my %opts;
getopts('cs:f:',\%opts);

my $blocksize = $opts{s};
my $writecrds = $opts{c};
my $forceformat = $opts{f};

@ARGV == 2 || die $syntax;

my ($infile,$outfile) = @ARGV;


my @reqheaders = qw/FORMAT HEADER0 HEADER1 HEADER2 CRDSYS NDIM/;

my %headers;
my %pts;
my @trgs;
my ($xmin, $xmax, $ymin, $ymax);
my $firstpt = 1;
my $ndata = -1;

# Load the data

open(IN,"<$infile") || die "Cannot open triangle input file $infile\n";

my $ok = 1;
while( <IN> ) {
  if( /^\s*P\s+(\d+)\s+(\-?\d+\.?\d*)\s+(\-?\d+\.?\d*)\s+(.*?)\s*$/i ) {
     my($id,$x,$y,$data) = ($1,$2,$3,$4);
     my @data = split(' ',$data);
     if( $ndata < 0 ) { $ndata = scalar(@data); }
     if( $ndata == 0 || $ndata != scalar(@data) ) {
        print STDERR "Missing or inconsistent number of data values for point $id\n";
        $ok = 0;
        }
     if( defined $pts{$id} ) {
        print STDERR "Duplicated point id $id\n";
        $ok = 0;
        next;
        }
     $pts{$id} = {id=>$id, srcid=>$id, crd=>[$x,$y],data=>\@data};
     if( $firstpt ) {
       $xmin = $xmax = $x;
       $ymin = $ymax = $y;
       $firstpt = 0;
       }
     else {
       if( $x > $xmax ) { $xmax = $x; } elsif( $x < $xmin ) { $xmin = $x; }
       if( $y > $ymax ) { $ymax = $y; } elsif( $y < $ymin ) { $ymin = $y; }
       }
     }

  elsif (/^\s*T\s+(\d+)\s+(\d+)\s+(\d+)\s*$/i) {
     my ($p1, $p2, $p3) = ($1,$2,$3);
     for my $p ($p1, $p2, $p3 ) {
       if( ! defined($pts{$p}) ) {
          print STDERR "Point $p referenced in triangle is not defined\n";
          $ok = 0;
          next;
          }
       }
     push(@trgs,{ pts=>[$pts{$p1},$pts{$p2},$pts{$p3}] } );
     }
  elsif ( /^\s*(\S+)\s*(\S.*?)\s*$/ ) {
     $headers{uc($1)} = $2;
     } 
  }

close(IN);

foreach my $header (@reqheaders) {
   next if exists $headers{$header};
   print STDERR "Header record $header missing from file\n";
   $ok = 0;
   }

$headers{CRDSYS} = uc($headers{CRDSYS});
if($headers{CRDSYS} !~ /^\w+$/) {
   print STDERR "Invalid coordinate system definition $headers{CRDSYS}\n";
   $ok = 0;
   }

my $ndim = $headers{NDIM};
if( $ndim != $ndata ) {
   print STDERR "Dimension of data in NDIM header record incompatible with data\n";
   $ok = 0;
   }

my $fmt = $forceformat || uc($headers{FORMAT});
if( $fmt !~ /^TRIG2[BL]$/ ) {
   print STDERR "Format in FORMAT header record not valid - must be TRIG1B or TRIG1L\n";
   $ok = 0;
   }


die "Aborted with errors\n" if ! $ok;

my $bigendian = $fmt eq 'TRIG2B';
my $pack = new Packer( $bigendian );
my $filesig = $bigendian ? "CRS trig binary v2.0  \r\n\x1A" :
                           "SNAP trig binary v2.0 \r\n\x1A";
          
# Sort the points into ascending order of x coordinate, and renumber

my @pts = sort { $a->{crd}->[0] <=> $b->{crd}->[0] } values %pts;
my $pid = 1;
foreach my $pt (@pts) { $pt->{id} = $pid++; }

# Make sure each triangle is ordered in the same direction

foreach my $t (@trgs) { &OrderTriangleNodes($t); }

# Sort the triangles by area from largest to smallest as may have to 
# reverse order of nodes of redundant triangles (with zero area)

@trgs = sort { $b->{area} <=> $a->{area} } @trgs;

# At each nodes form a hash of nodes around it, in which each element identifies
# the opposite node.  Also form a hash of edges, identifying the opposite node on 
# the adjacent edge of the triangle, indexed by the two nodes in reverse order.

my %oppositenode;
foreach my $t (@trgs) {
   my $nodes = $t->{pts};

   # Check whether a node for this direction is already defined, and if so 
   # reverse the triangle..
   for( my($i0,$i1) = (2,0); $i1 < 3; $i0=$i1,$i1++ )
   {
       my $pt0 = $nodes->[$i0];
       my $pt1 = $nodes->[$i1];
       if( exists $pt0->{nextpt}->{$pt1->{id}} ){ ReverseTriangleNodes($t); last; }
   }
 
   # Add the nodes to the triangle list

   for( my($i0,$i1,$i2) = (2,0,1); $i1 < 3; $i2=$i0,$i0=$i1,$i1++ )
   {
       my $pt0 = $nodes->[$i0];
       my $pt1 = $nodes->[$i1];
       my $pt2 = $nodes->[$i2];

       if( exists $pt0->{nextpt}->{$pt1->{id}} )
       {
          die "Invalid triangulation - cannot form consistent triangle node order\n";
       }

       $pt0->{nextpt}->{$pt1->{id}} = $pt2->{id};
       $oppositenode{$pt2->{id}.' '.$pt1->{id}} = $pt0->{id};
   }
}

# For each node form the ordered array of surrounding nodes and adjacent edges.
# Ultimately these will be put into a single array of integers, containing for each
# node a count of surrounding nodes and the ids of the surrounding nodes and opposite
# nodes across the corresponding edges.  arrindex will the the index into this array 
# for the node.

my $arrindex = 0;
foreach my $pt (@pts )
{
   $pt->{arrindex} = $arrindex;
   my $snodes = $pt->{nextpt};
   my $nsnodes = scalar(keys %$snodes);
   if( $nsnodes == 0 ) { die "Invalid triangulation: node $pt->{srcid} is not used in any triangulation\n"; }

   my %reverse = map { $snodes->{$_} => $_ } keys %$snodes;
   my @start = grep { ! exists $reverse{$_} } keys %$snodes;
   if( scalar(@start) > 1 ){ die "Triangulation too complex at node $pt->{srcid}\n"; }
   my $start = $start[0] || (keys %$snodes)[0];
   my @nodes = ();
   my @opposite = ();
   push(@nodes,$start);
   my $end = $start;
   my $next = $snodes->{$start};
   do
   {
      push(@nodes,$next); 
      push(@opposite, $oppositenode{ $end.' '.$next} || 0 );
      $end = $next;
      $next = $snodes->{$end} || 0;
   } until $next == $start || ! $end;
   push(@opposite, $oppositenode{ $end.' '.$start} || 0 );
   $pt->{snodes} = \@nodes;
   $pt->{opposite} = \@opposite;
   $nsnodes+=2 if ! $next;
   die "Error in algorithm - node $pt->{srcid}!\n" if scalar(@nodes) != $nsnodes || scalar(@opposite) != $nsnodes;
   $arrindex += 1 + 2*$nsnodes;
}


# Create the output file ...

open(OUT,">$outfile") || die "Cannot open output file $outfile\n";
#no strict qw/subs/;
binmode(OUT);
#use strict qw/subs/;

# Write the output file signature string

print OUT $filesig;

# Write out the header information

my $npts = scalar(@pts);

print OUT $pack->string( @headers{qw/HEADER0 HEADER1 HEADER2 CRDSYS/} );
print OUT $pack->double($ymin,$ymax,$xmin,$xmax);
print OUT $pack->short($npts,$ndim);
print OUT $pack->long($arrindex);

# Write out the point coordinates

foreach my $pt (@pts) { print OUT $pack->double( @{$pt->{crd}} ); }

# Write out the point data

foreach my $pt (@pts) { print OUT $pack->double( @{$pt->{data}} ); }

# Write out the topology array index

foreach my $pt (@pts) { print OUT $pack->long( $pt->{arrindex} ); }

# Write out the topology array

foreach my $pt (@pts) { 
  my $nsnode = scalar(@{$pt->{snodes}});
  print OUT $pack->short($nsnode);
  print OUT $pack->short(@{$pt->{snodes}});
  print OUT $pack->short(@{$pt->{opposite}});
  }
   
close(OUT);

sub OrderTriangleNodes
{
  my ($trg) = @_;
  my $c0 = $trg->{pts}->[0]->{crd};
  my $c1 = $trg->{pts}->[1]->{crd};
  my $c2 = $trg->{pts}->[2]->{crd};
  my $dx1 = $c1->[0] - $c0->[0];
  my $dy1 = $c1->[1] - $c0->[1];
  my $dx2 = $c1->[0] - $c2->[0];
  my $dy2 = $c1->[1] - $c2->[1];
  my $a = $dx2*$dy1 - $dx1*$dy2;
  $trg->{area} = $a;
  &ReverseTriangleNodes($trg) if $a < 0;
}

sub ReverseTriangleNodes
{
  my ($trg) = @_;
  my $tmp=$trg->{pts}->[1];
  $trg->{pts}->[1] = $trg->{pts}->[2];
  $trg->{pts}->[2] = $tmp;
  $trg->{area} = -$trg->{area};
}
