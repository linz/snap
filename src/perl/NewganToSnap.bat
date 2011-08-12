@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
perl -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl
:WinNT
perl -x -S %0 %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#!perl
#line 15
#
# Script to build SNAP command files from a newgan dat file.
#

use strict;
use FindBin;
use FileHandle;
use Getopt::Std;
use lib "$FindBin::Bin/perllib";
use NewGanFile;

# do "perllib/NewGanFile.pm";

my $syntax = <<EOD;
newgan_snap: Generates SNAP input files from a NEWGAN file.

Syntax: newgan_snap [options] newgan_file snap_root

Where
   newgan_file is the name of the NEWGAN file
   snap_root   is the base name of the snap files (ie no extension)
   
Options can include
   -s          print a summary of information from the NEWGAN file


EOD

my %opts;
getopts("s",\%opts);
my $printsummary = $opts{s};

@ARGV == 2 || die $syntax;

my($newganfile, $snapfile) = @ARGV;

my $snapcmdfile = $snapfile.".snp";

my $snapcrdfile = $snapfile.".crd";
my $snapdatfile = $snapfile.".dat";
my $logfile = $snapfile.".newgan_snap.log";

if( lc($snapcmdfile) eq lc($newganfile) ||
    lc($snapcrdfile) eq lc($newganfile) ||
    lc($snapdatfile) eq lc($newganfile) ||
    lc($logfile) eq lc($newganfile) ) {
    die "Snap files will overwrite NEWGAN file ... try a different name\n";
    }

my $log = new FileHandle($logfile,"w");
my $runtime = localtime;

$log->print("NEWGAN-Snap translation: $runtime\n\n");
$log->print("NEWGAN file: $newganfile\n");
$log->print("Snap command file: $snapcmdfile\n");
$log->print("Snap coordinate file: $snapcrdfile\n");
$log->print("Snap data file: $snapdatfile\n");

# Try reading the NEWGAN file

my $newgan; 

eval {
   $newgan = new NewGanFile( $newganfile );
   };
if( $@ ) {
   $log->print($@);
   $log->close;
   print $@;
   print "\nTranslation terminated\n";
   exit;
   }

my @warnings = $newgan->Warnings;
if( @warnings ) {
   $log->print("\n","*"x70,"\nWarnings reading NEWGAN file\n");
   $log->print(@warnings);
   $log->print("\n","*"x70,"\n\n");
   }

my $cmd = new FileHandle($snapcmdfile,"w");
my $crd = new FileHandle($snapcrdfile,"w");
my $dat = new FileHandle($snapdatfile,"w");

&PrintSummary( $newgan, $log ) if $printsummary;

&PrintSnapCommandFile( $newgan, $cmd, $snapcrdfile, $snapdatfile );
my $snapcodes = &PrintSnapCoordinateFile( $newgan, $crd, $cmd, $log );
&PrintSnapDataFile( $newgan, $dat, $cmd, $crd, $log, $snapcodes );

######################################################################

sub PrintSnapCommandFile {
  my( $newgan, $cmd, $snapcrdfile,$snapdatfile ) = @_;
  $snapcrdfile =~ s/.*[\\\/]//;
  $snapdatfile =~ s/.*[\\\/]//;

  my $options = $newgan->options;

  $cmd->print("title ",$options->title,"\n");

  $cmd->print("!\n! SNAP job generated from NEWGAN file ",$newgan->filename,"\n!\n");

  my $warnings = join('',$newgan->Warnings);
  if( $warnings ne '' ) {
     $warnings =~ s/\n/\n!  /g;
     $warnings = "!\n! Newgan file warnings\n!  ".$warnings."\n\n";
     $cmd->print($warnings);
     }
  
  my $dimension = $options->dimension;
  my $iterations = $options->iterations;

  my $mode = $dimension.'d';
  if( $iterations == 99 ) {
     $iterations = 10;
     $mode .= ' data_check';
     }
  elsif( $iterations < 0 ) {
     $iterations = 10;
     $mode .= ' network_analysis';
     }
  else {
     $mode .= ' adjustment';
     }

  $cmd->print("mode $mode\n");
  $cmd->print("coordinate_file $snapcrdfile\n");
  $cmd->print("data_file $snapdatfile\n");

  $cmd->print("\nmax_iterations $iterations\n");

  # Convert tolerance in seconds to tolerance in metres ... not strictly
  # correct of course!

  $cmd->print("convergence_tolerance ",$options->tolerance*30.0,"\n");

  # A priori variance factor ...
  if( $options->apriori_variance != 1.0 ) {
     $cmd->print("! A priori variance from NEWGAN\n");
     my $factor = sqrt(abs($options->apriori_variance));
     $cmd->print("classification data_file $snapdatfile error_factor $factor\n");
     }
  
  if( $options->list_iteration_corrections ) {
     $cmd->print("print station_adjustments\n");
     }
  if( $options->print_equations ) {
     $cmd->print("print observation_equations\n");
     }


  if( $options->print_full_inverse ) {
     $cmd->print("output covariance_matrix\n");
     }

  if( $options->bandwidth_optimisation eq 'N' ) {
     $cmd->print("\n! Newgan settings had matrix optimisation disabled\n",
                 "! To replicate in SNAP uncomment the following line\n");
     $cmd->print("! reorder_stations off\n");
     }

  }

#----------------------------------------------------------------------

sub PrintSnapCoordinateFile {
  my( $newgan, $crd, $cmd, $log ) = @_;

  my $crdsys = $newgan->crdsys;

  $crd->print($newgan->options->title,"\n");
  $crd->print($crdsys->code,"\n");
 
  my $got_geoid = $newgan->got_geoid;
  $crd->print("no_geoid\n") if ! $got_geoid;

  $crd->print("!\n! SNAP coordinate file generated from NEWGAN file ",$newgan->filename,"\n!\n");
  
  my $snapcodes = {};
  my $newgancodes={};
  my @free = ();
  my @fixed = ();
  my %float = ();
  my $floating = 0;
  my %constraint = ();

  my $stations = $newgan->stations;

  foreach my $stn (@$stations) {
     my $newgancode = $stn->station_id;
     my $snapcode = uc($newgancode);
     $snapcode =~ s/^\s+//;
     $snapcode =~ s/\s+$//;
     $snapcode =~ s/\s+/_/g;
     if( exists $newgancodes->{$snapcode} ) {
        my $snapbase = $snapcode;
        my $idn = 1;
        do {
           $snapcode = $snapbase.sprintf("%02d",$idn++);
           }
        until ! exists $newgancodes->{$snapcode};
        $log->print("Station $newgancode renamed to $snapcode to avoid name conflict\n");
        $crd->print("! Station $newgancode renamed to $snapcode to avoid name conflict\n");
        }
     $newgancodes->{$snapcode} = $newgancode;
     $snapcodes->{$newgancode} = $snapcode;
     my $name = $stn->station_name || $newgancode;

     my $crds = $stn->coord->as($crdsys)->asstring(6,4);
     $crd->print(sprintf("%-10s",$snapcode)," ",join(" ",@$crds)," ");
     if( $got_geoid ) {
        if( ! $stn->has('geoid_separation') ) {
            $log->print("Geoid values were not defined for $snapcode\n");
            $crd->print("! Geoid values were not defined for $snapcode\n");
            }
        $crd->printf("%.3f %.3f %.3f ",
            $stn->get('deflection_north',0.0),
            $stn->get('deflection_east',0.0),
            $stn->get('geoid_separation',0.0));
        }
     $crd->print($name,"\n");

     my $mode = $stn->adjustment_mode;

     if( $mode eq 'free') {
        push(@free,$snapcode);
        }
     elsif( $mode eq 'fixed') {
        push(@fixed,$snapcode);
        }
     elsif( $mode eq 'float' ) {
        my $id = $stn->constraint->id;
        $floating = 1;
        $float{$id} = [] if ! exists $float{$id};
        $constraint{$id} = $stn->constraint if ! exists $constraint{$id};
        push(@{$float{$id}},$snapcode);
        }
     else {
        die "Missing/invalid adjustment mode $mode for station $newgancode\n";
        }
     }

  $cmd->print("\nfree all\n");

  PrintStationList( $cmd, 'fix', \@fixed );

  if( $floating ) {
     $log->print("\nConstraint/stations have different values in SNAP to NEWGAN\n",
                 "The SNAP horizontal error is based on the NEWGAN latitude constraint\n");
     $cmd->print("\n! Constraint/float stations have different errors in SNAP to NEWGAN\n",
                 "! The SNAP horizontal error is based on the NEWGAN latitude constraint\n");
     foreach my $id ( sort keys %constraint ) {
       my $constraint = $constraint{$id};
       # Approximate conversion of lat_constraint to metres.  
       # Cannot enter a different value for longitude constraint in SNAP, so
       # just use the latitude value...

       my $horiz = ($constraint->{lat_constraint}/1000.0)*30.0;
       my $vert = $constraint->{hgt_constraint}/100.0;
       $cmd->printf("\nhorizontal_float_error %.4f\n",$horiz);
       $cmd->printf("vertical_float_error %.4f\n",$vert);
       PrintStationList( $cmd, 'float', $float{$id});
       };
     }

  # Don't print free stations, as this is the default 
  # PrintStationList( $cmd, 'free', \@free );
   
  return $snapcodes;
  }

sub PrintStationList {
  my($cmd,$command,$list) = @_;
  $cmd->print("\n");
  for( my $i = 0; $i < @$list; $i += 8 ) {
      my $imax = $i+7;
      $imax = $#$list if $imax > $#$list;
      $cmd->print(join(' ',$command,@$list[$i..$imax]),"\n");
      }
   }

sub CreateDummyStation {
  my ( $newgan, $crd, $cmd, $code, $coord, $name ) = @_;
  my $crds = $coord->asstring(6,4);
  $crd->print(sprintf("%-10s",$code)," ",join(" ",@$crds)," ");
  if( $newgan->got_geoid ) { 
     $crd->printf("%.3f %.3f %.3f ",0,0,0);
     }
  $crd->print($name,"\n");
  $cmd->print("\n!Fixing dummy station $code: $name\n");
  $cmd->print("fix $code\n");
  }

sub GetDummyCode {
  my ($snapcodes, $base) = @_;
  
  my $id = 0;
  my $refname;
  do {
    $refname = sprintf("DUMMY%03d",$id++)
    }
  while exists $snapcodes->{$refname};

  my %used = map { $_ => 1 } values %$snapcodes;

  $id = 0;
  my $dummy = $base;
  while( exists $used{$dummy} ) {
     $dummy = sprintf("%s%03d",$base,$id++);
     }
  $snapcodes->{$refname} = $dummy;
  return $dummy;
  }

#----------------------------------------------------------------------

sub PrintSnapDataFile {
  my( $newgan, $dat, $cmd, $crd, $log, $snapcodes ) = @_;

my $snap_datadef = {
   direction_set=>'#data HA error no_heights',
   azimuth=>'#data AZ error no_heights',
   slopedist=>'#data SD error no_heights',
   elldist=>'#data ED error no_heights',
   msldist=>'#data MD error no_heights',
   angle=>'#data HA error no_heights',
   hgtdiff=>'#data LV error no_heights',
   elevation=>'#data OH error no_heights',
   zendist=>'#data ZD error',
   zendistih=>'#data ZD error',
   llh_position=>'#data GB error grouped no_heights',
   xyz_position=>'#data GB error grouped no_heights',
   llh_baseline=>'#data GB error grouped no_heights',
   xyz_baseline=>'#data GB error grouped no_heights',
   dxyz_baseline=>'#data GB error no_heights',
   };

my $snap_notes = {
   direction_set=>
      "Plumbing errors for directions are converted to\n".
      "angle errors based on the trial coordinates of the marks",
   azimuth=>
      "Plumbing errors for azimuths are converted to\n".
      "angle errors based on the trial coordinates of the marks",
   angle=>
      "Angle observations are converted into direction observations\n".
      "Plumbing errors for angles are converted to\n".
      "angle errors based on the trial coordinates of the marks",
   zendist=>
      "Plumbing errors for zenith distances are converted to\n".
      "angle errors based on the trial coordinates of the marks",
   zendistih=>
      "Plumbing errors for zenith distances are converted to\n".
      "angle errors based on the trial coordinates of the marks",
   llh_position=>
      "Position observations are converted to sets of GPS baseline\n".
      "observations referenced to a ficticious generated station",
   xyz_position=>
      "Position observations are converted to sets of GPS baseline\n".
      "observations referenced to a ficticious generated station",
   };
  

  $dat->print($newgan->options->title,"\n");
 
  $dat->print("!\n! SNAP observation file generated from NEWGAN file ",$newgan->filename,"\n!\n");

  $dat->print("#date unknown\n");
  
  my $printsource = 1;
  my $datadef = '';
  my $gotusercode = 0;
  my $usercode = '';
  my $refcoef = '';
  my $gpsrefsys = '';
  my $gpserror = '';
  my %commands = ();

  my $observations = $newgan->observations;
  
  my %counts = ();
  my %notes = ();

  foreach my $obs ( @$observations ) {
     my $type = $obs->type;
     my $category = $obs->category;
   
     $counts{$type}++;
     
     my $newdatadef = $snap_datadef->{$type};
     next if ! $newdatadef;

     if( $newdatadef ne $datadef ) {
        $datadef = $newdatadef;
        $dat->print("\n$datadef\n");
        }

     if( $obs->user_code ne $usercode ) {
        $usercode = $obs->user_code;
        if( ! $gotusercode ) {
           $gotusercode = 1;
           $dat->print("#classification usercode\n");
           }
        $dat->print("\n#classify usercode $usercode\n");
        }

     if( $printsource ) {
        $dat->printf("\n! From %s line %d\n",$newgan->filename,$obs->source_line );
        }
     if( $obs->type eq 'direction_set' ) {
        $dat->print("\n".$snapcodes->{$obs->station_id_from}."\n");
        while( $obs ) {
           $dat->printf("%-10s %3d %02d %05.2f %5.2f\n",
                $snapcodes->{$obs->station_id_to},
                $obs->angle_deg, $obs->angle_min, $obs->angle_sec,
                $obs->CalcStdDev($newgan) );
           $obs = $obs->get('next');
           }
        }

     elsif( $obs->type eq 'angle' ) {
        $dat->print("\n".$snapcodes->{$obs->station_id_from}."\n");
        my $stddev = $obs->CalcStdDev($newgan) / sqrt(2.0);
        $dat->printf("%-10s %3d %02d %05.2f %5.2f\n",
                $snapcodes->{$obs->station_id_ref},0,0,0,$stddev );
        $dat->printf("%-10s %3d %02d %05.2f %5.2f\n",
                $snapcodes->{$obs->station_id_to},
                $obs->angle_deg, $obs->angle_min, $obs->angle_sec,
                $stddev );
        }

     elsif( $obs->type eq 'azimuth' ) {
        $dat->printf("%-10s %-10s %3d %02d %05.2f %5.2f\n",
                $snapcodes->{$obs->station_id_from},
                $snapcodes->{$obs->station_id_to},
                $obs->angle_deg, $obs->angle_min, $obs->angle_sec,
                $obs->CalcStdDev($newgan) );
        }

     elsif( $obs->type eq 'zendist' ) {
        my $newrefcoef = $obs->refraction_coef->id;
        if( $newrefcoef ne $refcoef ) {
           $refcoef = $newrefcoef;
           $dat->print("\n#refraction_coefficient $refcoef\n");
           $commands{"refraction_coefficient $refcoef ".$obs->refraction_coef->refrcoef} = 1;
           }
        $dat->printf("%-10s %-10s %3d %02d %05.2f %5.2f\n",
                $snapcodes->{$obs->station_id_from}, 
                $obs->get('inst_hgt_from',0.0),
                $snapcodes->{$obs->station_id_to},
                $obs->get('inst_hgt_to',0.0),
                $obs->angle_deg, $obs->angle_min, $obs->angle_sec,
                $obs->CalcStdDev($newgan) );
        }
     elsif( $obs->type eq 'elevation' ) {
        $dat->printf("%-10s %11.4f %7.4f\n",
                $snapcodes->{$obs->station_id_from}, 
                $obs->value,
                $obs->CalcStdDev($newgan) );
        }
     elsif( $obs->type =~ /^(slopedist|elldist|msldist|hgtdiff)/ ) {
        $dat->printf("%-10s %-10s %11.4f %7.4f\n",
                $snapcodes->{$obs->station_id_from}, 
                $snapcodes->{$obs->station_id_to}, 
                $obs->value,
                $obs->CalcStdDev($newgan) );
        }
     elsif( $obs->type eq 'dxyz_baseline' ) {
        if( $gpserror ne 'full' ) {
           $dat->print("\n#gps_error_type full\n");
           $gpserror = 'full';
           }
        if( $obs->refsys->id ne $gpsrefsys ) {
           my $refsys = $obs->refsys;
           $gpsrefsys = $refsys->id;
           $dat->print("\n#reference_frame $gpsrefsys\n");
           $commands{sprintf("reference_frame geocentric %s scale %.2f rotation %.4f %.4f %.4f",
                $gpsrefsys,$refsys->tfm_scale,
                $refsys->tfm_rx, $refsys->tfm_ry, $refsys->tfm_rz )} = 1;
           }
        my $lt;
        eval {
           $lt = $obs->CalcLTCovariance($newgan);
           };
        if( $@ ) {
           $dat->printf("! $@");
           $notes{$@} = 1;
           }
        else {
           my $dxyz = $obs->components->[0];
           $dat->printf("%-10s %-10s %11.4f %11.4f %11.4f &\n",
                $snapcodes->{$dxyz->station_id_from}, 
                $snapcodes->{$dxyz->station_id_to}, 
                $dxyz->x, $dxyz->y, $dxyz->z );
           $dat->printf("   %16.10f &\n".
                     "   %16.10f %16.10f &\n".
                     "    %16.10f %16.10f %16.10f\n",
                     @$lt );
           }
        }
     else {
        $dat->print("! Not handling $type observations yet ...\n");
        }
     }


  $cmd->print("\n") if %commands;
  foreach my $command ( sort keys %commands ) {
     $cmd->print($command,"\n");
     }

  foreach my $type ( sort keys %counts ) {
     if( ! exists $snap_datadef->{$type} ) {
        $notes{sprintf("*** %d %s observations could not be translated",
                $counts{$type},$type)} = 0;
        }
     elsif( exists $snap_notes->{$type} ) {
        $notes{$snap_notes->{$type}} = 1;
        }
     }

  if( %notes ) {
     $log->print("\nOBSERVATION NOTES\n");
     foreach my $note ( sort { $notes{$a} <=> $notes{$b} || $a cmp $b } keys %notes ) {
        $log->print("\n$note\n");
        print("\n$note\n");
        }
     }
  }

######################################################################

sub PrintSummary {
   my( $newgan, $log) = @_;

   $log->print("\nSummary of NEWGAN file\n");

   my $options = $newgan->options;
   my $stations = $newgan->stations;
   my $observations = $newgan->observations;


   $log->print("Options:\n");

   foreach my $k (sort keys %$options) {
      $log->printf("    %-30s %10s\n", $k, $options->{$k});
      }
   
   my $count = scalar(@$stations);
   $log->print("\nFile defines $count stations\n\n");
   
   my %obscount = ();
   foreach my $o ( @$observations ) {
      $obscount{$o->{type}}++;
      }
   
   $log->print("\nObservations\n");
   foreach my $ot ( sort keys %obscount ) {
      $log->print("   ",$obscount{$ot}," ",$ot," observations\n");
      }
   }

__END__
:endofperl
