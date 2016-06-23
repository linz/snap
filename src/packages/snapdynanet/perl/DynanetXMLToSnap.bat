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
use strict;
use FindBin;
use lib "$FindBin::Bin/perllib";
use DynanetXML;
use Getopt::Std;
use Data::Dumper;

my $syntax = <<EOD;

DynanetToSnap: Converts Dynanet XML files to SNAP format

Syntax:
  DynanetToSnap stn_xml_file [geoid_file] obs_xml_file snap_root
  DynanetToSnap -o obs_xml_file snap_root
  DynanetToSnap -s stn_xml_file [geoid_file] snap_root


EOD


my %snapobsdef = 
(
   	GB=>{gps_error_type=>'full'}, 
	GX=>{gps_error_type=>'full'}, 
	SD=>{}, 
	HD=>{}, 
	ED=>{}, 
	DR=>{}, 
	HA=>{deg_angles=>'',_ndp=>5}, 
	AZ=>{deg_angles=>'',_ndp=>5}, 
	PB=>{deg_angles=>'',_ndp=>5}, 
	ZD=>{deg_angles=>'',_ndp=>5}, 
	LV=>{}, 
	OH=>{}, 
	EH=>{}, 
	LT=>{deg_angles=>'',_ndp=>9}, 
	LN=>{deg_angles=>'',_ndp=>9}, 
	# EP=>{deg_angles=>''}, 
);

die $syntax if ! @ARGV;

my %opts;
getopts('soc:',\%opts);

my $stnonly = $opts{s};
my $obsonly = $opts{o};
my $llhcrdsys = $opts{c} || 'AGD84';

die "Cannot have both -o and -s options\n" if $obsonly && $stnonly;

my $snaproot = pop(@ARGV) if @ARGV > 1;

die "Snap file name root missing\n" if ! $snaproot;

my $obsfile = pop(@ARGV) if ! $stnonly;
my $stnfile = shift(@ARGV) if ! $obsonly;
my $geoidfile = shift(@ARGV) if ! $obsonly;

my $valid = ($obsfile || $stnonly) && ($stnfile || $obsonly);

die $syntax if ! $valid || @ARGV;

my $dml = new DynanetXML;

my $cmdfile;

my $title = "Converted from dynanet XML files: ";
{
   my ($sc,$mn,$hr,$dy,$mo,$yr) = localtime();
   $mo++; $yr+=1900;
   $title .= sprintf("%d-%02d-%4d %02d:%02d",$dy,$mo,$yr,$hr,$mn);
}

if( ! $stnonly && ! $obsonly )
{
   open($cmdfile,">", "$snaproot.snp") ||
     die "Cannot create SNAP command file $snaproot.snp\n";
   print $cmdfile "title $title\n\n";
   print $cmdfile "coordinate_file $snaproot.crd\n";
   print $cmdfile "data_file $snaproot.dat\n";
   print $cmdfile "mode 3d adjustment\n";
}

my @errors;
if( $stnfile )
{
   eval
   {
    open(my $crdfile,">","$snaproot$\.crd") || die "Cannot create $snaproot.crd\n";
    my $stns = $dml->ParseStation($stnfile,$geoidfile);
    push(@errors,$dml->Errors(1));
    die "No station data in $stnfile\n" if ! $stns || ! @$stns;

    my $crdsys = $stns->[0]->{crdsys};
    my $havegeoid = exists $stns->[0]->{geoid} ? ' geoid' : '';
    foreach my $s (@$stns)
    {
	die "Inconsistent coordinate systems in $stnfile\n"
             if $s->{crdsys} ne $crdsys;
    }
    my $proj = $crdsys ne 'LLH';
    $crdsys = $llhcrdsys if $crdsys eq 'LLH';
    my $crdfmt = $proj ? "%13.4f" : "%14.9f";
    my $degrees = $proj ? '' : ' degrees';

    print $crdfile $title,"\n";
    print $crdfile $crdsys,"\n";
    print $crdfile "options$degrees ellipsoidal_heights$havegeoid\n";
    print $crdfile "\n";
    
    my %fix;
    foreach my $s (@$stns )
    {
        my $g = $havegeoid ?  join(' ',map {sprintf("%7.3f",$_)} @{$s->{geoid}}) : '';
	my $code = StationCode($s->{code});
        printf $crdfile "%-8s  $crdfmt $crdfmt %11.4f %s %s\n",
             $code,@{$s->{crd}},$g,$s->{name};
        push(@{$fix{$s->{constraint}}},$code);
    }
    if( $cmdfile )
    {

       print $cmdfile "\nfree all\n";
       foreach my $c (sort keys %fix)
       {
	  next if $c eq 'FFF';
          my $cmd;
          $cmd = "fix" if $c eq 'CCC';
          $cmd = "fix horizontal" if $c eq 'CCF';
          $cmd = "fix vertical" if $c eq 'FFC';
          if( ! $cmd )
          {
             push(@errors,"Unusable station constraint $c");
             next;
          }
          print $cmdfile "\n";
          foreach my $s (sort @{$fix{$c}}){ print $cmdfile "$cmd $s\n";}
       }
    }
  };
  push(@errors,$@) if $@;
}

if( $obsfile )
{
   eval 
   {
    open(my $datfile,">","$snaproot$\.dat") || die "Cannot create $snaproot.dat\n";
    print $datfile $title,"\n";
    
    my $status = {};

    $dml->ParseObs($obsfile,
         sub { 
            push(@errors,$dml->Errors(1));
            eval 
            {
               WriteSnapObs($datfile,$status,$_[0]);
            };
            push(@errors,$@) if $@;
            });
   };
   push(@errors,$@) if $@;
}

if( @errors )
{
   foreach my $e (@errors)
   {
      chomp($e); print $e,"\n";
   }
}

sub WriteSnapObs
{
   my($datfile,$status,$obs) = @_;
   eval 
   {
   my $id = $obs->{_id};
   my $type = $obs->{type};
   my $to = $obs->{to};

   my $ignoreobs = $obs->{ignore} ? '* ' : '';
   my $grouped = @{$obs->{to}} > 1;
   my $groupopt = $grouped ? ' grouped' : '';
   my $heights = $obs->{fromhgt} != 0;
   my $heightopt = $heights ? '' : ' no_heights';
   foreach my $t (@$to) { $heights = 'heights' if $t->{tohgt} != 0.0; }
   my $covar = exists $obs->{cvr};
   
   die "Obs $id: Cannot use SNAP observation type $type\n" 
        if ! exists  $snapobsdef{$type};
   my $newstatus = $snapobsdef{$type};
   $newstatus->{data} = "$type error$groupopt$heightopt";
   $newstatus->{date} = $obs->{date};

   foreach my $s (sort keys %$newstatus)
   {
       next if $s =~ /^_/;
       my $sts = $newstatus->{$s};
       next if exists $status->{$s} && $status->{$s} eq $sts;
       print $datfile "\n#$s $sts\n";
       $status->{$s} = $sts;
   }

   my $ndp = $newstatus->{_ndp} || 4;
   my $ofmt = " %.$ndp"."f";
   my $stnfmt = "%-8s";
   $stnfmt .= " %8.3f" if $heights;
   my $obset = $grouped && $covar;
   my $note = $obs->{comment};
   
   print $datfile "\n";
   print $datfile "\n#note $note\n\n" if $note;
   print $datfile "! id ",$obs->{_id},"\n";
   printf $datfile $stnfmt,StationCode($obs->{from}),$obs->{fromhgt} if $obs->{from};
   print $datfile $grouped ? "\n" : "  ";
   foreach my $t (@$to )
   {
       printf $datfile $stnfmt,StationCode($t->{to}),$t->{tohgt} if $t->{to};
       my $value = $t->{value};
       if( ref($value) ) { $value = join(' ',map {sprintf($ofmt,$_)} @$value);}
       else { $value = sprintf($ofmt,$value); } 
       print $datfile " ",$ignoreobs,$value;
       if( ! $covar ) { printf $datfile " $ofmt",$t->{sd}; }
       elsif( ! $obset ) { print $datfile " ",join(' ',@{$obs->{cvr}}); }
       print $datfile "\n";
   }
   if ($obset )
   {
      print $datfile "#end_set";
      my $cvr = $obs->{cvr};
      my $ncvr = $#$cvr;
      my $i = 0;
      my $nr = 1;
      while( $i <= $ncvr )
      {
          my $i0 = $i;
          $i += $nr;
          $nr++;
          my $maxnp = 8;
          my $np = $maxnp;
          for(;$i0 < $i; $i0++) 
	  {
              if($np >= $maxnp) { print $datfile "\n  "; $np=0; }
              print $datfile "  ",$cvr->[$i0];
              $np++;
          }
      }
      print $datfile "\n";
      
   }
   };

   if( $@ )
   {
      my $error = $@;
      chomp($error);
      die  "Obs ".$obs->{_id}.": Snap type ".$obs->{type}.": From ".$obs->{from}.": Error $error\n";
   }
}

sub StationCode
{
   my ($code) = @_;
   $code =~ s/^\s+//;
   $code =~ s/\s+$//;
   $code =~ s/\s+/_/;
   return $code;
}


__END__
:endofperl
