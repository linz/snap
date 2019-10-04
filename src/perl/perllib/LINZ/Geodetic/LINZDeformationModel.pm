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

use LINZ::Geodetic::Util::Unpacker;
use FileHandle;

package LINZ::Geodetic::LINZDeformationModel;

use vars qw/$debug/;
use vars qw/$AUTOLOAD/;

$debug = $ENV{DEBUG_LINZDEF} || 0;


# Each possible triangle file format has a specified header record.  Valid records
# are listed here.  The %formats hash converts these to a format definition
# string - one of DEF1L (little endian version 1 format), 
# DEF1B (big endian version 1 format), DEF2L, or DEF2B

my $sigdef1l = "LINZ deformation model v1.0L\r\n\x1A";
my $sigdef1b = "LINZ deformation model v1.0B\r\n\x1A";
my $sigdef2l = "LINZ deformation model v2.0L\r\n\x1A";
my $sigdef2b = "LINZ deformation model v2.0B\r\n\x1A";
my $sigdef3l = "LINZ deformation model v3.0L\r\n\x1A";
my $sigdef3b = "LINZ deformation model v3.0B\r\n\x1A";

my $siglen = length($sigdef1l);

my %formats = (
    $sigdef1l => 'DEF1L',
    $sigdef1b => 'DEF1B',
    $sigdef2l => 'DEF2L',
    $sigdef2b => 'DEF2B',
    $sigdef3l => 'DEF3L',
    $sigdef3b => 'DEF3B',
);

sub new {
   my ($class, $filename) = @_;

   # Open the  deformation file in binary mode.
   
   my $fh = new FileHandle; 
   $fh->open($filename,'r') || die "Cannot open LINZ deformation file $filename\n";
   binmode($fh);

   my $self = {
       filename=>$filename,
       fh=>$fh,
       titles=>["Undefined deformation model"],
       crdsyscode=>'NONE'
       };

   bless $self, $class;
  
   $self->Setup;

   return $self;
   }

# Access file parameter

sub AUTOLOAD
{
    my ($self)=@_;
    my $function=$AUTOLOAD;
    $function =~ s/.*:://;
    return $self->{$function} if exists $self->{$function};
    return '';
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
   
   my $filebigendian = $fmt =~ /B$/;
   my $format_version = $1 if $fmt =~ /^DEF(\d+)[LB]$/;

   my $unpacker = new LINZ::Geodetic::Util::Unpacker($filebigendian,$fh);
   
   # Read location in the file of the deformation index data
   
   my $loc=$unpacker->read_long;
   die "Deformation file not completed\n" if ! $loc;
   
   # Jump to the index and read the header data
   
   seek($fh,$loc,0);
   
   my ($name,$crdsyscode,$startdate,$enddate,$range,$isgeog,$versions);
   if( $format_version < 3 )
   {
       $name = $unpacker->read_string;
       my $version = $unpacker->read_string;
       $crdsyscode = $unpacker->read_string;
       my $description = $unpacker->read_string;
       my $versiondate = &ReadDate($unpacker);
       $startdate = &ReadDate($unpacker);
       $enddate = &ReadDate($unpacker);
       $range = LINZ::Geodetic::LINZDeformationModel::Range->read($unpacker);
       $isgeog = $unpacker->read_short;
       # Fix for 20130801 model
       $version='00000000' if $version eq '';
       $versions=[{
               version=>$version,
               versiondate=>$versiondate,
               description=>$description
            }];
   }
   else
   {
       $name = $unpacker->read_string;
       $crdsyscode = $unpacker->read_string;
       $startdate = &ReadDate($unpacker);
       $enddate = &ReadDate($unpacker);
       $range = LINZ::Geodetic::LINZDeformationModel::Range->read($unpacker);
       $isgeog = $unpacker->read_short;
       $versions=[];
       my $nver = $unpacker->read_short;
       for my $iver (1..$nver)
       {
           my $version = $unpacker->read_string;
           my $versiondate = &ReadDate($unpacker);
           my $description = $unpacker->read_string;
           push(@$versions,{
                   version=>$version,
                   versiondate=>$versiondate,
                   description=>$description
               });
       }
   }
   my $nseq = $unpacker->read_short;
   sort { $b->{version} cmp $a->{version} } @$versions;
   my $version=$versions->[0];

   if( $debug )
   {
        print "Loading LINZ deformation model\n";
        print "    name=$name\n";
        print "    crdsyscode=$crdsyscode\n";
        print "    startdate=$startdate->{years}\n";
        print "    enddate=$enddate->{years}\n";
        printf "    range=%s\n",$range->tostring;
        print "    isgeog=$isgeog\n";
        print "    version=$version->{version}\n";
        print "    versiondate=$version->{versiondate}->{years}\n";
        print "    description=$version->{description}\n";
   }
   
   my @sequences = ();
   for my $iseq (1..$nseq) {
      my $s = LINZ::Geodetic::LINZDeformationModel::Sequence->read($format_version,$fh,$unpacker);
      $s->{seqid} = $iseq;
      push(@sequences,$s );
      }

   $self->{name} = $name;
   $self->{crdsyscode} = $crdsyscode;
   $self->{startdate} = $startdate;
   $self->{enddate} = $enddate;
   $self->{range} = $range;
   $self->{isgeog} = $isgeog;
   $self->{versions} = $versions;
   $self->{sequences} = \@sequences;
   $self->SetVersion($version->{version});
   }

sub SetVersion {
   my ($self, $version)=@_;
   my $vdef;
   foreach my $v (@{$self->{versions}})
   {
       if( $v->{version} eq $version )
       {
           $vdef=$v;
           last;
       }
   }
   die "Deformation model version $version\n" if ! $vdef;
   $self->{version}=$vdef->{version};
   foreach my $seq (@{$self->{sequences}} )
   {
       $seq->{enabled}=$version ge $seq->{startversion} && $version lt $seq->{endversion};
   }
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
      my @seqcomp=$s->CalcComponents($date,$x,$y);
      if( $debug )
      {
         my $name=$s->{name};
         $name =~ s/\s+//g;
         $name = substr($name,0,40);
         print "Evaluating sequence ",$s->{seqid}," $name\n";
         foreach my $c (@seqcomp)
         {
             my $desc=$c->[0]->{description};
             my $factor=$c->[1];
             $desc =~ s/\s+/ /g;
             $desc = substr($desc,0,40);
             print "   $factor * $desc\n";
         }
      }
      push(@components,@seqcomp);
      }


   # Total up the deformation ...

   my @def = (0,0,0);
   foreach my $cv (@components) {
      my ($c,$mult,$zerobeyond,$ndim) = @$cv;
      next if $mult == 0;
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

package LINZ::Geodetic::LINZDeformationModel::Sequence;

sub read {
  my( $class,$format_version,$fh,$unpacker ) = @_;

   my $name = $unpacker->read_string;
   my $description = $unpacker->read_string;
   my $startdate = &LINZ::Geodetic::LINZDeformationModel::ReadDate($unpacker);
   my $enddate = &LINZ::Geodetic::LINZDeformationModel::ReadDate($unpacker);
   my $range = LINZ::Geodetic::LINZDeformationModel::Range->read($unpacker);
   my ($isvelocity,$isnested)=(0,0);
   my ($startver,$endver)=('00000000','99999999');
   $isvelocity=$unpacker->read_short if $format_version == 1;
   my ($dimension,$zerobeyond) = $unpacker->read_short(2);
   $isnested=$unpacker->read_short if $format_version > 1;
   $startver=$unpacker->read_string if $format_version > 2;
   $endver=$unpacker->read_string if $format_version > 2;
   my $ncomponents = $unpacker->read_short;
  
   my @components = ();
   
   my $self = bless {
      name => $name,
      description => $description,
      startdate => $startdate,
      enddate => $enddate,
      range => $range,
      dimension => $dimension,
      isvelocity => $isvelocity,
      isnested => $isnested,
      zerobeyond => $zerobeyond,
      startversion => $startver,
      endversion => $endver,
      enabled => $endver eq '99999999' ? 1 : 0,
      components => \@components,
      }, $class;

   if( $LINZ::Geodetic::LINZDeformationModel::debug )
   {
        my $dsc=$description;
        $dsc =~ s/\s*$//;
        $dsc =~ s/\s+/ /;
        print "Loading sequence:\n";
        print "    name=$name\n";
        print "    description=$dsc\n";
        printf "    startdate=%.3f\n",$startdate->{years};
        printf "    enddate=%.3f\n",$enddate->{years};
        printf "    range=%s\n",$range->tostring;
        print "    dimension=$dimension\n";
        print "    isvelocity=$isvelocity\n";
        print "    isnested=$isnested\n";
        print "    startversion=>$startver\n";
        print "    endversion=>$startver\n";
        print "    zerobeyond=$zerobeyond\n";
   };

   for my $icomp (1..$ncomponents) {
      my $c = LINZ::Geodetic::LINZDeformationModel::Component->read($format_version,$self,$fh,$unpacker);
      if( $c->{istrig} && $isnested )
      {
          die "Cannot handle triangulated component in nested sequence\n";
      }
      $c->{compid} = $icomp;
      push(@components,$c);
      }

   # Fix up version 1 time model which interpolate between components.
   # Note: This doesn't handle first and last components extrapolating.

   if( $format_version == 1 )
   {
      foreach my $i (1..$ncomponents-1)
      {
          if( $components[$i]->{usebefore} == 2 )
          {
              $components[$i]->{timemodel}->[0]->[0] = 
                 $components[$i-1]->{refdate}->{year};
          }
          if( $components[$i-1]->{useafter} == 2 )
          {
              $components[$i-1]->{timemodel}->[-1]->[0] = 
                 $components[$i]->{refdate}->{year};
   
          }
      }
   }

   if( $LINZ::Geodetic::LINZDeformationModel::debug )
   {
       foreach my $c (@components)
       {
           my $dsc=$c->{description};
           $dsc =~ s/\s+/ /;
           $dsc =~ s/\s*$//;
           print "    Component:\n";
           print "      description=$dsc\n";
           printf "      refdate=%.3f\n",$c->{refdate}->{years};
           printf "      range=%s\n",$c->{range}->tostring;
           print "      usebefore=$c->{usebefore}\n";
           print "      useafter=$c->{useafter}\n";
           print "      istrig=$c->{istrig}\n";
           print "      fileloc=$c->{fileloc}\n";
           print "      factor0=$c->{factor0}\n";
           foreach my $t (@{$c->{timemodel}})
           {
               printf "        time %.3f factor %.4f\n",$t->[0],$t->[1];
           }
       }
   }
   
   return $self;
   }

sub CalcComponents {
   my( $self, $date, $x, $y ) = @_;
   if( ! $self->{enabled} )
   {
      if( $LinzDeformationModel::debug ) {
         print "Sequence ",$self->{seqid}," not enabled\n";
         }
      return () ;
   }
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
   my $components=$self->{components};
   my @results=();
   foreach my $c (@$components)
   {
       next if ! $c->{range}->Includes( $x, $y );

       my $factor=$c->{factor0};
       my $tm=$c->{timemodel};
       if( @$tm && $date > $tm->[0]->[0] )
       {
           $factor=$tm->[-1]->[1];
           foreach my $i (1 .. $#$tm)
           {
               if( $date <= $tm->[$i]->[0] )
               {
                   my $y0=$tm->[$i-1]->[0];
                   my $f0=$tm->[$i-1]->[1];
                   my $y1=$tm->[$i]->[0];
                   my $f1=$tm->[$i]->[1];
                   $factor=(($y1-$date)*$f0+($date-$y0)*$f1)/($y1-$y0);
               }
           }
       }
       push( @results,[$c,$factor,$c->{zerobeyond},$self->{dimension}] );
       last if $self->{isnested};
   }
   return @results;
   }


#======================================================================

package LINZ::Geodetic::LINZDeformationModel::Component;

sub read {
   my( $class,$format_version,$sequence,$fh,$unpacker ) = @_;
   my $description = $unpacker->read_string;
   my $refdate = &LINZ::Geodetic::LINZDeformationModel::ReadDate($unpacker);
   my $range = LINZ::Geodetic::LINZDeformationModel::Range->read($unpacker);
   my ($usebefore,$useafter) = (0,0);
   my $factor0=0.0;
   my @timemodel=();
   if( $format_version == 1 )
   {
       # Convert version 1 time model to version 2 equivalent (partly here,
       # partly in Sequence.read, as need to take account of interpolation between
       # components.
      ($usebefore,$useafter) = $unpacker->read_short(2);
      if( $sequence->{isvelocity} )
      {
          my $year=$sequence->{startdate}->{years};
          my $factor=$year-$refdate->{years};
          $factor0=$factor;
          push(@timemodel,[$year,$factor]);
          push(@timemodel,[$refdate->{years},0.0]);
          $year=$sequence->{enddate}->{years};
          $factor=$year-$refdate->{years};
          push(@timemodel,[$year,$factor]);
      }
      else
      {
          my $year=$refdate->{years};
          $factor0=$usebefore == 1 ? 1.0 : 0.0;
          push(@timemodel,[$year,0.0]) if $usebefore == 2;
          push(@timemodel,[$year,1.0]) if $useafter == 2;
          push(@timemodel,[$year,$useafter == 1 ? 1.0 : 0.0]);
      }
   }
   else
   {
       my $tmtype=$unpacker->read_short;
       die "Invalid time model type $tmtype: can only use piecewise linear time model\n" if $tmtype != 1;
       my $ntimemodel=$unpacker->read_short;
       $factor0=$unpacker->read_double;
       while( $ntimemodel-- > 0 )
       {
           my $tmdate = &LINZ::Geodetic::LINZDeformationModel::ReadDate($unpacker);
           my $tmvalue = $unpacker->read_double;
           push(@timemodel,[$tmdate->{years},$tmvalue]);
       }
   }
   my $istrig = $unpacker->read_short;
   my $fileloc = $unpacker->read_long;
   
   my $self = {
      description => $description,
      refdate => $refdate,
      range => $range,
      usebefore => $usebefore,
      useafter =>$useafter,
      istrig => $istrig,
      factor0 => $factor0,
      timemodel => \@timemodel,
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
           require LINZ::Geodetic::Util::TrigFile;
           $LINZ::Geodetic::Util::TrigFile::debug = $LinzDeformationModel::debug;
           $self->{model} = newEmbedded LINZ::Geodetic::Util::TrigFile $self->{fh}, $self->{fileloc};
           }
       else {
           require LINZ::Geodetic::Util::GridFile;
           $LINZ::Geodetic::Util::GridFile::debug = $LinzDeformationModel::debug;
           $self->{model} = newEmbedded LINZ::Geodetic::Util::GridFile $self->{fh}, $self->{fileloc};
           }
       }
   if( $LinzDeformationModel::debug ) {
       print "Calculating component ".$self->{compid},"\n";
       }
   return $self->{model}->Calc($x,$y);
   }


#======================================================================

package LINZ::Geodetic::LINZDeformationModel::Range;

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

sub tostring
{
    my( $self ) = @_;
    return sprintf("%.5f,%.5f,%.5f,%.5f",@$self);
}
 
1;
