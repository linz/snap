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
#    FORMAT   either TRIG1L (little endian) or TRIG1B (big endian)
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

Options are:
  -s blocksize  Defines the number of triangles stored in each block
  -c            Output point coordinates at end of each block

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
     $pts{$id} = {id=>$id, crd=>[$x,$y],data=>\@data};
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
if( $fmt !~ /^TRIG1[BL]$/ ) {
   print STDERR "Format in FORMAT header record not valid - must be TRIG1B or TRIG1L\n";
   $ok = 0;
   }


die "Aborted with errors\n" if ! $ok;

my $bigendian = $fmt eq 'TRIG1B';
my $pack = new Packer( $bigendian );
my $filesig = $bigendian ? "CRS trig binary v1.0  \r\n\x1A" :
                           "SNAP trig binary v1.0 \r\n\x1A";
          
# Sort out the triangles, identifying adjacent triangles

my %edges;
foreach my $t (@trgs) {
   my $pts = $t->{pts};
   my @trg_edges = (undef,undef,undef);
   $t->{edges} = \@trg_edges;
   my $nextpt = $pts->[0];
   for my $i (2,1,0) {
      my $edge = join( ' ' , sort {$a <=> $b} $pts->[$i]->{id}, $nextpt->{id} );
      $nextpt = $pts->[$i];
      if( defined $edges{$edge} ) {
         ${$edges{$edge}->{edge}} = $t;
         $trg_edges[$i] = $edges{$edge}->{trg};
         }
      else {
         $edges{$edge}->{edge} = \$trg_edges[$i];
         $edges{$edge}->{trg} = $t;
         }
      }
   }

# Sort out the triangles into data blocks..
# Define the block size to use...
 
$blocksize = 20 if ! $blocksize; 

# Create an array of triangle centres...

my @centres;
foreach my $t (@trgs) {
   my ($x, $y);
   for my $i (0..2) {
      $x += $t->{pts}->[$i]->{crd}->[0];
      $y += $t->{pts}->[$i]->{crd}->[1];
      }
   push( @centres, { crd=>[$x/3,$y/3], trg=>$t } );
   }

# Split the array into blocks of up to blocksize elements

my $ntrg = scalar(@trgs);

my $nblock = int(($ntrg-1)/$blocksize) + 1;
$blocksize = int(($ntrg-1)/$nblock) + 1;
my $nfullblock = $ntrg - $nblock*($blocksize-1);

my @splits;
my @splitdir;
my @blockrange;
my $range = [[$xmin,$ymin],[$xmax,$ymax]];

&CalcSplits( \@centres, 0, $#centres, $blocksize, $nfullblock, \@splits,
 \@splitdir, 0, $nblock-2, $range, 0, \@blockrange );

my $c0 = 0;
my @block;
my $maxblockpoints = 0;

foreach my $nb (0 .. $nblock-1) { 
   my $c1 = $c0+$blocksize-1;
   $c1-- if $nb >= $nfullblock;
   my @blockcentres = @centres[$c0 .. $c1];
   my $blockrange = $blockrange[$nb];
   my @blocksplits;
   my @blocksplitdir;
   &CalcSplits( \@blockcentres, 0, $#blockcentres,1,scalar(@blockcentres),
       \@blocksplits, \@blocksplitdir, 0, $#blockcentres-1,
       $blockrange, 0, undef );
   $c0 = $c1+1;

   # Form a list of points for the block and number the triangle vertices
   # correspondingly.  Note that the vertices are numbered according to the
   # edge that they oppose... (hence the 2,0,1 list of nodes)

   my %pntblockid;
   my @blkpoints;

   foreach my $bid (0 .. $#blockcentres) {
      my $c = $blockcentres[$bid];
      my @v = map { $c->{trg}->{pts}->[$_]->{id} } (2,0,1); 
      my @cv;
      foreach my $ptid ( @v ) {
         if( ! exists $pntblockid{$ptid} ) {
            push(@blkpoints,$pts{$ptid});
            $pntblockid{$ptid} = $#blkpoints;
            }
         push(@cv, $pntblockid{$ptid});
         }
      $c->{trg}->{blockid} = $bid;
      $c->{trg}->{blockno} = $nb;
      $c->{blockvertexid} = \@cv;
      }

   my $nblockpoints = scalar(@blkpoints);
   $maxblockpoints = $nblockpoints if $nblockpoints > $maxblockpoints;

   $block[$nb] = { centres=>\@blockcentres, 
                   points=>\@blkpoints,
                   splits=>\@blocksplits,
                   splitdir=>\@blocksplitdir };
   }

# Create the output file ...

open(OUT,">$outfile") || die "Cannot open output file $outfile\n";
#no strict qw/subs/;
binmode(OUT);
#use strict qw/subs/;

# Write the output file signature string

print OUT $filesig;

# Write a placeholder for output file index location

my $indexloc = tell(OUT);
print OUT $pack->long(0);

# Now write out the data for each block of triangles ...

foreach my $nb (0 .. $nblock-1) { 
   # Get the location of the file in the block...
   my $block = $block[$nb];
   my $loc = tell(OUT);
   $block->{loc} = $loc;
   my $ntrig = scalar(@{$block->{centres}});

   # Print out the size of the block ...

   print OUT $pack->short($ntrig,scalar(@{$block->{points}}));

   # Print out the block splits ...

   if( $ntrig > 1 ) {
      print OUT $pack->double(@{$block->{splits}});
      print OUT $pack->char(@{$block->{splitdir}});
      }

   # Print out the centres - first the short data (vertex ids, adjacent 
   # triangle identifiers ...

   foreach my $c ( @{$block->{centres}} ) {
      print OUT $pack->short( @{$c->{blockvertexid}} );
   
      # Then print out adjacent triangle info ...

      my $edges = $c->{trg}->{edges};
      foreach my $e (@$edges) {
        if( defined($e) ) {
          print OUT $pack->short( $e->{blockno}, $e->{blockid} );
          }
        else {
          print OUT $pack->short( -1, -1 );
          }
        }

      }

   # ... then the double type data for the triangles, the centre,
   # and for each edge the normal to the edge and the height in that 
   # direction ... 

   foreach my $c ( @{$block->{centres}} ) {
      print OUT $pack->double( @{$c->{crd}} );
      # Form an array of coordinates ...
      my @c;
      for my $i (0..2) {
        $c[$i] = $c->{trg}->{pts}->[$i]->{crd};
        }
      $c[3] = $c[0];
      $c[4] = $c[1];
      for my $i (0..2) {
        my $dx = $c[$i]->[1] - $c[$i+1]->[1]; 
        my $dy = $c[$i+1]->[0] - $c[$i]->[0]; 
        my $ds = sqrt($dx*$dx+$dy*$dy);
        # Code to handle triangles with a redundant side - these could
        # occur on break lines...
        if( $ds <= $small ) {
           $dx = $c[$i+2]->[0] - ($c[$i+1]->[0]+$c[$i]->[0])/2;
           $dy = $c[$i+2]->[1] - ($c[$i+1]->[1]+$c[$i]->[1])/2;
           $ds = sqrt($dx*$dx+$dy*$dy);
           }
        if( $ds > 0.0 ) { $dx /= $ds; $dy /= $ds; }
        $ds = ($c[$i+2]->[0] - $c[$i]->[0])*$dx +
              ($c[$i+2]->[1] - $c[$i]->[1])*$dy;
        print OUT $pack->double( $dx, $dy, $ds );
        }
      }

   # Print out the point data ...

   foreach my $pt ( @{$block->{points}} ) {
      print OUT $pack->double( @{$pt->{data}} );
      }

   if( $writecrds ) {
      foreach my $pt ( @{$block->{points}} ) {
         print OUT $pack->double( @{$pt->{crd}} );
         }
      }
   
   }

# Now print out the block index information ...

my $indexval = tell(OUT);

print OUT $pack->double($ymin,$ymax,$xmin,$xmax);
print OUT $pack->long($ntrg);
print OUT $pack->short($ndim,$nblock,$blocksize,$nfullblock,
                       $maxblockpoints,$writecrds ? 1 : 0);
print OUT $pack->string( @headers{qw/HEADER0 HEADER1 HEADER2 CRDSYS/} );
if( $nblock > 1 ) {
   print OUT $pack->double( @splits );
   print OUT $pack->char( @splitdir );
   }
print OUT $pack->long( map {$_->{loc}} @block );

seek(OUT,$indexloc,0);
print OUT $pack->long($indexval);
close(OUT);


# CalcSplits recursively splits data at (alternatively) E-W or N-S boundaries
# (based on whichever range is greater in the element being split)
# The data are finally to be split into $nblock blocks, of which the first
# $nfullblocks are to be full, and the rest to have one less element per block
# The current split is of elements $c0 to $c1 if the centres array, which will
# be reordered. The split EW and NS values are stored into @$splits arrays with
# the current splits being saved to elements $b0 to $b1.  The axis used for
# the split (element 0 or 1 of the coordinates) is stored in the @$splitdir
# array.  $range is the coordinate extents being split.  Optionally the 
# $blockrange array is used to store the final ranges of the blocks.
#
# Looking at this code it is useful to remember that n splits divide 
# n+1 blocks.

sub CalcSplits {
  my ($centres, $c0, $c1, $blocksize, $nfullblocks, $splits, $splitdir, $b0, $b1, $range, $nb, $blockrange ) = @_;

  if ($b0 > $b1) { 
      if( defined($blockrange) ) {
        $blockrange->[$nb] = [ [$range->[0]->[0], $range->[0]->[1]],
                               [$range->[1]->[0], $range->[1]->[1]] ];
        }
      return 
      }
  # Pick the axis to split
  my $ns = ($range->[1]->[0]-$range->[0]->[0]) > 
           ($range->[1]->[1]-$range->[0]->[1]) ?
           0 : 1;
  # Sort the centres according to the coordinate
  @{$centres}[$c0..$c1] = 
     sort {$a->{crd}->[$ns] <=> $b->{crd}->[$ns]} @{$centres}[$c0..$c1];
  # Calculate the offset of the next split
  my $bs = int( ($b1-$b0+1)/2);
  my $cs = ($bs+1)*$blocksize;
  $nfullblocks = 0 if $nfullblocks < 0;
  if( $bs+1 > $nfullblocks ) {
      $cs -= ($bs+1-$nfullblocks); 
      }
  $cs += $c0-1;
  $bs += $b0;

  my $splitval = ($centres->[$cs]->{crd}->[$ns] + $centres->[$cs+1]->{crd}->[$ns])/2;
  $splits->[$b0] = $splitval;
  $splitdir->[$b0] = $ns;
  
  my $save = $range->[1]->[$ns];
  $range->[1]->[$ns] = $splitval;
  &CalcSplits($centres, $c0, $cs, $blocksize, $nfullblocks, 
               $splits, $splitdir, $b0+1, $bs, $range, $nb, $blockrange );

  $range->[1]->[$ns] = $save;
  $save = $range->[0]->[$ns];
  $range->[0]->[$ns] = $splitval;
  &CalcSplits($centres, $cs+1, $c1, $blocksize, $nfullblocks-($bs+1-$b0), 
               $splits, $splitdir, $bs+1, $b1, $range, $nb+($bs+1-$b0),
               $blockrange );
  $range->[0]->[$ns] = $save;

  }
