#!/usr/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Script converts an ASCII definition of a deformation
#                      model to a binary representation of the data
#
# PARAMETERS:          def_file   The name of the file defining the deformation
#                      bin_file   The name of the binary grid file generated.
#                      
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook           3/02/2004  Created
#===============================================================================
#
#

use strict;

use FindBin;
use lib $FindBin::Bin.'/perllib';

use Getopt::Std;
use Packer;

my $prgdir = $FindBin::Bin;

# Used to identify redundant triangle edges (ie zero length).

my $syntax = <<EOD;

Syntax: [options] source_file binary_file

Options are:
   -f format (either LINZDEF1B or LINZDEF1L)

EOD

my %opts;
getopts('f:',\%opts);
my $forceformat = $opts{f};

my $endianness = {
   LINZDEF1B =>
     { sig => "LINZ deformation model v1.0B\r\n\x1A",
       gridparam => '-f GRID1B',
       trigparam => '-f TRIG1B',
       bigendian => 1
       },
   LINZDEF1L =>
     { sig => "LINZ deformation model v1.0L\r\n\x1A",
       gridparam => '-f GRID1L',
       trigparam => '-f TRIG1L',
       bigendian => 0
       },
   };

my @temp_files;
my $ntmpfile = 1000;


my %model_param = qw/
    FORMAT         (LINZDEF1B|LINZDEF1L)
    VERSION_NUMBER \d+\.\d+
    VERSION_DATE   date
    START_DATE     date
    END_DATE       date
    COORDSYS       \w+
    GEOGRAPHICAL   (yes|no)?
    DESCRIPTION    .*
    /;

my %seq_param = qw/
    DIMENSION          [123]
    DATA_TYPE          (velocity|deformation)?
    START_DATE         date
    END_DATE           date
    ZERO_BEYOND_RANGE  (yes|no)?
    DESCRIPTION        .*
    /;

my %comp_param = qw/
    MODEL_TYPE         (trig|grid)
    REF_DATE           date
    BEFORE_REF_DATE    (fixed|zero|interpolate)?
    AFTER_REF_DATE     (fixed|zero|interpolate)?
    DESCRIPTION        .*
    /;

my $def_model;

eval {
   my $model = &LoadDefinition( $ARGV[0] );
   &CheckSyntax($model);
   my $format = uc($forceformat || $model->{FORMAT});
   $model->{FORMAT} = $format;
   my $endian = $endianness->{$format};
   die "Invalid model format $format specified\n" if ! $endian;
   &BuildAllComponents($model,$endian);
   
   &WriteModelBinary($ARGV[1],$model,$endian);
};
if( $@ ) {
   print STDERR $@;
   }
unlink @temp_files;



sub LoadDefinition {
   
   my ($deffile) = @_;
   
   open(DEF,"<$deffile") 
      || die "Cannot open deformation definition file $deffile\n";
   
   my $curmod;
   my $curseq;
   my $curcomp;
   my $curobj;
   my $objname;
   
   while(<DEF>) {
      next if /^\s*($|\#)/;   # Skip blank lines, comments....
      chomp;
      my ($rectype, $value ) = split(' ',$_,2);
      $rectype = uc($rectype);
      
      if( $rectype =~ /^(DEFORMATION_(MODEL|SEQUENCE|COMPONENT))$/ ) {
         if( $rectype eq 'DEFORMATION_MODEL' ) {
            die "Only one DEFORMATION_MODEL can be defined\n" if $curmod;
            $curmod = { type=>$rectype, name=>$value, params=>\%model_param,
                        sequences=>[] };
            $curobj = $curmod;
            }
         elsif( $rectype eq 'DEFORMATION_SEQUENCE' ) {
            die "Must define DEFORMATION_MODEL before $rectype\n" if ! $curmod;
            $curseq = { type=>$rectype, name=>$value, params=>\%seq_param,
                        components=>[], model=>$curmod };
            push(@{$curmod->{sequences}}, $curseq );
            $curobj = $curseq;
            }
         else {
            die "Must define DEFORMATION_SEQUENCE before $rectype\n" if ! $curseq;
            $curcomp = { type=>$rectype, source=>$value, params=>\%comp_param,
                        components=>[], sequence=>$curseq };
            push(@{$curseq->{components}}, $curcomp );
            $curobj = $curcomp;
            }
         }
   
      elsif( ! $curobj ) {
         die "DEFORMATION_MODEL must be defined first in file\n";
         }
    
      elsif( ! $curobj->{params}->{$rectype} ) {
         die "Parameter $rectype is not valid in ".$curobj->{type}."\n";
         }
   
      else {
         if( $rectype eq 'DESCRIPTION' ) {
             $value = '';
             while(<DEF>) {
               last if /^\s*end_description\s*$/i;
               $value .= $_;
               }
             }
          $curobj->{$rectype} = $value;
          }
      }
   close(DEF);
   return $curmod;
   }
   
sub TempFileName {
   my $tmpfilename;
   do {
     $tmpfilename = sprintf("deftmp%d",$ntmpfile++)
     }
   while -r $tmpfilename;

   push(@temp_files,$tmpfilename);
   return $tmpfilename;
   }


sub BuildGridComponent {
   my( $comp, $filename, $params ) = @_;
   my $origname = $filename;
   if( -T $filename ) {
     my $tmpfile = &TempFileName();
     my $command = "perl $prgdir/makegrid.pl $params $filename $tmpfile";
     system($command);
     die "Cannot create grid file from $filename\n$command\n" if ! -r $tmpfile;
     $filename = $tmpfile;
     }

   use GridFile;
   my $grid;
   eval {
      $grid = new GridFile $filename;
      };
   if( $@ ) {
      die "Cannot create or use grid file $origname\n";
      };
   if( ! $grid ) {
      die "Invalid grid in $filename\n" if ! $grid;
      };

   my $coordsys = $grid->CrdSysCode();
   my $ndim = $grid->Dimension();
   my ($xmin,$ymin,$xmax,$ymax) = $grid->Range();
   my $filesize = -s $filename;

   $comp->{sourcefile} = 
     { name=>$filename,
       origname=>$origname,
       size=>$filesize,
       coordsys=>$coordsys,
       ndim=>$ndim,
       range=>[$ymin,$ymax,$xmin,$xmax]
       };
   }

sub BuildTrigComponent {
   my( $comp, $filename, $params ) = @_;
   my $origname = $filename;
   if( -T $filename ) {
     my $tmpfile = &TempFileName();
     my $command = "perl $prgdir/maketrig.pl $params $filename $tmpfile";
     system($command);
     die "Cannot create grid file from $filename\n$command\n" if ! -r $tmpfile;
     $filename = $tmpfile;
     }

   use TrigFile;
   my $trig;
   eval {
      $trig = new TrigFile $filename;
      };
   if( $@ ) {
      die "Cannot create or use trig file $origname\n";
      };
   if( ! $trig ) {
      die "Invalid trig in $filename\n" if ! $trig;
      };

   my $coordsys = $trig->CrdSysCode();
   my $ndim = $trig->Dimension();
   my ($xmin,$ymin,$xmax,$ymax) = $trig->Range();
   my $filesize = -s $filename;

   $comp->{sourcefile} = 
     { name=>$filename,
       origname=>$origname,
       size=>$filesize,
       coordsys=>$coordsys,
       ndim=>$ndim,
       range=>[$ymin,$ymax,$xmin,$xmax]
       };
   }

sub BuildComponent {
   my ($comp,$endian) = @_;
   my $source = $comp->{source};
   my ($filename, $params) = split(' ',$source,2);
   die "Cannot find component source file $filename\n" if ! -r $filename;

   my $type = $comp->{MODEL_TYPE};
   if( $type eq 'grid' ) {
      &BuildGridComponent($comp,$filename,$params.' '.$endian->{gridparam});
      }
   elsif( $type eq 'trig' ) {
      &BuildTrigComponent($comp,$filename,$params.' '.$endian->{trigparam});
      }
   else {
      die "Invalid component MODEL_TYPE $type\n";
      }
   }
    
   
sub CheckDate {
   my ($date) = @_;
   return 0 if $date !~
     /^([0-3]?\d)\-
       (jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)\-
       ([12]\d\d\d)
       (?:\s+([0-2]\d)\:([0-5]\d)\:[0-5]\d)?$/ix;
   return 0 if $1 < 1 || $1 > 31 || $4 > 23;
   return 1;
   }

sub CheckObject {
   my ($obj) = @_;
   my $nerror = 0;
   my $params = $obj->{params};
   foreach my $k (sort keys %$params) {
      my $pattern = $params->{$k};
      my $value = $obj->{$k};
      my $result;
      if( $pattern eq 'date' ) {
        $result = &CheckDate($value);
        }
      else {
        $result = $value =~ /^$pattern$/s;
        }
      if( ! $result ) {
        print $value eq '' ? "Missing value" : "Invalid value $value",
              " for $k in ",$obj->{type},"\n";
        $nerror++;
        }
      }
   return $nerror;
   }

sub ExpandRange {
   my ($oldrange,$newrange) = @_;
   if( ! $oldrange ) {
       $oldrange = [@$newrange];
       }
   else {
       if( $oldrange->[0] > $newrange->[0] ) { $oldrange->[0] = $newrange->[0];}
       if( $oldrange->[1] < $newrange->[1] ) { $oldrange->[1] = $newrange->[1];}
       if( $oldrange->[2] > $newrange->[2] ) { $oldrange->[2] = $newrange->[2];}
       if( $oldrange->[3] < $newrange->[3] ) { $oldrange->[3] = $newrange->[3];}
       }
   return $oldrange;
   }
 

sub BuildAllComponents {
   my ($model,$endian) = @_;
   
   foreach my $s (@{$model->{sequences}}) {
      foreach my $c (@{$s->{components}}) {
         &BuildComponent($c,$endian);
         if( $c->{sourcefile}->{ndim} != $s->{DIMENSION} ) {
            die "Component ".$c->{sourcefile}->{origname}." has incorrect dimension\n";
            }
         $s->{range} = &ExpandRange( $s->{range}, $c->{sourcefile}->{range} );
         }
      $model->{range} = &ExpandRange( $model->{range}, $s->{range} );
      }
   }

sub CheckSyntax {
   my ($model) = @_;
   
   my $nerror = &CheckObject( $model );
 
   my $nsequences = scalar(@{$model->{sequences}});
   if( $nsequences < 1 ) {
      print "Deformation model has no sequences\n";
      $nerror++;
      }
   $model->{nsequences} = $nsequences;
   foreach my $s (@{$model->{sequences}}) {
      $nerror += &CheckObject($s);
      my $ncomponents = scalar(@{$s->{components}});
      if( $ncomponents < 1 ) {
         print "Deformation sequence has no components\n";
         $nerror++;
         }
      $s->{ncomponents} = $ncomponents;
      foreach my $c (@{$s->{components}}) {
         $nerror += &CheckObject($c);
         }
      }
  
   die "Failed with invalid syntax\n" if $nerror;
   }

   
sub PackDate {
   my ($pack,$date) = @_;
   
   my $month = {
       jan=>1, feb=>2, mar=>3, apr=>4, may=>5, jun=>6, 
       jul=>7, aug=>8, sep=>9, oct=>10, nov=>11, dec=>12 };

   $date =~
     /^([0-3]?\d)\-
       (jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)\-
       ([12]\d\d\d)
       (?:\s+([0-2]\d)\:([0-5]\d)\:[0-5]\d)?$/ix;
   
   return $pack->short($3+0,$month->{lc($2)},$1+0,$4+0,$5+0,$6+0);
   }

sub WriteModelBinary {
   my($binfile,$model,$endian) = @_;
   my $pack = new Packer( $endian->{bigendian} );

   open(OUT,">$binfile") || die "Cannot create output file $binfile\n";
   binmode(OUT);

   # Print out file signature...

   print OUT $endian->{sig};
   my $indexloc = tell(OUT);
   print OUT $pack->long(0);

   # Print out component data ...

   foreach my $s (@{$model->{sequences}}) {
     foreach my $c (@{$s->{components}}) {
       my $sf = $c->{sourcefile};
       open(SF,"<$sf->{name}") || 
          die "Cannot open component souce file $sf->{name}\n";
       binmode(SF);
       $sf->{loc} = tell(OUT);
       my $buf;
       sysread(SF,$buf,$sf->{size});
       print OUT $buf;
       close(SF);
       }
     }

   # Print out model, sequences, components ... 

   my %method = ( zero=>0, fixed=>1, interpolate=>2 );
   
   my $loc = tell(OUT);

   print OUT $pack->string( @{$model}{qw/name VERSION COORDSYS DESCRIPTION/});
   print OUT &PackDate($pack,$model->{VERSION_DATE});
   print OUT &PackDate($pack,$model->{START_DATE});
   print OUT &PackDate($pack,$model->{END_DATE});
   print OUT $pack->double( @{$model->{range}} );
   print OUT $pack->short( lc($model->{GEOGRAPHICAL}) eq 'no' ? 0 : 1 );
   print OUT $pack->short( $model->{nsequences} );
                          
   foreach my $s (@{$model->{sequences}}) {
     print OUT $pack->string( @{$s}{qw/name DESCRIPTION/});
     print OUT &PackDate($pack,$s->{START_DATE});
     print OUT &PackDate($pack,$s->{END_DATE});
     print OUT $pack->double( @{$s->{range}} );
     print OUT $pack->short( lc($s->{DATA_TYPE}) eq 'velocity' ? 1 : 0 );
     print OUT $pack->short( $s->{DIMENSION} );
     print OUT $pack->short( lc($s->{ZERO_BEYOND_RANGE}) eq 'no' ? 0 : 1 );
     print OUT $pack->short( $s->{ncomponents} );

     foreach my $c (@{$s->{components}}) {
       print OUT $pack->string( $c->{DESCRIPTION} );
       print OUT &PackDate($pack,$c->{REF_DATE});
       print OUT $pack->double( @{$c->{sourcefile}->{range}} );
       print OUT $pack->short( 0 + $method{lc($c->{BEFORE_REF_DATE}) || 'interpolate' } );
       print OUT $pack->short( 0 + $method{lc($c->{AFTER_REF_DATE}) || 'interpolate' } );
       print OUT $pack->short( lc($c->{MODEL_TYPE}) eq 'trig' ? 1 : 0 );
       print OUT $pack->long( $c->{sourcefile}->{loc} );
       }
     }

  seek(OUT,$indexloc,0);
  print  OUT $pack->long($loc);

  close(OUT);
  }

__END__

# Example of an input file ...
#
# The model consists of one or more deformation sequences, each of which 
# comprises one or more deformation components.  Each sequence can define
# 1, 2, or 3d deformation, and can define velocities or deformation.  The
# sequence defines a range of dates for which it applies.  It also specifies
# whether it is valid to assume that the deformation is zero for points 
# beyond the range of the sequence, or whether points beyond the range are
# to be treated as invalid points for calculating deformation.

# A velocity sequence consists of one or more velocity models, each with a 
# reference date.  They can be extrapolated before or after the reference
# date, or both.  All velocities in the sequence that are valid for the
# evaluation date are applied.

# A deformation sequence consists of one or more deformation models in order
# of date.  Each model can apply as a fixed or interpolated model before its
# reference date and after its reference date.  For dates between two models,
# the valid options are that the deformation is interpolated between the two
# models, or one or other model applies as a fixed model.  Times before the
# first model or after the last can be extrapolated based upon the first or
# last two models respectively.

# Each component in the sequence can be either a triangulated model, or 
# a grid model.  The source file referenced can be a pre-built binary file,
# or more usefully an ascii source file from which the binary can be built.
# The latter is preferable, as it will ensure that all components are built
# with the same endian-ness.

# Two example sequences follow, one with a single velocity grid, the other
# with two triangulated deformation components.


DEFORMATION_MODEL NZGD2000 deformation model
# Format is LINZDEF1L or LINZDEF1B, depending upon endian-ness.  Can be
# over-ridden by command line parameter to script.
FORMAT LINZDEF1B
VERSION_NUMBER 1.0
VERSION_DATE  12-Mar-2004
# Calculation will fail for values outside range start date to end date
START_DATE 1-Jan-1850
END_DATE 1-Jan-2100
COORDSYS NZGD2000
DESCRIPTION
This is the description of the model
This is a first try
END_DESCRIPTION

DEFORMATION_SEQUENCE National model
DATA_TYPE velocity
DIMENSION 2
START_DATE 1-Jan-1850
END_DATE 1-Jan-2100
ZERO_BEYOND_RANGE no
DESCRIPTION
The IGNS velocity model.  Based upon data from 1992 to 1997
Report number 12345
END_DESCRIPTION

DEFORMATION_COMPONENT igns98b.dmp
MODEL_TYPE grid
REF_DATE 1-Jan-2000
# BEFORE_REF_DATE and AFTER_REF_DATE can have values 'zero', 'fixed', 
# and 'interpolate'.  The latter two are equivalent for velocity sequences.
BEFORE_REF_DATE interpolate
AFTER_REF_DATE interpolate
DESCRIPTION
END_DESCRIPTION

# Discrete event ...

DEFORMATION_SEQUENCE Patch
DATA_TYPE deformation
DIMENSION 2
START_DATE 30-Jun-2002
END_DATE 1-Jan-2100
ZERO_BEYOND_RANGE yes
DESCRIPTION
Totally ficticious deformation model ... 
Report number 456/7
END_DESCRIPTION

DEFORMATION_COMPONENT patch1.trg
MODEL_TYPE trig
REF_DATE 30-Jun-2002 13:40:00
BEFORE_REF_DATE zero
AFTER_REF_DATE interpolate
DESCRIPTION
Initial offset
END_DESCRIPTION

DEFORMATION_COMPONENT patch2.trg
MODEL_TYPE trig
REF_DATE 30-Jun-2003 13:40:00
BEFORE_REF_DATE interpolate
AFTER_REF_DATE fixed
DESCRIPTION
Initial offset
END_DESCRIPTION
