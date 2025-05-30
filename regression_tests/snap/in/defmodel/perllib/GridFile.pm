#!/usr/local/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Perl module to interpolate from a gridded model in
#                      a binary file generated by makegrid.pl.  
#
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook          03/02/2004  Created
#===============================================================================

package GridFile;

use strict;
use FileHandle;
use Unpacker;

# Each possible grid file format has a specified header record.  Valid records
# are listed here.  The %formats hash converts these to a format definition
# string - one of GEOID (old SNAP geoid format), GRID1L (little endian version
# 1 grid format), GRID1B (big endian version 1 grid format).

my $siggeoid = "SNAP geoid binary file\r\n\x1A";
my $siggrid1l = "SNAP grid binary v1.0 \r\n\x1A";
my $siggrid1b = "CRS grid binary v1.0  \r\n\x1A";
my $siglen = length($siggeoid);

my %formats = (
    $siggeoid => 'GEOID',
    $siggrid1l => 'GRID1L',
    $siggrid1b => 'GRID1B' );


sub new {
   my ($class, $filename) = @_;
   

   # Open the  grid file in binary mode.
   
   my $fh = new FileHandle;
   $fh->open($filename,'r') || die "Cannot open grid file $filename\n";
   binmode($fh);

   my $self = {
       filename=>$filename,
       fh=>$fh,
       titles=>["Grid data from file $filename"],
       nval=>1,
       vres=>1.0,
       crdsyscode=>'NONE',
       offset=>0,
       embedded=>0
       };

   bless $self, $class;
  
   $self->Setup;

   if( $self->{ngridx} < 2 || $self->{ngridy} < 2 ) {
       die "Invalid grid definition in grid file $filename\n";
       }

   return $self;
   }

sub newEmbedded {
   my ($class, $fh, $offset ) = @_;

   my $self = {
       filename=>'Embedded',
       fh=>$fh,
       titles=>["Grid data from embedded file"],
       nval=>1,
       vres=>1.0,
       crdsyscode=>'NONE',
       offset=>$offset,
       embedded=>1
       };

   bless $self, $class;
  
   seek($fh,$offset,0);

   $self->Setup;

   return $self;
   }


# Default set up for a LINZ grid file

sub Setup {
   my ($self) = @_;

   my $filename = $self->{filename};
   my $fh = $self->{fh};
  
   # Read the grid file signature and ensure that it is valid
   
   my $testsig;
   read($fh,$testsig,$siglen);
   
   my $fmt = $formats{$testsig};
   
   die "$filename is not a valid grid file - signature incorrect\n" if ! $fmt;
   
   my $filebigendian = $fmt eq 'GRID1B';
   my $unpacker = new Unpacker($filebigendian,$fh);
   
   # Read location in the file of the grid index data
   
   my $loc = $unpacker->read_long;
   die "Grid file not completed\n" if ! $loc;
   
   # Jump to the index and  read the grid data
   
   seek($fh,$loc+$self->{offset},0);
   
   my ($ymn,$ymx,$xmn,$xmx,$vres) = $unpacker->read_double(5);
   my ($ny,$nx) = $unpacker->read_short(2);
   
   # If the format is 'GEOID'  then two fields are not in the file, they
   # are set to default values.
   
   my( $dim, $latlon);
   if( $fmt ne 'GEOID' ) {
      ($dim,$latlon)=$unpacker->read_short(2);
      }
   else {
      $dim = 1;
      $latlon = 1;
      }
   
   # Read the grid descriptive data
   
   my ($s1,$s2,$s3,$s4) = $unpacker->read_string(4);
   
   # Read the file location of each row of data in the file..
   
   my @locs=$unpacker->read_long($ny);
   
   # Calculate the size of a buffer representing an entire row
   
   my $rowsize = $nx*$dim;

   # Store the generic grid file info

   $self->SetTitles($s1,$s2,$s3);
   $self->SetCrdSysCode( $s4 );
   $self->SetGridExtents( $xmn, $xmx, $nx, $ymn, $ymx, $ny, $latlon );
   $self->SetDimension( $dim );

   # Store the information specific to LINZ grid files

   $self->{locdata} = \@locs;
   $self->{rowsize} = $rowsize;
   $self->{data} = {};
   $self->{vres} = $vres;
   $self->{unpacker} = $unpacker;
   }

sub GetRow {
   my( $self, $row ) = @_;
   my $data = $self->{data};
   if( ! exists($data->{$row}) ) {
      die "Invalid grid row $row requested from $self->{filename}\n"
        if $row < 0 || $row >= $self->{ngridy};
      my $unpacker = $self->{unpacker};
      my $fh = $self->{fh};
      my $rowsize = $self->{rowsize};
      my $loc = $self->{locdata}->[$row];
      seek($fh,$loc+$self->{offset},0);
      my @vals = $unpacker->read_short($rowsize);
      $data->{$row} = \@vals;
      }
   return $data->{$row};
   }


sub SetTitles {
   my ($self, @titles) = @_;
   $self->{titles} = \@titles;
   }

sub SetCrdSysCode {
   my ($self, $code ) = @_;
   $self->{crdsyscode} = $code;
   }

sub SetGridExtents {
   my($self,$xmin,$xmax,$ngrdx,$ymin,$ymax,$ngrdy,$latlon) = @_;
   if( $ngrdx < 2 || $ngrdy < 2 ) {
      my $filename = $self->{filename};
      die "$filename: Invalid grid extents\n";
      }

   my $xres = ($xmax - $xmin)/($ngrdx-1);
   my $yres = ($ymax - $ymin)/($ngrdy-1);

   my $global = 0;
   $global = 1 if $latlon && abs($ngrdx*$xres - 360) < 0.001;

   $self->{xmin} = $xmin;
   $self->{xmax} = $xmax;
   $self->{ymin} = $ymin;
   $self->{ymax} = $ymax;
   $self->{xres} = $xres;
   $self->{yres} = $yres;
   $self->{ngridx} = $ngrdx;
   $self->{ngridy} = $ngrdy;
   $self->{latlon} = $latlon;
   $self->{global} = $global;
   }


sub SetDimension {
   my ($self,$ndim) = @_;
   $self->{nval} = $ndim;
   }
sub DESTROY {
   my( $self ) = @_;
   if( ! $self->{embedded} ) {
     my $fh = $self->{fh};
     close($fh);
     }
   }

sub FileName {
   my ($self) = @_;
   return $self->{filename};
   }

sub Title {
   my ($self) = @_;
   return grep /\S/, @{$self->{titles}};
   }

sub CrdSysCode {
   my ($self) = @_;
   return $self->{crdsyscode};
   }

sub Range {
   my ($self) = @_;
   return @{$self}{ 'xmin', 'ymin', 'xmax', 'ymax' };
   }

sub GridSize {
   my ($self) = @_;
   return @{$self}{ 'ngridx', 'ngridy' };
   }

sub GridSpacing {
   my ($self) = @_;
   return @{$self}{ 'xres', 'yres' };
   }

sub Dimension {
   my ($self) = @_;
   return $self->{nval};
   }
  
sub GridLoc {
   my( $self, $x, $y ) = @_;
   $x -= $self->{xmin};
   if( $self->{latlon} ) {
     $x -= 360.0 while $x > 360;
     $x += 360.0 while $x < 0;
     }
   $x /= $self->{xres};
   $y = ($y - $self->{ymin}) / $self->{yres};
   return ($x, $y);
   }
   
sub LinearFactors {
   my( $x ,$max, $global ) = @_;
   die "Coordinates out of range for grid\n" if
     ! $global && ($x < 0 || $x > $max-1);
   my $r = int($x);
   $r-- if $r > $x;
   $x -= $r;
   $r %= $max;
   my $r1 = ($r+1) % $max;
   return ($r, 1-$x, $r1, $x );
   }

sub CalcLinear {
   my( $self, $x, $y) = @_;
   ($x,$y) = $self->GridLoc($x,$y);
   my $nval = $self->{nval};
   my @xi = &LinearFactors( $x, $self->{ngridx}, $self->{global} );
   my @yi = &LinearFactors( $y, $self->{ngridy} );
   my ($ix, $iy);
   my @val;
   for( $iy = 0; $iy < $#yi; $iy+=2 ) {
       my( $row, $ymult ) = @yi[$iy,$iy+1];
       my $data = $self->GetRow($row);
       for( $ix = 0; $ix < $#xi; $ix+=2 ) {
          my( $col, $xmult ) = @xi[$ix,$ix+1];
          $col *= $nval;
          foreach( 0 .. $nval-1 ) {
             $val[$_] += $xmult*$ymult*$data->[$col+$_];
             }
          }
       }

   return map { $_ *= $self->{vres} } @val;
   }

sub CubicFactors {
   my( $x ,$max, $global ) = @_;
   die "Coordinates out of range for grid\n" if
     ! $global && ($x < 1 || $x > $max-2 );
   my $r = int($x);
   $r-- if $r > $x;
   $x -= $r;
   $x = 2*$x-1;
   my( $x0, $x1, $x2, $x3 ) = ($x+3, $x+1, $x-1, $x-3);
   return (
      ($r-1)%$max, -($x1*$x2*$x3)/48.0,
      $r % $max,    ($x0*$x2*$x3)/16.0,
      ($r+1)%$max, -($x0*$x1*$x3)/16.0,
      ($r+2)%$max,  ($x0*$x1*$x2)/48.0 );
   }


sub CalcCubic {
   my( $self, $x, $y) = @_;
   ($x,$y) = $self->GridLoc($x,$y);
   my $nval = $self->{nval};
   my @xi = &CubicFactors( $x, $self->{ngridx}, $self->{global} );
   my @yi = &CubicFactors( $y, $self->{ngridy} );
   my ($ix, $iy);
   my @val;
   for( $iy = 0; $iy < $#yi; $iy+=2 ) {
       my( $row, $ymult ) = @yi[$iy,$iy+1];
       my $data = $self->GetRow($row);
       for( $ix = 0; $ix < $#xi; $ix+=2 ) {
          my( $col, $xmult ) = @xi[$ix,$ix+1];
          $col *= $nval;
          foreach( 0 .. $nval-1 ) {
             $val[$_] += $xmult*$ymult*$data->[$col+$_];
             }
          }
       }

   return map { $_ *= $self->{vres} } @val;
   }


sub Calc {
   my( $self, $x, $y) = @_;
   return $self->CalcLinear($x,$y);
   }

1;

