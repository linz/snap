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

use FileHandle;

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

    my $ofk;
    foreach $ofk (keys %output_files) {
       my $of = $output_files{$ofk};
       my $fh = $self->{fh}->{$ofk} = new FileHandle ">$file".$of->{name};
       die "Unable to create $of->{name}\n" if ! $fh;
       print $fh $of->{fields},"\n";
    }

    $self->{obsdate}="1980-1-1 00:00:00";
    $self->InitIds;
    $self->CreateObsTypes;
    $self->{stored_obn} = [];
    $self->{stored_oba} = [];
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
    my($sql)=@_;
    $sql->WritePendingObs;
    $sql->WriteRegs;
    $sql->WriteNodes;
    $sql->WriteAdjParams;
    $sql->{closed} = 1;
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
    my $sql = shift;
    $sql->close() if ! $sql->{closed};
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
    my ($sql, $method) = @_;
    my $idadm = $sql->{AdjustmentMethodId};
    my $methodtypes = {
       'cadastral' => { software=>'LNZC', type=>'CADA'}, 
       'geodetic' => { software=>'LNZG', type=>'GEOD'}, 
       'height' => { software=>'LNZH', type=>'HGHT'}, 
       };
    my $type  = $methodtypes->{lc($method)};
    if( ! $idadm ) {
        my %data = ( SOFTWARE_USED=>$type->{software},
                     TYPE=>$type->{type});
        $idadm = $sql->NextId("ADM");
        $sql->{ADM}->{idadm} =  \%data;
        $sql->{AdjustmentMethodId} = $idadm;
    }
    return $idadm;
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
    my($sql,$new) = @_;
    if( ! $sql->{AdjustmentId} || ($new && ! $sql->{AdjustmentIdIsNew}) ) {
       $sql->{AdjustmentId} = $sql->NextId("ADJ");
       $sql->{AdjustmentIdIsNew} = 1;
       }
    if( $new ) { $sql->{AdjustmentIdIsNew} = 0 };
    return $sql->{AdjustmentId};
    }
    

sub CreateAdjustment {
    my( $sql, $cs, $method ) = @_;
    
    my $idadm = $sql->GetAdjustmentMethodId( $method );
    my $id = $sql->AdjustmentId(1);
    my $fh = $sql->{fh}->{'adj'};
    my $adm = $sql->{ADM}->{idadm};
    $sql->{AdjustmentSurvey} = $sql->{Survey};

    printf $fh "%d|%d|%d|Test adjustment run|Test method|%s|%s\n",
        $id, $cs->{id}, $sql->{Survey}, $adm->{SOFTWARE_USED}, $adm->{TYPE};

    return $id;
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
    my( $sql, $code, $dflt, $description, $val ) = @_;
    my $idadm = $sql->GetAdjustmentMethodId;
    my $seq = ++($sql->{ADC_Sequence});
    my $id = $sql->NextId("ADC");

    my $param = {
       ADM_ID=>$idadm,
       COEF_CODE=>$code,
       VALUE=>($val ? $val : $dflt),
       DESCRIPTION=>$description,
       SEQUENCE=>$seq
       };
    $sql->{AdmCode}->{$code} = $param;
    push(@{$sql->{AdjParams}},$param );
    return $id;
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
  my ($sql)= @_;
  foreach (@ObsTypes) {
    $sql->{ObsType}->{$_->{sub_type}} = $_;
    }
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
   my( $sql, $code, $cs, $x, $y, $z, $fix, $corid, $sdc, $omit ) = @_;
   $x = '' if $cs->{ort1} eq '' || $x eq '-';
   $y = '' if $cs->{ort2} eq '' || $y eq '-';
   $z = '' if $cs->{ort3} eq '' || $z eq '-';
   my $id = $sql->NextId("NOD");
   my $idcoo = $sql->NextId("COO");
   $sql->{IdCoo}->{$id} = $idcoo;
   $sql->{IdNod}->{$code} = $id;
   my $adjid =$sql->AdjustmentId;
   my $sdc_status = $sdc ? 'Y' : 'N';
   if( $adjid  && ! $omit ) {
      $fix = 'FREE' if $fix eq '';
      $fix = uc($fix);
      $fix = 'FIX3' if $fix !~ /^(FIX3|FIXV|FIXH|FREE|FREX|FXHX|FXVX)$/;

      $sql->{nodelist}->{$id} = 
          sprintf("%d|%d|%d|$code|%d|$corid|$sdc_status|$x|$y|$z|$fix",
             $adjid,$id,$idcoo,$cs->{id});
   }
   return $id;
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
   my ($sql) = @_;;
   $sql->{IdSetup} = {};
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
   my ($sql,$code) = @_;
   my $idnod = $sql->{IdNod}->{$code};
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
   my( $sql, $date ) = @_;
   $date =~ /(\d+)\-(\d+)\-(\d+)/;
   $sql->{obsdate} = "$3-$2-$1 00:00:00";
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
   my( $sql, $type ) = @_;
   my $obs_id = $sql->NextId("OBS");
   $sql->{obs_id} = $obs_id;
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
   my( $sql ) = @_;
   delete $sql->{obs_id};
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
   my( $sql, $from, $to, $type, $value, $error, $csdef, $exclude, $accmult, $obsclass, $srvclass ) = @_;
   my $idloc = $sql->CreateSetup( $from );
   my $idrem = $sql->CreateSetup( $to ) if defined($to);
   die "Observation type $type not defined\n" if ! $sql->{ObsType}->{$type};
   $value = [$value] if ref($value) ne 'ARRAY';
   my $id = $sql->NextId("OBN");
   my $fh = $sql->{fh}->{obn};
   my $idcs = $csdef->{id};
   my $wrkid = $sql->{Survey};

   my $adjid =$sql->AdjustmentId;
   $exclude = ($exclude eq '') ? "N" : "Y";
   $accmult += 0.0;
   $accmult = 1.0 if ! $accmult;

   my ($stype,$oet1,$oet2,$oet3) = 
      @{$sql->{ObsType}->{$type}}{'sub_type','oet_type_1','oet_type_2','oet_type_3'};
   my ($val1,$val2,$val3) = @$value[0,1,2];
   my $obsdate = $sql->{obsdate};
   my $obsid = $sql->{obs_id};
   $srvclass = uc($srvclass);
   $srvclass = 'MEAS' if $srvclass eq '';

   if( $adjid ) {
      my $data = "$adjid|$id|$obsid|$wrkid|$idcs|$idloc|$idrem|$stype|".
                 "$oet1|$oet2|$oet3|$val1|$val2|$val3|".
                 "||$exclude|AUTH|$obsclass|$srvclass|$accmult|$obsdate\n";
      if( $obsid ) {
         push(@{$sql->{stored_obn}}, $data );
         }
      else {
         print $fh $data;
         }
   
      }

   if( $adjid && defined($error) ) {
      $error = [$error*$error] if ref($error) ne 'ARRAY';
      $fh = $sql->{fh}->{oba};
      my $data = sprintf "$adjid|$id||$id||%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
                @$error;
      if( $obsid ) {
         push(@{$sql->{stored_oba}}, $data );
         }
      else {
         print $fh $data;
         }
   
      }

   $sql->SetNodePurpose( $from );
   $sql->SetNodePurpose( $to ) if defined($to);

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
   my( $sql, $id1, $id2, $covar ) = @_; 
   die "Invalid covariance data id1($id1) > id2($id2)\n" if $id1 > $id2;
   my $fh = $sql->{fh}->{oba};
   my $adjid =$sql->AdjustmentId;
   my $obsid = $sql->{obs_id};
   my $data = sprintf "$adjid|$id1|$obsid|$id2|$obsid|%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
                @$covar;
   if( $obsid ) {
      push(@{$sql->{stored_oba}}, $data );
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
   my( $sql, $from, $to, $dval, $derr, $bval, $berr, 
       $rad, $dir, $csdef, $exclude, $accmult, $obsclass, $srvclass ) = @_;

   my $idloc = $sql->CreateSetup( $from );
   my $idrem = $sql->CreateSetup( $to );

   my $type = 'ARCO';
   my $cdir = $dir =~ /^l/i ? 'LEFT' : 'RGHT';
   my $value = [$bval,$dval];

   my $id = $sql->NextId("OBN");
   my $fh = $sql->{fh}->{obn};
   my $idcs = $csdef->{id};
   my $wrkid = $sql->{Survey};

   my $adjid =$sql->AdjustmentId;
   $exclude = ($exclude eq '') ? "N" : "Y";
   $accmult += 0.0;
   $accmult = 1.0 if ! $accmult;
   $srvclass = uc($srvclass);
   $srvclass = 'MEAS' if $srvclass eq '';

   my ($stype,$oet1,$oet2,$oet3) = 
      @{$sql->{ObsType}->{$type}}{'sub_type','oet_type_1','oet_type_2','oet_type_3'};
   my ($val1,$val2,$val3) = @$value[0,1,2];
   my $obsdate = $sql->{obsdate};
   my $obsid = $sql->{obs_id};

   if( $adjid ) {
      my $data = "$adjid|$id|$obsid|$wrkid|$idcs|$idloc|$idrem|$stype|".
                 "$oet1|$oet2|$oet3|$val1|$val2|$val3|".
                 "$rad|$cdir|$exclude|AUTH|$obsclass|$srvclass|$accmult|$obsdate\n";

      if( $obsid ) {
           push(@{$sql->{stored_obn}},$data);
           }
      else {
           print $fh $data;
           }
      my $error = [$berr*$berr,undef,undef,undef,$derr*$derr];
      $fh = $sql->{fh}->{oba};
      my $data = sprintf "$adjid|$id|$obsid|$id|$obsid|%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
                @$error;
      if( $obsid ) {
           push(@{$sql->{stored_oba}},$data);
           }
      else {
           print $fh $data;
           }
      }

   $sql->SetNodePurpose( $from );
   $sql->SetNodePurpose( $to );

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
   my ($sql)=@_;
   my $fh = $sql->{fh}->{obn};
   my $data = $sql->{stored_obn};
   print $fh join('',@$data);

   $fh = $sql->{fh}->{oba};
   $data = $sql->{stored_oba};
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
   my ($sql)=@_;
   my $fh = $sql->{fh}->{reg};
   
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
   my ($sql)=@_;
   my $fh = $sql->{fh}->{nod};
   my $wrkid = $sql->{AdjustmentSurvey};
   
   foreach (sort keys %{$sql->{nodelist}}) {
     my $nodedata = $sql->{nodelist}->{$_};
     my $purpose = $sql->{NodePurpose}->{$_};
     my $adopted = $sql->{NodeAdopted}->{$_};
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
   my ($sql)=@_;
   my $fh = $sql->{fh}->{prm};
   my $adjid = $sql->AdjustmentId;

   foreach (@{$sql->{AdjParams}}) {
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
   my ($sql,$init)=@_;
   return if ! $sql;
   $init = 10000 if ! $init;
   my $t;
   for $t (qw/NOD COO OBS OBN OBA ADM ADJ STP ADC SRV/ ) {
      $sql->{nextid}->{$t} = $init;
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
   my ($sql, $def ) = @_;
   my $id = $sql->{nextid}->{$def};
   $sql->{nextid}->{$def}++;
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
  my ($sql, $srvtype, $srvid, $class, $date, $plantype ) = @_;
  my $id = $sql->NextId("SRV");
  my $fh = $sql->{fh}->{srv};
  my $adjid = $sql->AdjustmentId;
  print $fh "$adjid|$id|$srvtype $srvid|$class|$date|$plantype\n";
  $sql->{Survey} = $id;
  $sql->ClearSetupList;
  if( ! $sql->{AdjustmentSurvey} ) {
     %{$sql->{NodePurpose}} = ();
     %{$sql->{NodeAdopted}} = ();
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
  my ($sql,$code,$purpose,$adopted) = @_;
  return if $sql->{Survey} != $sql->{AdjustmentSurvey};
  my $idnod = $sql->{IdNod}->{$code};
  die "Aborting: Undefined node $code for purpose\n" if ! $idnod;
  return if $purpose eq '' && exists $sql->{NodePurpose}->{$idnod};
  $purpose = 'TRAV' if ! $purpose;
  $sql->{NodePurpose}->{$idnod} = $purpose;
  if( $adopted ne '' || ! $sql->{NodeAdopted}->{$idnod} ) {
     $adopted = $adopted ? 'Y' : 'N';
     $sql->{NodeAdopted}->{$idnod} = $adopted;
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
   { sub_type=>"THDR", description=>"Theodolite Direction", oet_type_1=>"THDR"},
   { sub_type=>"BEAR", description=>"Bearing", oet_type_1=>"BEAR" },
   { sub_type=>"HZDI", description=>"Distance - Horizontal", oet_type_1=>"HZDI" },
   { sub_type=>"DIST", description=>"Distance - Slope", oet_type_1=>"DIST" },
   { sub_type=>"SLDI", description=>"Distance - Sea level", oet_type_1=>"SLDI" },
   { sub_type=>"PLLH", description=>"GPS Point Position (Lat/Long/Hgt)", 
       oet_type_1=>"LATD", oet_type_2=>"LONG", oet_type_3=>"HGHT"},
   { sub_type=>"PXYZ", description=>"GPS Point Position (X,Y,Z)", 
       oet_type_1=>"X", oet_type_2=>"Y", oet_type_3=>"Z"},
   { sub_type=>"DXYZ", description=>"Vector difference (XYZ)", 
       oet_type_1=>"DX", oet_type_2=>"DY", oet_type_3=>"DZ"},
   { sub_type=>"ARCO", description=>"Arc", 
       oet_type_1=>"ARCB", oet_type_2=>"ARCL" },
   { sub_type=>"DFHT", description=>"Height Difference", oet_type_1=>"DFHT" },
   { sub_type=>"ORHT", description=>"Orthometric height", oet_type_1=>"ORHT" },
   { sub_type=>"ZDIS", description=>"Zenith Distance", oet_type_1=>"ZDIS" },
   { sub_type=>"GAZM", description=>"Geodetic Azimuth", oet_type_1=>"GAZM" }
 );

%output_files = (
adj=> {
 name=>"_getadj.csv",
 fields=>'ADJ_ID|COS_ID|WRK_ID|DESCRIPTION|METHOD_NAME|SOFTWARE_USED|METHOD_TYPE'
 },
prm=> {
 name=>"_getadjprm.csv",
 fields=>'ADJ_ID|COEF_CODE|DESCRIPTION|COEF_VALUE|SEQUENCE'
 },
nod=> {
 name=>"_getnode.csv",
 fields=>'ADJ_ID|NOD_ID|COO_ID_SOURCE|MARK_NAME|COS_ID|COR_ID|SDC_STATUS|VALUE1|VALUE2|VALUE3|COO_CONSTRAINT|WRK_ID|PURPOSE|ADOPTED'
 },
obn=> {
 name=>"_getobn.csv",
 fields=>'ADJ_ID|OBN_ID|OBS_ID|WRK_ID|COS_ID|NOD_ID_LOCAL|NOD_ID_REMOTE|SUB_TYPE|OET_TYPE_1|OET_TYPE_2|OET_TYPE_3|VALUE1|VALUE2|VALUE3|ARC_RADIUS|ARC_DIRECTION|EXCLUDE|ORIG_STATUS|CADASTRAL_CLASS|SURVEYED_CLASS|ACC_MULTIPLIER|REF_DATETIME'
 },
oba=> {
 name=>"_getoba.csv",
 fields=>'ADJ_ID|OBN_ID1|OBS_ID1|OBN_ID2|OBS_ID2|VALUE_11|VALUE_12|VALUE_13|VALUE_21|VALUE_22|VALUE_23|VALUE_31|VALUE_32|VALUE_33'
},
srv=> {
 name=>"_getsrv.csv",
 fields=>'ADJ_ID|WRK_ID|SURVEY_NUMBER|SURVEY_CLASS|SURVEY_DATE|TYPE_OF_DATASET'
},
reg=> {
 name=>"_getregs.csv",
 fields=>'CODE|DESCRIPTION|CHAR_VALUE'
}
);

@regs = (
   "1000|name|1999 Survey regulations",
   "1001|Reg 26.2.a.i: Relative accuracy of boundary marks|vector from BNDY,ORBD,WIBD, ORWB to BNDY,ORBD,WIBD, ORWB require all meeting tolerance 0.03 m 0.01 m/100m",
   "1002|Reg 26.2.a.ii and 13.a.i: Relative accuracy and proximity of boundary marks to witness marks|vector from BNDY,ORBD,WIBD, ORWB to WITN,ORWI,WIBD,ORWB require 1 within 125 m meeting tolerance 0.03 m",
   "1003|Reg 26.2.a.iii: Relative accuracy of boundary marks to origins|vector from BNDY,ORBD,WIBD, ORWB to ORIG, ORWI, ORBD, ORWB require all meeting tolerance 0.03 m 0.01 m/100m",
   "1004|Reg 26.2.a.iv: Relative accuracy of witness/traverse/origin marks|vector from ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV to ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV require all meeting tolerance 0.02 m 0.01 m/100m",
   "1005|Reg 13.a.ii: Proximity of natural boundary fix to witness marks|vector from NATB to WITN,ORWI,WIBD,ORWB require 1 within  250 m",
   "1006|Reg 28 and 26.2.a.i: Misclose of obs between boundary marks|misclose from BNDY,ORBD,WIBD, ORWB to BNDY,ORBD,WIBD, ORWB less than 0.03 m 0.01 m/100m",
   "1007|Reg 28 and 26.2.a.ii: Misclose of obs from boundary marks to witness marks|misclose from BNDY,ORBD,WIBD, ORWB to WITN,ORWI,WIBD,ORWB less than 0.03 m within 125 m",
   "1008|Reg 28 and 26.2.a.iii: Misclose of obs from boundary marks to origins|misclose from BNDY,ORBD,WIBD, ORWB to ORIG, ORWI, ORBD, ORWB less than 0.03 m 0.01 m/100m",
   "1009|Reg 28 and 26.2.a.iv: Misclose of obs between witness/traverse/origin marks|misclose from ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV to ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV less than 0.02 m 0.01 m/100m",
   "2000|name|1999 Survey regulations",
   "2001|Reg 26.2.b.i: Relative accuracy of boundary marks|vector from BNDY,ORBD,WIBD, ORWB to  BNDY,ORBD,WIBD, ORWB require all meeting tolerance 0.1 m 0.01 m/100m",
   "2002|Reg 26.2.b.ii and 13.b.i: Relative accuracy and proximity of boundary marks to witness marks|vector from BNDY,ORBD,WIBD, ORWB to WITN,ORWI,WIBD,ORWB require 1 within 250 m meeting tolerance 0.06 m",
   "2003|Reg 26.2.b.iii: Relative accuracy of boundary marks to origins|vector from BNDY,ORBD,WIBD, ORWB to ORIG, ORWI, ORBD, ORWB require all meeting tolerance 0.06 m 0.01 m/100m",
   "2004|Reg 26.2.b.iv: Relative accuracy of witness/traverse/origin marks|vector from ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV to ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV require all meeting tolerance 0.02 m 0.01 m/100m",
   "2005|Reg 13.b.ii: Proximity of natural boundary fix to witness marks|vector from NATB to WITN,ORWI,WIBD,ORWB require 1 within 500 m",
   "2006|Reg 28 and 26.2.b.i: Misclose of obs between boundary marks|misclose from BNDY,ORBD,WIBD, ORWB to  BNDY,ORBD,WIBD, ORWB less than 0.1 m 0.01 m/100m",
   "2007|Reg 28 and 26.2.b.ii: Misclose of obs from boundary marks to witness marks|misclose from BNDY,ORBD,WIBD, ORWB to WITN,ORWI,WIBD,ORWB less than 0.06 m within 250 m",
   "2008|Reg 28 and 26.2.b.iii: Misclose of obs from boundary marks to origins|misclose from BNDY,ORBD,WIBD, ORWB to ORIG, ORWI, ORBD, ORWB less than 0.06 m 0.01 m/100m",
   "2009|Reg 28 and 26.2.b.iv: Misclose of obs between witness/traverse/origin marks|misclose from ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV to ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV less than 0.02 m 0.01 m/100m",
   "3000|name|1999 Survey regulations",
   "3001|Reg 26.2.c.i: Relative accuracy of boundary marks|vector from BNDY,ORBD,WIBD, ORWB to BNDY,ORBD,WIBD, ORWB require all meeting tolerance 0.25 m 0.01 m/100m",
   "3002|Reg 26.2.c.ii and 13.b.i: Relative accuracy and proximity of boundary marks to witness marks|vector from BNDY,ORBD,WIBD, ORWB to WITN,ORWI,WIBD,ORWB require 1 within 250 m meeting tolerance 0.13 m",
   "3003|Reg 26.2.c.iii: Relative accuracy of boundary marks to origins|vector from BNDY,ORBD,WIBD, ORWB to ORIG, ORWI, ORBD, ORWB require all meeting tolerance 0.13 m 0.01 m/100m",
   "3004|Reg 26.2.c.iv: Relative accuracy of witness/traverse/origin marks|vector from ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV to ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV require all meeting tolerance 0.02 m 0.01 m/100m",
   "3005|Reg 13.b.ii: Proximity of natural boundary fix to witness marks|vector from NATB to WITN,ORWI,WIBD,ORWB require 1 within 500 m",
   "3006|Reg 28 and 26.2.c.i: Misclose of obs between boundary marks|misclose from BNDY,ORBD,WIBD, ORWB to BNDY,ORBD,WIBD, ORWB less than 0.25 m 0.01 m/100m",
   "3007|Reg 28 and 26.2.c.ii: Misclose of obs from boundary marks to witness marks|misclose from BNDY,ORBD,WIBD, ORWB to WITN,ORWI,WIBD,ORWB less than 0.13 m within 250 m",
   "3008|Reg 28 and 26.2.c.iii: Misclose of obs from boundary marks to origins|misclose from BNDY,ORBD,WIBD, ORWB to ORIG, ORWI, ORBD, ORWB less than 0.13 m 0.01 m/100m",
   "3009|Reg 28 and 26.2.c.iv: Misclose of obs between witness/traverse/origin marks|misclose from ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV to ORIG, WITN, ORWI, ORBD, WIBD, ORWB, TRAV less than 0.02 m 0.01 m/100m",
   "6000|name|Invented Survey regulations",
   "6001|Vector regulation no adopted|vector from ORIG, BNDY to ORIG, BNDY require all meeting tolerance 0.25 m 0.01 m/100m ignore for adopted",
   "6002|Node regulation no adopted|vector from ORIG, BNDY to ORIG, BNDY require 1 within 250 m meeting tolerance 0.13 m ignore for adopted",
   "6003|Misclose regulation no adopted|misclose from ORIG, BNDY to ORIG, BNDY less than 0.25 m 0.01 m/100m ignore for adopted",
   "6004|Vector regulation adopted only|vector from ORIG, BNDY to ORIG, BNDY require all meeting tolerance 0.25 m 0.01 m/100m only for adopted",
   "6005|Node regulation adopted only|vector from ORIG, BNDY to ORIG, BNDY require 1 within 250 m meeting tolerance 0.13 m only for adopted",
   "6006|Misclose regulation adopted only|misclose from ORIG, BNDY to ORIG, BNDY less than 0.25 m 0.01 m/100m only for adopted"
   );


}

1;
