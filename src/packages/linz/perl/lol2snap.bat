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
#!/usr/bin/perl
#line 15
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# Description:       Extracts test data from a CRS adjustment report with the 
#                    DBUG "dumpsql" output included.
#
# Dependencies:      Uses the following modules: 
#                      FileHandle  
#
# History
# Name                 Date        Description
# ===================  ==========  ============================================
# Chris Crook         23/04/2000  Created
#===============================================================================

use strict;
use FindBin;
use FileHandle;
use IO::String;

# CRS2 column name fixes

# May need reworking to reverse the direction, as runadj may be modified
# to use new column names.


die "Parameters: list_file_name  snap_file_root\n" if @ARGV != 2;
my($lst,$snaproot)=@ARGV;

my $degangles = 0;


my $datasets = 
{
getadj=>{ds=>'adj',id=>'ADJ_ID '},
getadjprm=>{ds=>'prm',id=>'COEF_CODE '},
getnode=>{ds=>'nod',id=>'NOD_ID '},
getobn=>{ds=>'obn',id=>'OBN_ID '},
getoba=>{ds=>'oba',id=>'OBN_ID1 OBN_ID2 '},
getsrv=>{ds=>'wrk',id=>'WRK_ID '},
crdsys=>{ds=>'cos',id=>'COS_ID '},
getcor=>{ds=>'cor',id=>'ID'},
};

my @month = qw/Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec/;

my $adjdata  = {};

my $reffile = "$FindBin::Bin/$FindBin::Script.refdata";
my $reff;
if( -e $reffile && open($reff,$reffile) )
{
   &ReadCSVData($reff,$adjdata,$datasets);
   close($reff);
}

my $flst;

if( lc($lst) eq '-c' )
{
  require Win32::Clipboard;
  $lst = "the clipboard";
  my $clip = Win32::Clipboard();
  my $txt = $clip->GetText() || die "Error: Cannot read from the clipboard\n";
  $flst = new IO::String($txt);
  
}
else
{
   open($flst,"<$lst") || die "Error: Cannot open $lst\n";
}



&ReadCSVData( $flst, $adjdata, $datasets );
close($flst);

if( ! exists $adjdata->{adj} )
{
   die "Error: Incorrect Landonline adjustment listing in $lst - use the DUMPSQL parameter\n";
}


my $adj = $adjdata->{adj};
my $cos = $adjdata->{cos};
my $nod = $adjdata->{nod};
my $obn = $adjdata->{obn};
my $oba = $adjdata->{oba};
my $wrk = $adjdata->{wrk};
my $prm = $adjdata->{prm};
my $cor = $adjdata->{cor};

my ($adjid) = keys %$adj;
$adj=$adj->{$adjid};

my $software = $adj->{SOFTWARE_USED};
if( $software !~ /^LNZ[CTG]$/ )
{
   die "Error: Currently only cadastral and geodetic adjustments can be converted to SNAP\n";
}

if( ! $cos )
{
   die "Error: No coordinate system information found - probably due to an error encountered in the Landonline adjustment\n";
}

my $adjcos = $cos->{$adj->{COS_ID}};
my $dtmid = $adjcos->{DTM_ID};
if( $dtmid ne '19' )
{
   die "Error: Currently only adjustments based on NZGD2000 can be converted - this adjustment uses another datum\n";
}

my $projcos = $adjcos->{COS_CODE};

foreach my $prmk (keys %$prm){ $prm->{$prmk}=$prm->{$prmk}->{COEF_VALUE};}

my $orders={};
foreach my $cork( keys %$cor ) 
{ 
    $orders->{$cork}=$cor->{$cork}->{DISPLAY} if $cor->{$cork}->{DTM_ID} eq $dtmid; 
}

my $snapcmd = $snaproot.".snp";
my $snapcrd = $snaproot.".crd";
my $snapobs = $snaproot.".dat";
my $rootname = $snaproot;
$rootname =~ s/.*[\\\/]//;

my($sfile,$cfile,$ofile);
open( $sfile, ">".$snapcmd ) || die "Cannot open $snapcmd for output\n";
open( $cfile, ">".$snapcrd ) || die "Cannot open $snapcrd for output\n";
open( $ofile, ">".$snapobs ) || die "Cannot open $snapobs for output\n";


my $plan = $wrk->{$adj->{WRK_ID}}->{SURVEY_NUMBER} if $adj->{WRK_ID};
 
my $title = 'Landonline adjustment : '.$adj->{ADJ_ID}.' - '.($adj->{DESCRIPTION} || ($adj->{METHOD_NAME} .' - ' .$plan ));

print $cfile <<"EOD";
$title
NZGD2000
options station_orders ellipsoidal_heights degrees no_geoid

EOD

# Print marks

print $ofile $title,"\n\n";
print $ofile "#deg_angles\n" if $degangles;

my $constraint={};
my $markname={};

my $fixmap = {FREX=>'FREE', FXHX=>'FIXH', FXVX=>'FIXV'};
$fixmap  = { FIXH=>'FIX3', FIXV=>'FREE' } if $software ne 'LNZG';


foreach my $id ( sort {$nod->{$a}->{COR_ID} <=> $nod->{$b}->{COR_ID} || 
               $nod->{$a}->{MARK_NAME} cmp $nod->{$b}->{MARK_NAME}} keys %$nod )
{
     my $mrk = $nod->{$id};
     my $c1 = sprintf("%.8f",$mrk->{VALUE1});
     my $c2 = sprintf("%.8f",$mrk->{VALUE2});
     my $c3 = sprintf("%.4f",$mrk->{VALUE3} || 0.0); 
     my $order = $orders->{$mrk->{COR_ID}};
     $order = '?' if $order eq '';
     my $name = $mrk->{MARK_NAME};
     printf $cfile "%-10s %s %s %s %s %s\n",$id,$c1,$c2,$c3,$order,$name;
   
     $markname->{$id} = $name;

     my $adjust = $mrk->{COO_CONSTRAINT};
     $adjust = $fixmap->{$adjust} || $adjust;
     push(@{$constraint->{$adjust}},$id);
}


# Print observations.  Sort by coordinate system, observation set, 
# and obs id.  Assume that observation sets don't span coordinate system
# Note that for cadastral surveys (current scope) observation set doesn't
# apply in any case.

my $lastcos = '';
my $lasttype = '';
my $lastbe = '';
my $lastsclass = '';
my $lastcclass = '';
my $lastrefdate = '';
my $lastsurvey = '';

my $brngerrs = {};
my $nodeinfo = {};
my $badtype = {};

my $nerr = 0;
my $avfunc;
my $aefunc;

if( $degangles )
{
    $avfunc = sub { return sprintf("%.4f",$_[0]); };
    $aefunc = sub { return sprintf("%.4f",$_[0]); };
}
else
{
    $avfunc = sub { return FormatDMS($_[0],1); };
    $aefunc = sub { return sprintf("%.1f",$_[0] * 3600.0); };
}

my $brsw = uc($prm->{BRSW}) || 'BY_SURVEY SET * 0';
my $brswcos = $brsw =~ /\bBY_CRDSYS\b/i;
my $brswbase = $plan;
$brswbase = $projcos if $brswcos;
$brswbase =~ s/\s+/_/g;


my $survey={};
my $have_gps_error_type = 0;
foreach my $id (keys %$wrk) { $survey->{$id}=$wrk->{$id}->{SURVEY_NUMBER}; }

foreach my $id ( sort {$obn->{$a}->{COS_ID} <=> $obn->{$b}->{COS_ID} || 
               $obn->{$a}->{OBS_ID} cmp $obn->{$b}->{OBS_ID} ||
               $obn->{$a}->{OBN_ID} cmp $obn->{$b}->{OBN_ID}}  keys %$obn )
{
     my $obs = $obn->{$id};
     my $var = $oba->{"$id:$id"};
     my $obscos = $cos->{$obs->{COS_ID}};
     $obscos = $obscos ? $obscos->{COS_CODE} : 'DEFAULT';
     my $from = $obs->{NOD_ID_LOCAL};
     my $to = $obs->{NOD_ID_REMOTE};
     my $type = $obs->{SUB_TYPE};
     my $accmult = $obs->{ACC_MULTIPLIER};
     my $refdate = $obs->{REF_DATETIME};
     $refdate =~ s/\s.*//;
     my $sclass = $obs->{SURVEYED_CLASS};
     $sclass = 'unknown' if $sclass eq '';
     my $cclass = $obs->{CADASTRAL_CLASS};
     $cclass = 'unknown' if $cclass eq '';
     my @snapobs;
     my $exclude = $obs->{EXCLUDE} eq 'Y' ? '* ' : '';
     my $obssurvey = $survey->{$obs->{WRK_ID}};
     $obssurvey =~ s/\s+/_/g;
     $obssurvey = 'unknown' if $obssurvey eq '';

     if( $sclass ne $lastsclass )
     {
          print $ofile "\n#classify surveyed_class $sclass\n";
          $lastsclass = $sclass;
     }
     if( $cclass ne $lastcclass )
     {
          print $ofile "\n#classify cadastral_class $cclass\n";
          $lastcclass = $cclass;
     }
     if( $obssurvey ne $lastsurvey )
     {
          $lastsurvey = $obssurvey;
          print $ofile "\n#classify survey $obssurvey\n";
     }
     if( $refdate ne $lastrefdate )
     {
	  $lastrefdate = $refdate;
          if( $refdate =~ /^(\d+)\-(\d+)\-(\d+)$/ ) 
          {
              $refdate = "$3 $month[$2-1] $1";
          }
          else
          {
              $refdate = 'unknown';
          }
          print $ofile "\n#date $refdate\n";
     }

     if( $type eq 'SLDI' )
     {
          push(@snapobs,{ 
              type=>'ED',
              value=>sprintf("%.4f",$obs->{VALUE1}),
              error=>sprintf("%.4f",$accmult*sqrt($var->{VALUE_11}))
              });
     }
     elsif ($type eq 'BEAR' || $type eq 'ARCO')
     {
         if( $obscos ne $lastcos )
         {
             print $ofile "\n\! Note: LOL cadastral adjustments use adjustment projection for all projection bearings\n#projection $projcos\n" if $projcos;
             undef $projcos;
             print $ofile "\n#classify pb circuit $obscos\n";
             $lastcos = $obscos;
                  
         }
         my $be = $brswcos ? $obscos : $obssurvey; 
         $be = uc($be);

          $brngerrs->{$be}++;
          push(@snapobs,{ 
              type=>'PB',
              value=>$avfunc->($obs->{VALUE1}),
              error=>$aefunc->($accmult*sqrt($var->{VALUE_11}))
              });
          if( $type eq 'ARCO' )
          {
              my $r = $obs->{ARC_RADIUS};
              my $sa = $obs->{VALUE2}/(2.0*$r);
              my $cl = 2.0*$r*sin($sa);
              my $ef = abs(cos($sa));
              $ef = 0.5 if $ef < 0.5;
              push(@snapobs,{ 
                  type=>'ED',
                  value=>sprintf("%.4f",$cl),
                  error=>sprintf("%.4f",$accmult*$ef*sqrt($var->{VALUE_22}))
                  });
          }
     }
     elsif ($type eq 'DXYZ')
    {
        my $ac2 = $accmult*$accmult;
        push( @snapobs,{
                type=>'GB',
                value=>join(' ',$obs->{VALUE1},$obs->{VALUE2},$obs->{VALUE3}),
                error=>join(' ',$var->{VALUE_11}*$ac2,$var->{VALUE_12}*$ac2,$var->{VALUE_22}*$ac2,$var->{VALUE_13}*$ac2,$var->{VALUE_23}*$ac2,$var->{VALUE_33}*$ac2),
            });
    }
     else
     {
         print "Error: Cannot process $type observations\n" if ! $badtype->{$type};
         $badtype->{$type}++;
         $nerr++;
         next;
     }

     foreach my $o (@snapobs )
     {
          my $stype = $o->{type};
          if( $type eq 'GB' && ! $have_gps_error_type )
          {
                print $ofile "\n#gps_error_type full\n";
                $have_gps_error_type = 1;
          }
          print $ofile "\n#data $stype id value error no_heights\n\n" if $type ne $lasttype;
          $lasttype = $stype;

          print $ofile "$exclude$from $to $id $o->{value} $o->{error} ! $markname->{$from} : $markname->{$to}\n";
          $nodeinfo->{$from}->{count}++;
          $nodeinfo->{$to}->{count}++;
     }
}


for my $k (keys %$oba)
{
    next if $k =~ /^(\-?\d+)\:\1$/;
    print "Error: Cannot process covariances between observations $k\n";
    $nerr++;
    last;
}


my $mode = 'adjustment';
$mode = 'free_net_adjustment' if lc($prm->{FNAD}) eq 'yes';
my $maxit = $prm->{MXIT} || 10;
my $maxch = $prm->{MXCH} || 1000;
my $conv = $prm->{CNVG} || 0.001;
my $defm = $prm->{DEFM};

my $dim = $software eq 'LNZG' ? '3d' : '2d';

print $sfile <<"EOD";
title $title

coordinate_file $rootname.crd
data_file $rootname.dat

mode $dim $mode

convergence_tolerance $conv
max_iterations $maxit
max_adjustment $maxch

EOD

my $free = 'free';
my $float = $prm->{WCRD};

my $consmap = { FREE=>'',FIX3=>'fix',FIXH=>'fix horizontal',FIXV=>'fix vertical' };
if( $float > 0 )
{
    print $sfile "\nhorizontal_float_error $float\n";
    $consmap->{FREE} = "float horizontal";
}

if( $defm eq '' )
{
    print $sfile "\ndeformation none\n";
}
else
{
    print $sfile "\n! Deformation from Landonline: $defm\n";
    print $sfile "deformation datum\n";
}

foreach my $cons (sort keys %$constraint)
{
    my $cmd = $consmap->{$cons};
    next if ! $cmd;
    my @ids = @{$constraint->{$cons}};

    while( my @slice = splice(@ids,0,8))
    {
        print $sfile join(' ',$cmd,@slice),"\n";
    }
}

print $sfile "\n";

my $fixedbrsw = 0;
print $sfile "bearing_orientation_error use ",$brswcos ? "circuit" : "survey","\n";
print $sfile "! bearing_orientation_error use ",$brswcos ? "survey" : "circuit","\n";
foreach my $brset ($brsw =~ /\b(calc\s+\S+|set\s+\S+\s+\S+)\b/ig)
{
    my($status,$code,$value) = split(' ',$brset);
    $code = uc($code);
    $code = $brswbase if $code eq '*0';
    next if $code ne '*' && ! exists $brngerrs->{$code};
    $status = lc($status) eq 'calc' ? 'calculate ' : '';
    $fixedbrsw = 1 if ! $status;
    $value += 0.0;
    print $sfile "bearing_orientation_error $status$code $value\n";
}

if( lc($prm->{AFIX}) eq 'yes' )
{
        my $ncon = 0;
        my $ndmax = '';

	for my $n ( keys %$nodeinfo )
        {
            if( $nodeinfo->{$n}->{fix} ) { $ndmax = ''; last; }
            if( $nodeinfo->{$n}->{count} > $ncon ) { $ncon = $nodeinfo->{$n}->{count}; $ndmax = $n }
        }
        if( $ndmax )
        {
            print $sfile "\n\n! Fixing node to emulate Landonline adjustment\nfix $ndmax\n";
            print "Fixing node $ndmax to emulate Landonline adjustment\n";
        }

        if( ! $fixedbrsw )
        {
            $ncon = 0;
            my $bsmax = '';
            foreach my $bs (keys %$brngerrs )
            {
               if( $brngerrs->{$bs} > $ncon ) { $ncon = $brngerrs->{$bs}; $bsmax = $bs; }
            }
            if( $bsmax ne '' )
            {
            print $sfile "\n\n! Fixing bearing swing to emulate Landonline adjustment\nbearing_orientation_error set $bsmax 0.0\n";
            print "Fixing bearing swing  $bsmax to emulate Landonline adjustment\n";
            }
            
        }
}




sub ReadCSVData
{
   my( $fh, $adjdata, $datasets ) = @_;


while( my $line = $fh->getline ) {
   next if ! ($line =~ /^\@([\@\*])(\w+)\|/);
   my ($header,$type,$record) = ($1,$2,$');
   next if ! exists $datasets->{$type};
   chomp($line);
   my @data = split(/\|/,$record);
   if( $header eq '*')
   {
       &SetupDataset($datasets->{$type},\@data);
   }
   else
   {   
      &LoadData($datasets->{$type},$adjdata,\@data);
   }
   }
}
  

sub SetupDataset
{
   my($dataset,$fields) = @_;
   foreach (@$fields) { s/\./_/g; }
   $dataset->{fields} = $fields;
   my @id = split(' ',$dataset->{id});
   $dataset->{id} = \@id;
}
	

sub LoadData
{
   my($dataset,$adjdata,$data) = @_;
   return if ! $dataset->{fields};
   my $fields = $dataset->{fields};
   my $record={};
   for my $i ( 0 .. $#$fields )
   {
      $record->{$fields->[$i]} = $data->[$i];
   }
   my $key = join(':',map { $record->{$_} } @{$dataset->{id}});
   $adjdata->{$dataset->{ds}}->{$key} = $record;
}


#===============================================================================
#
#   SUBROUTINE:   FormatDMS
#
#   DESCRIPTION:  Formats an angle as degrees, minutes, seconds
#
#   PARAMETERS:   
#
#   RETURNS:      
#
#   GLOBALS:
#
#===============================================================================

sub FormatDMS {
    my($value,$ndp,$codes) = @_;
    my($deg,$min,$sec,$hem);
    if( $codes ne '' ) {
       $hem = ' '.substr($codes,($value < 0 ? 0 : 1),1);
       $value = -$value if $value < 0;
       }
    else {
       $hem = '';
       $value += 360.0 while $value < 0.0;
       }
    $ndp = int($ndp);
    my $ndp3 = $ndp+3;

    $deg = int($value);
    $value = ($value - $deg)*60;
    $min = int($value);
    $sec = ($value-$min)*60;
    return sprintf("%d %02d %$ndp3.$ndp"."f%s",$deg,$min,$sec,$hem);
    }


__END__
:endofperl
