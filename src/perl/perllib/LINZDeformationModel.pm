#!/usr/local/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Perl module to interpolate deformation from a LINZ
#                      deformation model binary file.  The binary file may
#                      contain both grid and triangulated deformation models
#                      each of which may apply for a specific region and 
#                      interval.
#
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook          03/02/2004  Created
#===============================================================================


use strict;

use Unpacker;
use FileHandle;

package LINZDeformationModel;

use vars qw/$debug/;

$debug = 0;

# Each possible triangle file format has a specified header record.  Valid records
# are listed here.  The %formats hash converts these to a format definition
# string - one of DEF1L (little endian version 1 format), or
# DEF1B (big endian version 1 format).

my $sigdef1l = "LINZ deformation model v1.0L\r\n\x1A";
my $sigdef1b = "LINZ deformation model v1.0B\r\n\x1A";

my $siglen = length($sigdef1l);

my %formats = (
    $sigdef1l => 'DEF1L',
    $sigdef1b => 'DEF1B' );

sub new {
   my ($class, $filename) = @_;

   # Open the  deformation file in binary mode.
   
   my $fh = new FileHandle; 
   $fh->open($filename,'r') || die "Cannot open LINZ deformation file $filename\n";
   binmode($fh);

   my $self = {
       filename=>$filename,
       fh=>$fh,
       titles=>["Triangle mesh data from file $filename"],
       crdsyscode=>'NONE'
       };

   bless $self, $class;
  
   $self->Setup;

   return $self;
   }

# Default set up for a LINZ triangle file

sub Setup {
   my ($self) = @_;

   my $filename = $self->{filename};
   my $fh = $self->{fh};
  
   # Read the triangle file signature and ensure that it is valid
   
   my $testsig;
   read($fh,$testsig,$siglen);
   
   my $fmt = $formats{$testsig};
   
   die "$filename is not a valid deformation file - signature incorrect\n" if ! $fmt;
   
   # Set up the parameters required to handle endian-ness.  
   # $swapbytes specifies the file endian-ness is opposite that of the
   # host (ie bytes must be reversed for numeric types).  $shortcode and
   # $longcode are the pack/unpack codes for unsigned short and long integers.
   
   my $filebigendian = $fmt eq 'DEF1B';
   my $unpacker = new Unpacker($filebigendian,$fh);
   
   # Read location in the file of the deformation index data
   
   my $loc=$unpacker->read_long;
   die "Deformation file not completed\n" if ! $loc;
   
   # Jump to the index and  read the triangle data
   
   seek($fh,$loc,0);
   
   my $name = $unpacker->read_string;
   my $version = $unpacker->read_string;
   my $crdsyscode = $unpacker->read_string;
   my $description = $unpacker->read_string;
   my $versiondate = &ReadDate($unpacker);
   my $startdate = &ReadDate($unpacker);
   my $enddate = &ReadDate($unpacker);
   my $range = LINZDeformationModel::Range->read($unpacker);
   my $isgeog = $unpacker->read_short;
   my $nseq = $unpacker->read_short;
   
   my @sequences = ();
   for my $iseq (1..$nseq) {
      my $s = LINZDeformationModel::Sequence->read($fh,$unpacker);
      $s->{seqid} = $iseq;
      push(@sequences,$s );
      }

   $self->{name} = $name;
   $self->{version} = $version;
   $self->{crdsyscode} = $crdsyscode;
   $self->{description} = $description;
   $self->{versiondate} = $versiondate;
   $self->{startdate} = $startdate;
   $self->{enddate} = $enddate;
   $self->{range} = $range;
   $self->{isgeog} = $isgeog;
   $self->{sequences} = \@sequences;
   }

sub Calc {
   my ( $self, $date, $x, $y ) = @_;
   if( $date < $self->{startdate}->{years} ||
       $date > $self->{enddate}->{years} ) {
       die "Date outside valid range for deformation model\n";
       }

   # Obtain a list of components and multiples ...

   my @components;
   foreach my $s (@{$self->{sequences}} ) {
      if( $debug ) {
         print "Evaluating sequence ",$s->{seqid},"\n";
         }
      push(@components,$s->CalcComponents($date,$x,$y));
      }


   # Total up the deformation ...

   my @def = (0,0,0);
   foreach my $cv (@components) {
      my ($c,$mult,$zerobeyond,$ndim) = @$cv;
      eval {
        my @cdef = $c->Calc( $x, $y );
        if( $ndim == 1 ) {
           $def[2] += $cdef[0]*$mult;
           }
        else {
           $def[0] += $cdef[0]*$mult;
           $def[1] += $cdef[1]*$mult;
           $def[2] += $cdef[2]*$mult if $ndim == 3;
           }
        };
      if( $@ && ! $zerobeyond ) {
        die "Point is outside valid range of calculation\n";
        }
      }
   return @def;
   }

sub BEGIN {
use Time::JulianDay;
my %jan1days;

sub CalcYears {
   my ($y,$m,$d,$hr,$mn,$sc) = @_;
   my $y0 = $jan1days{$y} || ($jan1days{$y} = julian_day($y,1,1));
   my $y1 = $jan1days{$y+1} || ($jan1days{$y+1} = julian_day($y+1,1,1));
   my $yd = julian_day($y,$m,$d)+ ($hr+$mn/60+$sc/3600)/24;
   return $y + ($yd-$y0)/($y1-$y0);
   }
}

#===============================================================================
#
#   SUBROUTINE:   ReadDate
#
#   DESCRIPTION:  Reads a date value from $fh, taking account
#                 of endian-ness.  The date is packed as 6 short
#                 values, year, month, day, hour, minute, second.
#                 Returns an hash with elements ymd (array ref to
#                 array of 6 components), and years to decimal year.
#
#   PARAMETERS:   $fh          The triangle file
#                 $shortcode   Code for unpacking a short value
#
#   RETURNS:      The value read
#
#
#===============================================================================


sub ReadDate {
  my( $unpacker ) = @_;
  my @ymd =  $unpacker->read_short(6);
  return { ymd=>\@ymd, years=>&CalcYears(@ymd) };
}

#======================================================================

package LINZDeformationModel::Sequence;

sub read {
  my( $class,$fh,$unpacker ) = @_;

   my $name = $unpacker->read_string;
   my $description = $unpacker->read_string;
   my $startdate = &LINZDeformationModel::ReadDate($unpacker);
   my $enddate = &LINZDeformationModel::ReadDate($unpacker);
   my $range = LINZDeformationModel::Range->read($unpacker);
   my ($isvelocity,$dimension,$zerobeyond,$ncomponents) = $unpacker->read_short(4);
  
   my @components = ();
   for my $icomp (1..$ncomponents) {
      my $c = LINZDeformationModel::Component->read($fh,$unpacker);
      $c->{compid} = $icomp;
      push(@components,$c);
      }
   
   my $self = {
      name => $name,
      description => $description,
      startdate => $startdate,
      enddate => $enddate,
      range => $range,
      dimension => $dimension,
      isvelocity => $isvelocity,
      zerobeyond => $zerobeyond,
      components => \@components,
      };
   
   return bless $self, $class;
   }

sub CalcComponents {
   my( $self, $date, $x, $y ) = @_;
   if($date < $self->{startdate}->{years} || 
                $date > $self->{enddate}->{years}) {
      if( $LinzDeformationModel::debug ) {
         print "Sequence ",$self->{seqid}," not relevant for obs time\n";
         }
      return () ;
      }
   $x = $self->{range}->WrapLongitude($x);
   if( ! $self->{range}->Includes( $x, $y ) ) {
      die "Cannot evaluate ".$self->{name}." at $x $y\n"
        if ! $self->{zerobeyond};
      return ();
      }
   if( $self->{isvelocity} ) {
      if( $LinzDeformationModel::debug ) {
         print "Calculating velocity sequence ",$self->{seqid},"\n";
         }
      return $self->CalcVelocityComponents($date,$x,$y);
      }
   else {
      if( $LinzDeformationModel::debug ) {
         print "Calculating deformation sequence ",$self->{seqid},"\n";
         }
      return $self->CalcDeformationComponents($date,$x,$y);
      }
   }


sub CalcVelocityComponents {
   my( $self, $date, $x, $y ) = @_;
   my $components = $self->{components};
   my $zerobeyond = $self->{zerobeyond};
   my $dimension = $self->{dimension};
   my @results = ();
   for my $c (@$components) { 
     my $dy = $date - $c->{refdate}->{years}; 
     if( $dy < 0  && $c->{usebefore} || $dy > 0 && $c->{useafter} ) {
        $x = $self->{range}->WrapLongitude($x);
        if( $c->{range}->Includes($x,$y) ) {
          push( @results, [$c,$dy,$zerobeyond,$dimension] );
          }
        elsif( ! $c->{zerobeyond} ) {
          die "Cannot evaluate ".$self->{name}." at $x $y\n";
          }
        }
     }
   return @results;
   }


sub CalcDeformationComponents {
   my( $self, $date, $x, $y ) = @_;
   my $components = $self->{components};
   my $zerobeyond = $self->{zerobeyond};
   my $dimension = $self->{dimension};
   my ($i0,$i1,$y0,$y1);
   my $nc0 = $#$components;
   for my $i (0 .. $nc0) {
      $i1 = $i;
      $y1 = $components->[$i]->{refdate}->{years};
      last if $y1 > $date;  
      $i0 = $i1;
      $y0 = $y1;
      }

   # If between two components, valid options are  ...

   if( $i1 > 0 && $i0 < $nc0 ) {
      my $c0 = $components->[$i0];
      my $c1 = $components->[$i1];
      if( $c0->{useafter} == 2 && $c1->{usebefore} == 2 ) {
         $y0 = ($y1-$date)/($y1-$y0);
         $y1 = 1-$y0;
         return ([$c0,$y0,$zerobeyond,$dimension],[$c1,$y1,$zerobeyond,$dimension]);
         }
      elsif( $c0->{useafter} == 1 && ! $c1->{usebefore} ) {
         return ([$c0,1,$zerobeyond,$dimension]);
         }
      elsif( $c1->{usebefore} == 1 && ! $c0->{useafter} ) {
         return ([$c1,1,$zerobeyond,$dimension]);
         }
      elsif( ! $c0->{useafter} && ! $c1->{usebefore} ) {
         return ();
         }
      else {
         die "Inconsistent usage of components in sequence ".$self->{name}."\n";
         }
      }

   # Else if only one component ...
   elsif ( $nc0 == 0 ) { 
      my $c1 = $components->[0];
      if( ($c1->{usebefore}==1 && $y1 > $date) || ($c1->{useafter}==1 && $y1 <= $date) ) {
         return ([$c1,1,$zerobeyond,$dimension]);
         }
       return ();
       }

   # else if before first ...
   elsif ( $i1 == 0 ) {
      my $c1 = $components->[0];
      return () if ! $c1->{usebefore};
      return ([$c1,1,$zerobeyond,$dimension]) if $c1->{usebefore} == 1;
      my $c0 = $components->[1];
      my $y0 = $c0->{refdate}->{years};
      $y1 = ($date-$y0)/($y1-$y0);
      $y0 = 1-$y1;
      return ([$c1,$y1,$zerobeyond,$dimension],[$c0,$y0,$zerobeyond,$dimension]);
      }

   # else if after last ...
   elsif ( $i0 == $nc0 ) {
      my $c0 = $components->[$nc0];
      return () if ! $c0->{useafter};
      return ([$c0,1,$zerobeyond,$dimension]) if $c0->{useafter} == 1;
      my $c1 = $components->[$nc0-1];
      my $y1 = $c1->{refdate}->{years};
      $y1 = ($date-$y0)/($y1-$y0);
      $y0 = 1-$y1;
      return ([$c1,$y1,$zerobeyond,$dimension],[$c0,$y0,$zerobeyond,$dimension]);
      }
   }

#======================================================================

package LINZDeformationModel::Component;

sub read {
   my( $class,$fh,$unpacker ) = @_;
   my $description = $unpacker->read_string;
   my $refdate = &LINZDeformationModel::ReadDate($unpacker);
   my $range = LINZDeformationModel::Range->read($unpacker);
   my ($usebefore,$useafter,$istrig) = $unpacker->read_short(3);
   my $fileloc = $unpacker->read_long;
   
   my $self = {
      description => $description,
      refdate => $refdate,
      range => $range,
      usebefore => $usebefore,
      useafter =>$useafter,
      istrig => $istrig,
      fileloc => $fileloc,
      fh => $fh,
      model => undef
      };

   return bless $self, $class;
   }


sub Calc {
   my ($self, $x, $y ) = @_;
   if( ! $self->{model} ) {
       if( $self->{istrig} ) {
           require TrigFile;
           $TrigFile::debug = $LinzDeformationModel::debug;
           $self->{model} = newEmbedded TrigFile $self->{fh}, $self->{fileloc};
           }
       else {
           require GridFile;
           $GridFile::debug = $LinzDeformationModel::debug;
           $self->{model} = newEmbedded GridFile $self->{fh}, $self->{fileloc};
           }
       }
   if( $LinzDeformationModel::debug ) {
       print "Calculating component ".$self->{compid},"\n";
       }
   return $self->{model}->Calc($x,$y);
   }


#======================================================================

package LINZDeformationModel::Range;

sub read {
   my( $class, $unpacker ) = @_;
   my @range = $unpacker->read_double(4);
   my $self = \@range;
   return bless $self, $class;
   }

sub WrapLongitude {
    my ($self,$x) = @_;
    my $x0 = $x < $self->[2] ? $x+360 : $x > $self->[3] ? $x-360 : $x;
    return $x0 >= $self->[2] && $x0 <= $self->[3] ? $x0 : $x;
    }

sub Includes {
   my ($self, $x, $y ) = @_;
   return $y >= $self->[0] && $y <= $self->[1] &&
          $x >= $self->[2] && $x <= $self->[3];
   }
 
1;
