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
use lib $FindBin::RealBin.'/perllib';

use Getopt::Std;
use Packer;

my $prgdir = $FindBin::RealBin;

# Used to identify redundant triangle edges (ie zero length).

my $syntax = <<EOD;

Syntax: [options] source_file binary_file

Options are:
   -f format (either LINZDEF1B, LINZDEF1L, LINZDEF1B, LINZDEF2L)

EOD

my %opts;
getopts('f:',\%opts);
my $forceformat = $opts{f};

my $endianness = {
   LINZDEF1B =>
     { sig => "LINZ deformation model v1.0B\r\n\x1A",
       gridparam => '-f GRID2B',
       trigparam => '-f TRIG2B',
       bigendian => 1,
       multiversion => 0,
       },
   LINZDEF1L =>
     { sig => "LINZ deformation model v1.0L\r\n\x1A",
       gridparam => '-f GRID2L',
       trigparam => '-f TRIG2L',
       bigendian => 0,
       multiversion => 0,
       },
   LINZDEF2B =>
     { sig => "LINZ deformation model v2.0B\r\n\x1A",
       gridparam => '-f GRID2B',
       trigparam => '-f TRIG2B',
       bigendian => 1,
       multiversion => 0,
       },
   LINZDEF2L =>
     { sig => "LINZ deformation model v2.0L\r\n\x1A",
       gridparam => '-f GRID2L',
       trigparam => '-f TRIG2L',
       bigendian => 0,
       multiversion => 0,
       },
   LINZDEF3B =>
     { sig => "LINZ deformation model v3.0B\r\n\x1A",
       gridparam => '-f GRID2B',
       trigparam => '-f TRIG2B',
       bigendian => 1,
       multiversion => 1,
       },
   LINZDEF3L =>
     { sig => "LINZ deformation model v3.0L\r\n\x1A",
       gridparam => '-f GRID2L',
       trigparam => '-f TRIG2L',
       bigendian => 0,
       multiversion => 1,
       },
   };

my @temp_files;
my $ntmpfile = 1000;


my %model_param = qw/
    FORMAT         (LINZDEF[123][BL])
    VERSION_NUMBER \d+(\.\d+)?
    VERSION_DATE   date
    START_DATE     date
    END_DATE       date
    COORDSYS       \w+
    GEOGRAPHICAL   (yes|no)?
    DESCRIPTION    .*
    /;

my %model_param3 = qw/
    FORMAT         (LINZDEF[123][BL])
    START_DATE     date
    END_DATE       date
    COORDSYS       \w+
    GEOGRAPHICAL   (yes|no)?
    /;

my %version_param3 = qw/
    VERSION_DATE   date
    DESCRIPTION    .*
    /;

my %seq_param1 = qw/
    DIMENSION          [123]
    DATA_TYPE          (velocity|deformation)?
    START_DATE         date
    END_DATE           date
    ZERO_BEYOND_RANGE  (yes|no)?
    DESCRIPTION        .*
    /;

my %seq_param2 = qw/
    DIMENSION          [123]
    START_DATE         date
    END_DATE           date
    ZERO_BEYOND_RANGE  (yes|no)?
    NESTED_SEQUENCE    (yes|no)?
    DESCRIPTION        .*
    /;

my %seq_param3 = qw/
    DIMENSION          [123]
    START_DATE         date
    END_DATE           date
    ZERO_BEYOND_RANGE  (yes|no)?
    NESTED_SEQUENCE    (yes|no)?
    VERSION_START      (20\d\d[01]\d[0123]\d)
    VERSION_END        (20\d\d[01]\d[0123]\d|0|99999999)
    DESCRIPTION        .*
    /;

my %comp_param1 = qw/
    MODEL_TYPE         (trig|grid)
    REF_DATE           date
    BEFORE_REF_DATE    (fixed|zero|interpolate)?
    AFTER_REF_DATE     (fixed|zero|interpolate)?
    DESCRIPTION        .*
    /;

my %comp_param2 = qw/
    MODEL_TYPE         (trig|grid)
    REF_DATE           date
    TIME_MODEL         (PIECEWISE_LINEAR\s+float(\s+date\s+float)*|VELOCITY(\s+float(\s+date\s+float)*)?)
    DESCRIPTION        .*
    /;

my $format_version={
    LINZDEF1L=>'1',
    LINZDEF1B=>'1',
    LINZDEF2L=>'2',
    LINZDEF2B=>'2',
    LINZDEF3L=>'3',
    LINZDEF3B=>'3',
    };

my $version_parameters={
    1=>{model=>\%model_param,version=>undef, sequence=>\%seq_param1,component=>\%comp_param1},
    2=>{model=>\%model_param,version=>undef,sequence=>\%seq_param2,component=>\%comp_param2},
    3=>{model=>\%model_param3,version=>\%version_param3,sequence=>\%seq_param3,component=>\%comp_param2},
    };
    


my $def_model;

eval {
   my $model = &LoadDefinition( $ARGV[0] );
   my $format = uc($forceformat || $model->{FORMAT});
   $model->{FORMAT} = $format;
   my $endian = $endianness->{$format};
   die "Invalid model format $format specified\n" if ! $endian;
   &CheckSyntax($model);
   &BuildAllComponents($model,$endian);
   if( $model->{format_version} eq '1' )
   {
      &WriteModelBinaryV1($ARGV[1],$model,$endian);
   }
   else
   {
      &WriteModelBinaryV23($ARGV[1],$model,$endian);
   }
    
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
   my $seq_param=undef;
   my $comp_param=undef;
   my $ver_param=undef;
   
   while(<DEF>) {
      next if /^\s*($|\#)/;   # Skip blank lines, comments....
      chomp;
      my ($rectype, $value ) = split(' ',$_,2);
      $rectype = uc($rectype);
      
      if( $rectype =~ /^(DEFORMATION_(MODEL|SEQUENCE|COMPONENT)|VERSION)$/ ) {
         if( $rectype eq 'DEFORMATION_MODEL' ) {
            die "Only one DEFORMATION_MODEL can be defined\n" if $curmod;
            $curmod = { type=>$rectype, name=>$value, params=>\%model_param,
                        sequences=>[] };
            $curobj = $curmod;
            }
         elsif( defined $ver_param && $rectype eq 'VERSION' ) {
            die "Must define DEFORMATION MODEL before $rectype\n" if ! $curmod;
            die "Invalid version number $value\n" if $value !~ /^20\d\d[01]\d[0123]\d$/;
            $curobj = { version=>$value, params=>$ver_param, model=>$curmod };
            push(@{$curmod->{versions}},$curobj);
            }
         elsif( $rectype eq 'DEFORMATION_SEQUENCE' ) {
            die "Must define DEFORMATION_MODEL before $rectype\n" if ! $curmod;
            $curseq = { type=>$rectype, name=>$value, params=>$seq_param,
                        components=>[], model=>$curmod };
            push(@{$curmod->{sequences}}, $curseq );
            $curobj = $curseq;
            }
         else {
            die "Must define DEFORMATION_SEQUENCE before $rectype\n" if ! $curseq;
            $curcomp = { type=>$rectype, source=>$value, params=>$comp_param,
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
          if( $rectype eq 'FORMAT' )
          {
              my $version=$format_version->{$value};
              my $version_params=$version_parameters->{$version};
              $curmod->{params}=$version_params->{model};
              $ver_param=$version_params->{version};
              if( $ver_param )
              {
                  $curmod->{versions}=[];
              }
              $seq_param=$version_params->{sequence};
              $comp_param=$version_params->{component};
              $curmod->{format_version}=$version;
          }
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
   
sub CheckObject {
   my ($obj) = @_;
   my $nerror = 0;
   my $params = $obj->{params};
   my $datere='(?:[0-2]?[1-9]|10|20|30|31)\-
       (?:jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)\-
       [12]\d\d\d
       (?:(?:\s+|T)(?:[0-1]\d|2[0-3])\:[0-5]\d\:[0-5]\d)?';
   $datere=~s/\s//g;
   my $floatre='\-?\d+(?:\.\d+)?';

   foreach my $k (sort keys %$params) {
      my $pattern = $params->{$k};
      my $value = $obj->{$k};
      my $result;
      $pattern =~ s/date/$datere/g;
      $pattern =~ s/float/$floatre/g;
      $result = $value =~ /^$pattern$/is;
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

   if( exists $model->{versions} ) {
       my $nversions = scalar(@{$model->{versions}});
       if( $nversions < 1 )
       {
           print "Deformation model has no version number\n";
           $nerror++;
       }
       $model->{nversions} = $nversions;
   }
 
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

   
sub ParseDate {
   my ($date) = @_;
   
   my $month = {
       jan=>1, feb=>2, mar=>3, apr=>4, may=>5, jun=>6, 
       jul=>7, aug=>8, sep=>9, oct=>10, nov=>11, dec=>12 };

   $date =~
     /^([0-3]?\d)\-
       (jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)\-
       ([12]\d\d\d)
       (?:\s+([0-2]\d)\:([0-5]\d)\:[0-5]\d)?$/ix;
   
   return [$3+0,$month->{lc($2)},$1+0,$4+0,$5+0,$6+0];
   }

# Extracted from Time::JulianDay (as not available on all systems)
# calculate the julian day, given $year, $month and $day
sub julian_day
{
    my($year, $month, $day) = @_;
    my($tmp);

    use Carp;
#    confess() unless defined $day;

    $tmp = $day - 32075
      + 1461 * ( $year + 4800 - ( 14 - $month ) / 12 )/4
      + 367 * ( $month - 2 + ( ( 14 - $month ) / 12 ) * 12 ) / 12
      - 3 * ( ( $year + 4900 - ( 14 - $month ) / 12 ) / 100 ) / 4
      ;

    return($tmp);

}
sub DateToYear {
   my ($date)=@_;
   my $dateparts=ParseDate($date);
   my ($y,$m,$d,$hr,$mn,$sc) = @$dateparts;
   my $y0 = julian_day($y,1,1);
   my $y1 = julian_day($y+1,1,1);
   my $yd = julian_day($y,$m,$d)+ ($hr+$mn/60+$sc/3600)/24;
   return $y + ($yd-$y0)/($y1-$y0);
}
   
sub PackDate {
   my ($pack,$date) = @_;
   my $dateparts=ParseDate($date);
   return $pack->short(@$dateparts);
   }

sub WriteModelBinaryV1 {
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

   print OUT $pack->string( @{$model}{qw/name VERSION_NUMBER COORDSYS DESCRIPTION/});
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

sub WriteModelBinaryV23 {
   my($binfile,$model,$endian) = @_;
   my $version=$model->{format_version}; 
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

   if( $version >= 3  )
   {
       print OUT $pack->string( @{$model}{qw/name COORDSYS/});
       print OUT &PackDate($pack,$model->{START_DATE});
       print OUT &PackDate($pack,$model->{END_DATE});
       print OUT $pack->double( @{$model->{range}} );
       print OUT $pack->short( lc($model->{GEOGRAPHICAL}) eq 'no' ? 0 : 1 );
       print OUT $pack->short( $model->{nversions} );
       foreach my $v (@{$model->{versions}})
       {
          print OUT $pack->string( $v->{version} );
          print OUT &PackDate($pack,$v->{VERSION_DATE} );
          print OUT $pack->string( $v->{DESCRIPTION} );
       }
   }
   else
   {
       print OUT $pack->string( @{$model}{qw/name VERSION_NUMBER COORDSYS DESCRIPTION/});
       print OUT &PackDate($pack,$model->{VERSION_DATE});
       print OUT &PackDate($pack,$model->{START_DATE});
       print OUT &PackDate($pack,$model->{END_DATE});
       print OUT $pack->double( @{$model->{range}} );
       print OUT $pack->short( lc($model->{GEOGRAPHICAL}) eq 'no' ? 0 : 1 );
   }
                          
   print OUT $pack->short( $model->{nsequences} );
   foreach my $s (@{$model->{sequences}}) {
     print "Building sequence ",$s->{name},"\n";
     print OUT $pack->string( @{$s}{qw/name DESCRIPTION/});
     print OUT &PackDate($pack,$s->{START_DATE});
     print OUT &PackDate($pack,$s->{END_DATE});
     print OUT $pack->double( @{$s->{range}} );
     # print OUT $pack->short( lc($s->{DATA_TYPE}) eq 'velocity' ? 1 : 0 );
     print OUT $pack->short( $s->{DIMENSION} );
     print OUT $pack->short( lc($s->{ZERO_BEYOND_RANGE}) eq 'no' ? 0 : 1 );
     print OUT $pack->short( lc($s->{NESTED_SEQUENCE}) eq 'no' ? 0 : 1 );
     if( $version >= 3 )
     {
        print OUT $pack->string( $s->{VERSION_START} );
        my $vend=$s->{VERSION_END};
        if( $vend eq '0' ){ $vend='99999999' }
        print OUT $pack->string( $vend );
     }
     print OUT $pack->short( $s->{ncomponents} );

     foreach my $c (@{$s->{components}}) {
       my $tmodel=$c->{TIME_MODEL};
       my @tmparts=split(' ',$tmodel);
       my $tmtype=shift(@tmparts);
       push(@tmparts,'1.0') if ! @tmparts;
       if( lc($tmtype) eq 'velocity' )
       {
           # Convert a velocity time sequence to a displacement time sequence
           # that is zero at the reference epoch..
           my $offset=0;
           my $yref=DateToYear($c->{REF_DATE});
           unshift(@tmparts,0.0,$s->{START_DATE});
           push(@tmparts,$s->{END_DATE},0.0);
           my $y0=DateToYear($tmparts[1]);
           my $v0=$tmparts[2];
           $tmparts[2]=0.0;
           my $factor=0.0;
           for( my $i=3; $i < $#tmparts; $i++ )
           {
               my $v1=$tmparts[$i+1];
               my $y1=DateToYear($tmparts[$i]);
               if( $y0 <= $yref && $y1 > $yref )
               {
                   $offset=$factor+($yref-$y0)*$v0;
               }
               $factor += ($y1-$y0)*$v0;
               $tmparts[$i+1]=$factor;
               $v0=$v1;
               $y0=$y1;
           }
           for( my $i=0; $i <= $#tmparts; $i+=2 )
           {
               $tmparts[$i] -= $offset;
           }
       }
       print OUT $pack->string( $c->{DESCRIPTION} );
       print OUT &PackDate($pack,$c->{REF_DATE});
       print OUT $pack->double( @{$c->{sourcefile}->{range}} );
       print OUT $pack->short(1); # Time model type - always piecewise linear
       my $nstep=int($#tmparts/2);
       print OUT $pack->short($nstep);
       print OUT $pack->double( shift(@tmparts));
       while( $nstep-- )
       {
           print OUT &PackDate($pack,shift(@tmparts));
           print OUT $pack->double(shift(@tmparts));
       }
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

# A version 1 deformation sequence consists of one or more deformation models in order
# of date.  Each model can apply as a fixed or interpolated model before its
# reference date and after its reference date.  For dates between two models,
# the valid options are that the deformation is interpolated between the two
# models, or one or other model applies as a fixed model.  Times before the
# first model or after the last can be extrapolated based upon the first or
# last two models respectively.
#
# A version 2 deformation model sequence is a set of components each of which
# has either a velocity or a deformation time model.  The sequence can be
# nested, in which case only the first component matching the spatial location
# of the evaluation point is used.  Otherwise all components are calculated and
# summed to get the total deformation.
#
# A version 3 deformation model is similar to a version 2 model except that 
# it can hold multiple versions of the deformation model.  The header section
# may repeat VERSION_NUMBER, VERSION_DATE, DESCRIPTION multiple times.

# Each component in the sequence can be either a triangulated model, or 
# a grid model.  The source file referenced can be a pre-built binary file,
# or more usefully an ascii source file from which the binary can be built.
# The latter is preferable, as it will ensure that all components are built
# with the same endian-ness.

# Two example sequences follow, one with a single velocity grid, the other
# with two triangulated deformation components.


DEFORMATION_MODEL NZGD2000 deformation model
# Format is LINZDEFve, where v is the format version  2, or 3 
# and e is the endian-ness, either L or B.
# Endian-ness can be over-ridden by command line parameter to script.
#
FORMAT LINZDEF2B
# Calculation will fail for values outside range start date to end date
START_DATE 1-Jan-1850
END_DATE 1-Jan-2100
COORDSYS NZGD2000

# Version 1/2 format version info
VERSION_NUMBER 20171201
VERSION_DATE  12-Mar-2004
DESCRIPTION
This is the description of the model
This is a first try
END_DESCRIPTION

# Version 3 format version info - may be repeated
VERSION 20171201
VERSION_DATE  12-Mar-2004
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
NESTED_SEQUENCE no      (format version 2 or greater)
VERSION_START 20171201  (format version 3 or greater)
VERSION_END 0           (format version 3 or greater)
DESCRIPTION
The IGNS velocity model.  Based upon data from 1992 to 1997
Report number 12345
END_DESCRIPTION

DEFORMATION_COMPONENT igns98b.dmp
MODEL_TYPE grid
REF_DATE 1-Jan-2000
TIME_MODEL velocity
DESCRIPTION
END_DESCRIPTION

# Discrete event ...

DEFORMATION_SEQUENCE Patch
DIMENSION 2
START_DATE 30-Jun-2002
END_DATE 1-Jan-2100
ZERO_BEYOND_RANGE yes
NESTED_SEQUENCE no
DESCRIPTION
Totally ficticious deformation model ... 
Report number 456/7
END_DESCRIPTION

DEFORMATION_COMPONENT patch1.trg
MODEL_TYPE trig
REF_DATE 30-Jun-2002 13:40:00
TIME_MODEL PIECEWISE_LINEAR 0.0 30-Jun-2002 1.0 30-Jun-2003 0.0
DESCRIPTION
Initial offset
END_DESCRIPTION

DEFORMATION_COMPONENT patch2.trg
MODEL_TYPE trig
REF_DATE 30-Jun-2003 13:40:00
TIME_MODEL PIECEWISE_LINEAR 0.0 30-Jun-2002 0.0 30-Jun-2003 1.0
DESCRIPTION
Final offset
END_DESCRIPTION

# The version 1 format is as follows:

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
