@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
perl -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl
:WinNT
perl -x -S "%0" %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
goto endofperl
@rem ';
#!perl
#line 14
###############################################################################
# Program:           gpstrans.bat
#
# Description:       This programs converts GPS baseline summary files
#                    between different formats.  Currently accepted input
#                    formats are GPSurvey and Ski summary files, and 
#                    output formats are SNAP files and LINZ contract format
#                    GPS observation files.
#
# Dependencies:      Uses the following modules directly:
#                      GPS::GPSurveyFile
#                      GPS::SkiFile
#                      Geodetic::GeodeticCrd
#                      Getopt::Std
#                      FileHandle
#
# $Id: gpstrans.bat,v 1.4 2000/11/01 03:40:09 ccrook Exp $
#
# $Log: gpstrans.bat,v $
# Revision 1.4  2000/11/01 03:40:09  ccrook
# Fixed error in script where the perl library name was incorrectly specified as
# perlib!
#
# Revision 1.3  2000/06/13 00:12:09  ccrook
# Modified to ensure filenames use backslashes in defining library directory.
#
# Revision 1.2  2000/04/06 20:54:49  ccrook
# Removed a spurious carriage return from the end of every line.
#
# Revision 1.1  2000/03/30 09:04:22  chris
# Adding scripts generated for SNAP upgrade.
#
#
###############################################################################

use strict;
use FindBin;
use lib "$FindBin::Bin/perllib";

use vars qw/ %ClassAccuracy %GeneratedCode %CRSClass $NextCode $scriptfile/;

# Use "require" rather than "use" to facilitate testing.

eval {
   require GPS::GPSurveyFile;
   require GPS::SkiFile;
   require Geodetic::GeodeticCrd;
  };
if( $@ ) {
  print "Unable to locate required perl libraries\n";
  print join("\n   ","Libraries not in any of:",@INC),"\n";
  exit;
  }

#----------------------------------------------------------------------
# Set up some constants used by the program

%ClassAccuracy = (
   1 => "1.2 1.2 1.5 mm 0.04 0.04 0.15 ppm",
   2 => "1.2 1.2 5.1 mm 0.4 0.4 1.5 ppm",
   3 => "4.1 4.1 5.1 mm 1.2 1.2 5.1 ppm",
   4 => "4.1 4.1 5.1 mm 4.1 4.1 15.3 ppm",
   5 => "4.1 4.1 5.1 mm 12 12 50 ppm"
   );

%CRSClass = (
   1 => "B100",
   2 => "M1",
   3 => "M3",
   4 => "M10",
   5 => "M30" );

my $syntax = <<EOD;

Syntax: gpstrans [options] gpsfile output_filename

Note: The output file name should not include an extension.

The gpsfile parameter is the name of an ASCII summary output 
file.  Currently GPSurvey format summary files and Leica format
vector files are accepted.

Options can include:

   -c #     Defines the class of the observations.  This 
            is used to specify the accuracy.  If the class
            is not defined then the GPS baseline solution 
            accuracy is used.  Valid classes are 1-5.

   -o       Create a LINZ contract format file instead of
            SNAP files.

EOD

#----------------------------------------------------------------------
# Read the parameters from the command line.

use Getopt::Std;
my %options;
getopts('c:C:oO',\%options);
my $makeObsFile = $options{'o'} || $options{'O'};
my $class = $options{'c'}.$options{'C'};
if( $class ne '' && ! exists($ClassAccuracy{$class}) ) {
   die "Invalid class specified for data\n";
   };

die $syntax if @ARGV != 2;
my ($gpsfile, $snapfile ) = @ARGV;

die "$gpsfile does not exist\n" if ! -r $gpsfile;

#----------------------------------------------------------------------
# Try opening the input file for each supported format

my $gpsf;

eval { $gpsf = new GPS::GPSurveyFile( $gpsfile ); };
eval { $gpsf = new GPS::SkiFile( $gpsfile ); } if ! $gpsf;
die "Cannot recognize the format of $gpsfile\n" if ! $gpsf;

#----------------------------------------------------------------------
# Create the output file as either a GPSObsFile object or a SNAPJob object

my $of;

if( $makeObsFile ) {
   $of = new GPSObsFile($snapfile,$CRSClass{$class});
   }
else {
   my $project = $gpsf->{project};
   $project = "Data from $gpsfile" if ! $project;
   $of = new SNAPJob( $snapfile, $project, $class, $gpsf->{project} );
   if( $class ne '' ) {
      my $accuracy = $ClassAccuracy{$class};
      my @components = $accuracy =~ /([\d\.]+)/g;
      $of->SetAccuracy( @components );
      $of->SetClassification( 'class', 'M'.$class );
      };
   }
      
#----------------------------------------------------------------------
# Copy the observations from the input file  to the output file.

if( $of ) {
   my $obs;
   while( $obs = $gpsf->GetObs ) {
      &CheckObs($obs);
      $of->WriteObs( $obs );
      }
   $of->Summarise;
   }



#===============================================================================
#
#   Subroutine:   CheckObs
#
#   Description:  Checks that the data in a GPS obs file is OK.  Mainly
#                 generates station codes if these are not defined.  For
#                 GPSurvey data compiles some of the solution attributes
#                 into "snapnote", used to annotate the observation in
#                 SNAP observation files.
#
#   Parameters:   obs        The observation as a hash reference.
#
#   Returns:      The modified observation.
#
#   Globals:      
#===============================================================================

sub CheckObs {
   my ($obs) = @_;
   # Ensure that stations have codes...
   &GenerateCode($obs->{from});
   &GenerateCode($obs->{to});
   if( $obs->{GPSurvey} ) {
     my $gpsrvy = $obs->{GPSurvey};
     my $snapnote = "Dur: ".$gpsrvy->{duration};
     $snapnote .= "   Eph: ".$gpsrvy->{ephemeris};
     $snapnote .= "   Amb: ".$gpsrvy->{status};
     my $type = $gpsrvy->{type};
     $type =~  s/double difference/DD/i;
     $type =~  s/triple difference/TD/i;
     $type =~  s/code phase/DPR/i;
     $snapnote .= "   Type: ".$type;
     $obs->{snapnote} = $snapnote;
     }
   return $obs;
   }


#===============================================================================
#
#   Subroutine:   GenerateCode
#
#   Description:  Generates a code for stations used in GPS observations.
#                 
#                 generates station codes if these are not defined.  For
#                 GPSurvey data compiles some of the solution attributes
#                 into "snapnote", used to annotate the observation in
#                 SNAP observation files.
#
#   Parameters:   $stn       The station as a hash reference
#
#   Returns:      The modified observation.
#
#   Globals:      %GeneratedCode    A hash array of codes that have been
#                                   generated already, keyed on name.
#                 $NextCode         The next code to allocate
# 
#===============================================================================

sub GenerateCode{
   my( $stn ) = @_;
   my $code = $stn->{code};
   my $name = $stn->{name};
   $stn->{name} = $code if $name eq '';
   return if $code =~ /^[A-Z0-9_]{1,10}$/i;
   $code = $name if $code eq '';
   my $refcode = uc($code);
   $code = $GeneratedCode{$refcode} if exists $GeneratedCode{$refcode};
   $code = $1 if $code eq ''  && $name =~ /^([A-Z0-9]{4})(?:$|\W)/;
   $code = ++$NextCode if $code eq '';
   $GeneratedCode{uc($refcode)} = $code;
   # Should really check here that the code isn't already allocated, but
   # to do this properly need also to check already assigned codes, and then
   # what we allocate them before we find they are already in use....
   $stn->{code} = $code;
   }
     
     

######################################################################
# Package for output of a SNAP file

package SNAPJob;

sub new {
  my ($class, $filename, $title, $obsclass) = @_;
  $title = "Snap job" if ! $title;
  
  my $obsfh = new FileHandle ">$filename.dat";
  die "Cannot create SNAP data file $filename.dat\n" if ! $obsfh;
  my $crdfh = new FileHandle ">$filename.crd";
  die "Cannot create SNAP coordinate file $filename.crd\n" if ! $crdfh;
  my $cmdfh = new FileHandle ">$filename.snp";
  die "Cannot create SNAP command file $filename.dat\n" if ! $cmdfh;

  print $obsfh "$title\n";

  print $crdfh "$title\nNZGD2000\noptions no_geoid\n\n";

  print $cmdfh "title $title\n";
  print $cmdfh "coordinate_file $filename.crd\n";
  print $cmdfh "data_file $filename.dat\n";

  if( $obsclass ne ''  ) {
     print $cmdfh "configuration gpstest\n";
     print $cmdfh "test_specification order_$obsclass all\n";
     }
  else {
     print $cmdfh "mode 3d adjustment\n";
     };

  my $self = { 
     filename=>$filename, 
     obsfh=>$obsfh, 
     crdfh=>$crdfh,
     cmdfh=>$cmdfh,
     dataspec=>'', 
     date=>'',
     fixeerr=>'',
     stnlist=>{},
     nobs=>0,
     nstn=>0 };
  return bless $self, $class;
  }

sub DESTROY {
  my($self) = @_;
  $self->WriteStations if ! $self->{stationswritten};
  }

sub Comment {
  my( $self, @msg ) = @_;
  my $obsfh = $self->{obsfh};
  foreach (@msg) { 
     print $obsfh "! $_\n";
     }
  }

sub SetClassification {
  my( $self, $cls, $clsval ) = @_;
  my $obsfh = $self->{obsfh};
  print $obsfh "\n#classify $cls $clsval\n";
  }

sub SetAccuracy {
  my ($self,$emm,$nmm,$umm,$eppm,$nppm,$uppm) = @_;
  my $obsfh = $self->{obsfh};
  print $obsfh "\n#gps_enu_error $emm $nmm $umm mm $eppm $nppm $uppm ppm\n";
  $self->{fixederr} = 1;
  $self->{dataspec} = '';
  }

sub WriteObs {
  my( $self, $obs ) = @_;
  my $obsfh = $self->{obsfh};
  if( ! $self->{dataspec} ) {
    my $spec = '#data GPS ';
    if( ! $self->{fixederr} ) { $spec .= 'error ' };
    $spec .= 'no_heights';
    print $obsfh "\n$spec\n";
    $self->{dataspec} = $spec;
    print $obsfh "\n#gps_error_type full\n" if ! $self->{fixederr};
    }
  my $date = $obs->{date};
  if( $date eq '' ) {
    $date = 'unknown';
    }
  else {
    my ($day,$month,$year) = $date =~ /(\d+)\D+(\d+)\D+(\d+)/;
    $month = (qw/Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec/)[$month-1];
    $date = "$day $month $year";
    }
  if( $date ne $self->{lastdate} ) {
     $self->{lastdate} = $date;
     print $obsfh "\n#date $date\n";
     }

  if( $obs->{snapnote} ) {
     print $obsfh "\n#note $obs->{snapnote}";
     };

  printf $obsfh "\n%-5s %-5s %10.4lf %10.4lf %10.4lf",$obs->{from}->{code},
     $obs->{to}->{code},@{$obs->{vector}};
  if( ! $self->{fixederr} ) {
     printf $obsfh " &\n      %15.12lf %15.12lf %15.12lf".
                   " &\n      %15.12lf %15.12lf %15.12lf",
          @{$obs->{covar}};
     }
   print $obsfh "\n";

   $self->AddStation( $obs->{from} );
   $self->AddStation( $obs->{to} );
   $self->{nobs}++;
   }

sub AddStation {
   my( $self, $stn ) = @_;
   my $stnlist = $self->{stnlist};
   my $code = $stn->{code};
   if ( ! exists $stnlist->{$code} ) {
      $stnlist->{$code} = $stn; 
      $self->{nstn}++;
      }
   }
   
sub WriteStations {
   my ($self) = @_;
   my $crdfh = $self->{crdfh};
   my @codes = sort {&CmpCode($a,$b);} keys %{$self->{stnlist}};
   my @fixstn;
   foreach( @codes ) {
      my $stn = $self->{stnlist}->{$_};
      push( @fixstn, $_ ) if $stn->{fixed};
      my $crd = new Geodetic::GeodeticCrd($stn->{crd});
      my $dmscrd = $crd->asstring( 6, 3 );
      print $crdfh "$stn->{code} $dmscrd->[0] $dmscrd->[1] ",
                   "$dmscrd->[2] $stn->{name}\n";
      }
   push(@fixstn,$codes[0]) if ! @fixstn;
   my $cmdfh = $self->{cmdfh};
   my $nfix = 0;
   while( $nfix < @fixstn ) {
     my $nmax = $nfix+9;
     $nmax = $#fixstn if $nmax > $#fixstn;
     print $cmdfh "fix ",join(" ",@fixstn[$nfix..$nmax]),"\n";
     $nfix = $nmax + 1;
     }
   print $cmdfh "\n";
   print $cmdfh "! Too few fixed stations to calculate reference frame.\n!"
     if @fixstn < 3;
   print $cmdfh "reference_frame GPS calculate scale rotation\n";
   $self->{stationswritten} = 1;
   }

sub CmpCode {
   my($a, $b) = @_;
   my $result = $a <=> $b;
   $result = $a cmp $b if ! $result;
   return $result;
   }

sub Summarise {
   my( $self ) = @_;
   $self->WriteStations();
   my $filename = $self->{filename};
   print "The snap job generated consists of:\n";
   print "    Command file:    $filename.cmd\n";
   print "    Coordinate file: $filename.crd ($self->{nstn} stations)\n";
   print "    Data file:       $filename.dat ($self->{nobs} baselines)\n";
   }


package GPSObsFile;

sub new {
  my ($class, $filename, $crsclass ) = @_;
  
  my $fh = new FileHandle ">$filename.csv";
  die "Cannot create observation file $filename.csv\n" if ! $fh;

  # print $fh "FromCode,ToCode,Date,Time,dX,dY,dZ,Class\n";
  print $fh "FCODE,TCODE,DATE,TIME,DX,DY,DZ,ROBG,COMM\n";

  my $self = { 
     filename=>$filename, 
     fh=>$fh,
     nobs=>0,
     crsclass=>$crsclass
     };
  return bless $self, $class;
  }

sub DESTROY {
  my($self) = @_;
  my($fh) = $self->{fh};
  close($fh);
  }

sub WriteObs {
  my( $self, $obs ) = @_;
  my $fh = $self->{fh};
  my $date;
  my $time;
  my $start_time = $obs->{date};
  if( defined($start_time) ) {
    ($time) = $start_time =~ /(\d+\:\d+)\:\d+(?:\.\d+)?/;
    $time =~ s/\:/\./;
    $date = &GPS::DateToDayNumber($start_time);
  }
  printf $fh "%s,%s,%s,%s,%lf,%lf,%lf,%s,\n",
         $obs->{from}->{code}, $obs->{to}->{code},
         $date,$time,
         @{$obs->{vector}},
         $self->{crsclass};

  $self->{nobs}++;
  }

sub Summarise {
  my( $self ) = @_;
  my $fh = $self->{fh};
  close($fh);
  printf "\n%d observations written to LINZ format file %s.csv\n",
     $self->{nobs}, $self->{filename};
  }

__END__
:endofperl
