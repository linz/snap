#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         This script creates a customised interface for writing
#                      SQL files of adjustment data.  The schema is defined by
#                      the @crsdef array defined at the end of this file.
#
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook          19/06/1999  Created
#===============================================================================

use strict;
use FindBin;
use lib "$FindBin::Bin/../perllib";

use FileHandle;
use Geodetic::DMS

#===============================================================================
#
#   PACKAGE:     AdjTest
#
#   DESCRIPTION: Defines an AdjTest sql writing object
#
#===============================================================================

package AdjTest;

use vars qw/
   @regs
   %output_files
   @ObsTypes
   /;

#===============================================================================
#
#   SUBROUTINE:   new
#
#   DESCRIPTION:  Creates a new AdjTest object for writing an SQL file of
#                 adjustment data.
#
#   PARAMETERS:   $class      The perl class defined in the call to new
#                 $file       The name of the file to create
#                 $transsize  The maximum size of a  transaction (number
#                             of statements).
#
#   RETURNS:      The new AdjTest object
#
#   GLOBALS:
#
#===============================================================================

sub new {
    my ($class, $file ) = @_;
    my $self = {};
    bless $self, $class;
    $self->{rootname} = $file;

    $self->{crdfile} = new FileHandle(">$file.crd");
    $self->{obsfile} = new FileHandle(">$file.dat");
    $self->{cmdfile} = new FileHandle(">$file.snp");

    $self->{stations} = [];
    $self->{constraints} = [];

    $self->{obsdate}="1980-1-1 00:00:00";
    return $self;
}


#===============================================================================
#
#   SUBROUTINE:   close
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub close {
    my($self)=@_;
    $self->{crdfile}->close;
    $self->{obsfile}->close;
    $self->{cmdfile}->close;
 

    $self->{closed} = 1;
    }


#===============================================================================
#
#   SUBROUTINE:   DESTROY
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub DESTROY {
    my $self = shift;
    $self->close() if ! $self->{closed};
    }


#===============================================================================
#
#   SUBROUTINE:   GetAdjustmentMethodId
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub GetAdjustmentMethodId {
    my ($self, $method) = @_;
    if( $method eq 'LNZH' ) 
    {
        $self->AddConstraint("fix horizontal all");
    }
    elsif( $method eq 'LNZC' ) 
    {
        $self->AddConstraint("fix vertical all");
    }
    return 0;
}

sub AddConstraint {
   my ($self, $constraint ) = @_;
   push( @{$self->{constraints}}, $constraint );
   }

#===============================================================================
#
#   SUBROUTINE:   CreateAdjustment
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub AdjustmentId {
    my($self,$new) = @_;
    return 0;
    }
    

sub CreateAdjustment {
    my( $self, $cs, $method ) = @_;
    
    return 0;
}


#===============================================================================
#
#   SUBROUTINE:   AddAdjustmentParameter
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub AddAdjustmentParameter {
    my( $self, $code, $dflt, $description, $val ) = @_;

# Need to sort out coef codes mapping to adjustment commands

    return 0;
}



#===============================================================================
#
#   SUBROUTINE:   CreateObsTypes
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub CreateObsTypes {
  my ($self)= @_;
  }


#===============================================================================
#
#   SUBROUTINE:   CreateNode
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub CreateNode {
   my( $self, $code, $cs, $x, $y, $z, $fix, $corid, $sdc, $omit ) = @_;
   my $coords;
   $x += 0;
   $y += 0;
   $z += 0;
   if( $cs->{ort1} eq 'LATD' )
   {
       $coords = Geodetic::DMS::FormatDMS($x,6,"SN"). '  '.
                 Geodetic::DMS::FormatDMS($y,6,"WE"). '  '. 
                 sprintf("%8.4lf",$z);
   }
   elsif( $cs->{ort1} eq 'NRTH' )
   {
       $coords = sprintf("%.4d %.4d %.4d",$y,$x,$z);
   }
   else
   {
      $coords = "0.0 0.0 $z";
   }

   my $station = "$code $coords Node $code";
   push( @{$self->{stations}}, $station );

   my $sdc_status = $sdc ? 'Y' : 'N';

   if( $fix eq 'FIX3' ) $self->AddConstraint("fix $code");
   if( $fix eq 'FIXH' || $fix eq 'FXHX' ) $self->AddConstraint("fix horizontal $code");
   if( $fix eq 'FIXV' || $fix eq 'FXVX' ) $self->AddConstraint("fix vertical $code");
   return $code;
}


#===============================================================================
#
#   SUBROUTINE:   ClearSetupList
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub ClearSetupList {
   my ($self) = @_;;
   $self->{IdSetup} = {};
}


#===============================================================================
#
#   SUBROUTINE:   CreateSetup
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub CreateSetup {
   my ($self,$code) = @_;
   my $idnod = $self->{IdNod}->{$code};
   die "Aborting: Undefined code $code for setup\n" if ! $idnod;
   return $idnod;
}


#===============================================================================
#
#   SUBROUTINE:   SetObsDate
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub SetObsDate {
   my( $self, $date ) = @_;
   $date =~ /(\d+)\-(\d+)\-(\d+)/;
   $self->{obsdate} = "$3-$2-$1 00:00:00";
}


#===============================================================================
#
#   SUBROUTINE:   CreateObsSet
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub CreateObsSet {
   my( $self, $type ) = @_;
   my $obs_id = $self->NextId("OBS");
   $self->{obs_id} = $obs_id;
   }


#===============================================================================
#
#   SUBROUTINE:   ClearObsSet
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub ClearObsSet {
   my( $self ) = @_;
   delete $self->{obs_id};
   }

#===============================================================================
#
#   SUBROUTINE:   CreateObs
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub CreateObs {
   my( $self, $from, $to, $type, $value, $error, $csdef, $exclude, $accmult, $obsclass, $srvclass ) = @_;
   my $idloc = $from;
   my $idrem = $to;

   die "Observation type $type not defined\n" if ! $self->{ObsType}->{$type};
   my $obstype = $self->{ObsType}->{$type};

   print "Skipping non-SNAP obs type $type\n" if ! $obstype->{code};

   $value = [$value] if ref($value) ne 'ARRAY';
   my $id = $self->NextId("OBN");
   my $fh = $self->{fh}->{obn};
   my $idcs = $csdef->{id};
   my $wrkid = $self->{Survey};

   my $adjid =$self->AdjustmentId;
   $exclude = ($exclude eq '') ? "N" : "Y";
   $accmult += 0.0;
   $accmult = 1.0 if ! $accmult;

   my ($stype,$oet1,$oet2,$oet3) = 
      @{$self->{ObsType}->{$type}}{'sub_type','oet_type_1','oet_type_2','oet_type_3'};
   my ($val1,$val2,$val3) = @$value[0,1,2];
   my $obsdate = $self->{obsdate};
   my $obsid = $self->{obs_id};
   $srvclass = uc($srvclass);
   $srvclass = 'MEAS' if $srvclass eq '';

   if( $adjid ) {
      my $data = "$adjid|$id|$obsid|$wrkid|$idcs|$idloc|$idrem|$stype|".
                 "$oet1|$oet2|$oet3|$val1|$val2|$val3|".
                 "||$exclude|AUTH|$obsclass|$srvclass|$accmult|$obsdate\n";
      if( $obsid ) {
         push(@{$self->{stored_obn}}, $data );
         }
      else {
         print $fh $data;
         }
   
      }

   if( $adjid && defined($error) ) {
      $error = [$error*$error] if ref($error) ne 'ARRAY';
      $fh = $self->{fh}->{oba};
      my $data = sprintf "$adjid|$id||$id||%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
                @$error;
      if( $obsid ) {
         push(@{$self->{stored_oba}}, $data );
         }
      else {
         print $fh $data;
         }
   
      }

   $self->SetNodePurpose( $from );
   $self->SetNodePurpose( $to ) if defined($to);

   return $id;
}

#===============================================================================
#
#   SUBROUTINE:   CreateCovariance
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub CreateCovariance {
   my( $self, $id1, $id2, $covar ) = @_; 
   die "Invalid covariance data id1($id1) > id2($id2)\n" if $id1 > $id2;
   my $fh = $self->{fh}->{oba};
   my $adjid =$self->AdjustmentId;
   my $obsid = $self->{obs_id};
   my $data = sprintf "$adjid|$id1|$obsid|$id2|$obsid|%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
                @$covar;
   if( $obsid ) {
      push(@{$self->{stored_oba}}, $data );
      }
   else {
      print $fh $data;
      }
}



#===============================================================================
#
#   SUBROUTINE:   CreateArcObs
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub CreateArcObs {
   my( $self, $from, $to, $dval, $derr, $bval, $berr, 
       $rad, $dir, $csdef, $exclude, $accmult, $obsclass, $srvclass ) = @_;

   my $idloc = $self->CreateSetup( $from );
   my $idrem = $self->CreateSetup( $to );

   my $type = 'ARCO';
   my $cdir = $dir =~ /^l/i ? 'LEFT' : 'RGHT';
   my $value = [$bval,$dval];

   my $id = $self->NextId("OBN");
   my $fh = $self->{fh}->{obn};
   my $idcs = $csdef->{id};
   my $wrkid = $self->{Survey};

   my $adjid =$self->AdjustmentId;
   $exclude = ($exclude eq '') ? "N" : "Y";
   $accmult += 0.0;
   $accmult = 1.0 if ! $accmult;
   $srvclass = uc($srvclass);
   $srvclass = 'MEAS' if $srvclass eq '';

   my ($stype,$oet1,$oet2,$oet3) = 
      @{$self->{ObsType}->{$type}}{'sub_type','oet_type_1','oet_type_2','oet_type_3'};
   my ($val1,$val2,$val3) = @$value[0,1,2];
   my $obsdate = $self->{obsdate};
   my $obsid = $self->{obs_id};

   if( $adjid ) {
      my $data = "$adjid|$id|$obsid|$wrkid|$idcs|$idloc|$idrem|$stype|".
                 "$oet1|$oet2|$oet3|$val1|$val2|$val3|".
                 "$rad|$cdir|$exclude|AUTH|$obsclass|$srvclass|$accmult|$obsdate\n";

      if( $obsid ) {
           push(@{$self->{stored_obn}},$data);
           }
      else {
           print $fh $data;
           }
      my $error = [$berr*$berr,undef,undef,undef,$derr*$derr];
      $fh = $self->{fh}->{oba};
      my $data = sprintf "$adjid|$id|$obsid|$id|$obsid|%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
                @$error;
      if( $obsid ) {
           push(@{$self->{stored_oba}},$data);
           }
      else {
           print $fh $data;
           }
      }

   $self->SetNodePurpose( $from );
   $self->SetNodePurpose( $to );

   return $id;
}

#===============================================================================
#
#   SUBROUTINE:   WritePendingObs
#
#   DESCRIPTION:  Writes observations that have been deferred as they are 
#                 part of an obsevation set. (To reflect the order that
#                 they will be retrieved from the database).
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub WritePendingObs {
   my ($self)=@_;
   my $fh = $self->{fh}->{obn};
   my $data = $self->{stored_obn};
   print $fh join('',@$data);

   $fh = $self->{fh}->{oba};
   $data = $self->{stored_oba};
   print $fh join('',@$data);
}


#===============================================================================
#
#   SUBROUTINE:   WriteRegs
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub WriteRegs {
   my ($self)=@_;
   my $fh = $self->{fh}->{reg};
   
   foreach (@regs) {
     print $fh "$_\n";
     }
}


#===============================================================================
#
#   SUBROUTINE:   WriteNodes
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub WriteNodes {
   my ($self)=@_;
   my $fh = $self->{fh}->{nod};
   my $wrkid = $self->{AdjustmentSurvey};
   
   foreach (sort keys %{$self->{nodelist}}) {
     my $nodedata = $self->{nodelist}->{$_};
     my $purpose = $self->{NodePurpose}->{$_};
     my $adopted = $self->{NodeAdopted}->{$_};
     print $fh "$nodedata|$wrkid|$purpose|$adopted\n";
     }
}


#===============================================================================
#
#   SUBROUTINE:   WriteAdjParams
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub WriteAdjParams {
   my ($self)=@_;
   my $fh = $self->{fh}->{prm};
   my $adjid = $self->AdjustmentId;

   foreach (@{$self->{AdjParams}}) {
     my ($code,$desc,$val,$seq) = 
         @$_{'COEF_CODE','DESCRIPTION','VALUE','SEQUENCE'};
     $val = $1 if $val =~ /^\"(.*)\"/;
     print $fh "$adjid|$code|$desc|$val|$seq\n";
     }
}

#===============================================================================
#
#   SUBROUTINE:   InitIds
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub InitIds {
   my ($self,$init)=@_;
   return if ! $self;
   $init = 10000 if ! $init;
   my $t;
   for $t (qw/NOD COO OBS OBN OBA ADM ADJ STP ADC SRV/ ) {
      $self->{nextid}->{$t} = $init;
      $init += 2000;
   }
}


#===============================================================================
#
#   SUBROUTINE:   NextId
#
#   DESCRIPTION:  
#
#   PARAMETERS:   
#
#   RETURNS:      
#
#   GLOBALS:
#
#===============================================================================

sub NextId {
   my ($self, $def ) = @_;
   my $id = $self->{nextid}->{$def};
   $self->{nextid}->{$def}++;
   return $id;
   }


#===============================================================================
#
#   SUBROUTINE:   CreateSurvey
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub CreateSurvey {
  my ($self, $srvtype, $srvid, $class, $date, $plantype ) = @_;
  my $id = $self->NextId("SRV");
  my $fh = $self->{fh}->{srv};
  my $adjid = $self->AdjustmentId;
  print $fh "$adjid|$id|$srvtype $srvid|$class|$date|$plantype\n";
  $self->{Survey} = $id;
  $self->ClearSetupList;
  if( ! $self->{AdjustmentSurvey} ) {
     %{$self->{NodePurpose}} = ();
     %{$self->{NodeAdopted}} = ();
     }
  return $id;
}



#===============================================================================
#
#   SUBROUTINE:   SetNodePurpose
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub SetNodePurpose {
  my ($self,$code,$purpose,$adopted) = @_;
  return if $self->{Survey} != $self->{AdjustmentSurvey};
  my $idnod = $self->{IdNod}->{$code};
  die "Aborting: Undefined node $code for purpose\n" if ! $idnod;
  return if $purpose eq '' && exists $self->{NodePurpose}->{$idnod};
  $purpose = 'TRAV' if ! $purpose;
  $self->{NodePurpose}->{$idnod} = $purpose;
  if( $adopted ne '' || ! $self->{NodeAdopted}->{$idnod} ) {
     $adopted = $adopted ? 'Y' : 'N';
     $self->{NodeAdopted}->{$idnod} = $adopted;
     }
  }

#===============================================================================
#
#   SUBROUTINE:   comment
#
#   DESCRIPTION:
#
#   PARAMETERS:
#
#   RETURNS:
#
#   GLOBALS:
#
#===============================================================================

sub comment {
   }



BEGIN {


 @ObsTypes = (
   { sub_type=>"THDR", code=>'HA', angle=>'', ndp=>2, },
   { sub_type=>"BEAR", code=>'PB', angle=>'', ndp=>2, },
   { sub_type=>"HZDI", code=>'HD', },
   { sub_type=>"DIST", code=>'SD', }, 
   { sub_type=>"SLDI", code=>'ED', },
   { sub_type=>"PLLH", },
   { sub_type=>"PXYZ", },
   { sub_type=>"DXYZ", code=>'GPS', vector=>1, },
   { sub_type=>"ARCO", code=>'', },
   { sub_type=>"DFHT", code=>'LV', }, 
   { sub_type=>"ORHT", code=>'', },
   { sub_type=>"ZDIS", code=>'ZD', angle=>'',ndp=>2,  },
   { sub_type=>"GAZM", code=>'AZ', angle=>'',ndp=>2,  },
 );

}

1;
