#!/usr/local/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Script takes a ###.adj file defining a test dataset for
#                      the cadastral network adjustment function and generates
#                      test data (as SQL INSERT statements) 
#
# PARAMETERS:          def_file   The name of the file containing the adjustment
#                                 definition
#                      sql_file   The generated file of sql data
#                      trans_size The transaction size (optional)
#                      
# DEPENDENCIES:        AdjSQL.pm
#                      AdjTest.pm   (for creating file based test files)
#                      Geodetic.pm
#                      SQL_Interface.pm
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook          19/06/1999  Created
#===============================================================================


my $lib = $0;
if( $lib =~ s/[\/\\][^\\\/]+$// ) {
   push( @INC, $lib );
   }

require Geodetic;

if( lc($ARGV[0]) eq '-f' ) {
   shift(@ARGV);
   $filesys = 1;
   require 'AdjTest.pm';
   }
else {
   require 'AdjSQL.pm';
   }

@ARGV>=2 || die "Require parameters def_file sql_file [transaction_size]\n";

my($deffile, $sqlfile) = @ARGV;

open(DEF,$deffile) || die "Cannot open $deffile\n";

if( $filesys ) {
   $sql = new AdjTest( $sqlfile );
   }
else {
   $sql = new AdjSQL( $sqlfile, $ARGV[2] );
   }

my $method='cadastral';

while(<DEF>) {
  next if /^\s*(\#|$)/;
  s/[\r\n]//g;
  # Allow continuation onto following lines by ending with \
  while( /\s+\\\s*$/ ) { $_ = $`.' '.<DEF>; }
  eval {

    if( /^\s*trace\s+(\S+)\s*$/i ) {
		 $sql->comment("TRACE=$1");
		 next;
		 }

    if( /^\s*obs_class\s+(\S+)\s*$/i ) {
       $ObsClass = $1;
       next;
       }

    if( /^\s*crdsys\s/i ) {
        die "Crdsys cannot be repeated\n" if $csdef;
        die "Invalid crdsys\n" if ! /^\s*crdsys\s+(\d+)\s+(\S+)/;
        ($csdef,$csprj) = &CreateCrdsys( $1, $2 );
        ($file_csdef, $file_csprj ) = ($csdef, $csprj );
        ($node_csdef, $node_csprj ) = ($csdef, $csprj );
        ($obs_csdef, $obs_csprj ) = ($csdef, $csprj );
        next;
    }

    if( /^\s*file_crdsys\s/i) {
        die "Invalid file_crdsys\n" if ! /^\s*file_crdsys\s+(\d+)\s+(\S+)/;
        ($file_csdef, $file_csprj) = &CreateCrdsys( $1, $2 );
        next;
    }

    if( /^\s*node_crdsys\s/i) {
        die "Invalid node_crdsys\n" if ! /^\s*node_crdsys\s+(\d+)\s+(\S+)/;
        ($node_csdef, $node_csprj) = &CreateCrdsys( $1, $2 );
        next;
    }

    if( /^\s*obs_crdsys\s/i) {
        die "Invalid obs_crdsys\n" if ! /^\s*obs_crdsys\s+(\d+)\s+(\S+)/;
        ($obs_csdef, $obs_csprj) = &CreateCrdsys( $1, $2 );
        next;
    }

    if( /^\s*survey\s/i ) {
        die "Invalid survey definition\n" if ! /^\s*survey\s+([A-Z]+)(\S+)\s+(\d)\s+(\S+)(?:\s+(\S+))?/i;
        ($srvtype,$srvid,$srvclass,$srvdate,$plantype)=($1,$2,$3,$4,$5);
        $plantype = "SRVY" if $plantype eq '';
        $survey = $sql->CreateSurvey($srvtype,$srvid,$srvclass,$srvdate,$plantype);
        $ObsClass = $srvclass;
        $sql->SetObsDate($srvdate);
        next;
    }
    if( /^\s*start_id\s/i ) {
        die "Invalid start id definition\n" if ! /^\s*start_id\s*(\d+)/i;
        $sql->InitIds($1) if ! $started;
        next;
    }
    if( /^\s*adjustment_method\s+(cadastral|geodetic|height)/i ) {
        $method = lc($1);
        if( $method ne 'cadastral' ) {
           $obsdate = '1/1/1999' if ! $obsdate;
           $survey = $sql->CreateSurvey('DP',99999,1,$obsdate,'UNKN');
           }
        next;
        }

    die "Need coordinate system before anything else\n"
      if ! $csdef;
    die "Need survey before anything else\n"
      if ! $survey;
    if( ! $started ) {
        $sql->CreateAdjustment( $csdef, $method );
        $started = 1;
    }

    if( /^\s*parameter\s/i ) {
       die "Invalid parameter spec\n" if !
         /^\s*parameter\s+
            (\S+)\s+
            ([^\s\"]\S*|\"[^\"]*\")
            (?:\s+user\s+
             ([^\s\"]\S*|\"[^\"]*\")
             )?
            \s+(.*$)/xi;
       ($code,$dflt,$val,$desc)=($1,$2,$3,$4);
       if( $dflt =~ /^\"/) { $dflt = substr($dflt,1); chop($dflt);}
       if( $val =~ /^\"/) { $val = substr($val,1); chop($val);}
       $sql->AddAdjustmentParameter($code,$dflt,$desc,$val);
       next;
    }

    if( /^\s*node_purpose\s/i ) {
       die "Invalid node purpose\n" if ! /^\s*node_purpose\s+(\S+)\s+(\S.*)/i;
       $purpose=$1;
       @nodes = split(' ',$2);
       foreach (@nodes) { $sql->SetNodePurpose( $_, $purpose ); }
       next;
       }

    if( /^\s*node\s/i) {
       die "Invalid node spec\n" if !
          /^\s*node
            \s+(\S+)
            \s+([\d\-\+\.]+|\-)
            \s+([\d\-\+\.]+|\-)
            (?:\s+([\d\-\+\.]+|\-))?
            (?:\s+(fixv|fixh|fix|frex|fxhx|fxvx))?
            (?:\s+(omit))?
            (?:\s+o\=(\d+))?
            (?:\s+(sdc))?
            (?:\s+(adopted))?
            (?:\s+(\S+))?
            /xi;
       ($code,$x,$y,$z,$fix,$omit,$cor_id,$sdc,$adopted,$purpose)
             =($1,$2,$3,$4,$5,$6,$7,$8,$9,$10);
       ($y,$x) = $file_csprj->geog($y,$x) if defined($file_csprj);
       ($y,$x) = $node_csprj->proj($y,$x) if defined($node_csprj);
       $sdc = $sdc eq '' ? 0 : 1;
       $sql->CreateNode( $code, $node_csdef, $x, $y, $z, $fix, $cor_id, 
                         $sdc, $omit );
       $sql->SetNodePurpose( $code, $purpose,$adopted ) 
          if ($purpose || $adopted) && $method eq 'cadastral';
       next;
    }

    if( /^\s*date\s/i) {
       die "Invalid date - format dd-mm-yyyy" if !
          /^\s*date\s+(\d+\-\d+\-\d+)\s*$/i;
       $sql->SetObsDate( $1 );
       next;
       }


    if( /^\s*obs\s/i ) {
       die "Invalid observation\n" if !
          /^\s*obs
            \s+(\S+)
            \s+(\S+)
            \s+(\S+)
            \s+([\d\.\-]+)
            \s+([\d\.\-]+)
            (?:\s+([\d\.\-]+))?
            (?:\s+(CALC|MEAS|ADPT|PSED))?
            (?:\s+(\S+))?
            /xi;
       ($from,$to,$type,$val,$err,$accmult,$srvclass,$exclude)=($1,$2,$3,$4,$5,$6,$7,$8);
       $to = undef if $to eq '-';
       $sql->CreateObs( $from, $to, $type, $val, $err, $obs_csdef, $exclude, $accmult, $ObsClass, $srvclass );
       next;
    }

    if( /^\s*arc\s/i ) {
       die "Invalid arc observation\n" if !
          /^\s*arc
            \s+(\S+)
            \s+(\S+)
            \s+(A?[\d\.\-]+)
            \s+([\d\.\-]+)
            \s+([\d\.\-]+)
            \s+([\d\.\-]+)
            \s+([\d\.\-]+)
            \s+(\S+)
            (?:\s+([\d\.\-]+))?
            (?:\s+(CALC|MEAS|ADPT|PSED))?
            (?:\s+(\S+))?
            /xi;
       ($from,$to,$dval,$derr,$bval,$berr,$rad,$dir,$accmult,$srvclass,$exclude)=($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11);

       if( $dval =~ /a/i ) {
          $dval = substr($dval,1);
          }
       else {
          if( $dval >= 2.0*$rad ) { die "Invalid chord distance for arc radius\n"; }
          $dval /= 2;
          $dval = 2.0*$rad*atan2( $dval, sqrt($rad*$rad-$dval*$dval) );
          }
       $sql->CreateArcObs( $from, $to, $dval, $derr, $bval, $berr, $rad, $dir, $obs_csdef, $exclude, $accmult, $ObsClass, $srvclass );
       next;
    }

    if( /^\s*obs_set\b/i ) {
       die "Invalid obs_set command - already in a set\n" if $inset;
       die "Invalid obs_set command\n" if ! /^\s*obs_set\s+(\S+)/;
       $sql->CreateObsSet( $1 );
       $inset = 1;
       next;
       }

    if( /^\s*end_set/ ) {
       die "Vector covariance missing\n" if @vector;
       $sql->ClearObsSet();
       $inset = 0;
       next;
       }

    if( /^\s*vector\s/ && ! $inset) {
       die "Invalid vector observation\n$_\n" if !
          /^\s*vector
            \s+(\S+)
            \s+(\S+)
            \s+(\S+)

            \s+([\d\.\-]+)
            \s+([\d\.\-]+)
            \s+([\d\.\-]+)

            \s+([\d\.E\-]+)
            \s+([\d\.E\-]+)
            \s+([\d\.E\-]+)
            \s+([\d\.E\-]+)
            \s+([\d\.E\-]+)
            \s+([\d\.E\-]+)

            (?:\s+([\d\.\-]+))?
            (?:\s+(CALC|MEAS|ADPT|PSED))?
            (?:\s+(\S+))?
            /xi;
       my $to = $2;
       $to = undef if $to eq '-';
       $sql->CreateObs( $1, $to, $3, [$4,$5,$6],[$7,$8,$10,$8,$9,$11,$10,$11,$12],$obs_csdef,$15,$13,$ObsClass,$14);
       next;
       }


    if( /^\s*vector\s/ && $inset ) {
       die "Invalid vector observation\n" if !
          /^\s*vector
            \s+(\S+)
            \s+(\S+)
            \s+(\S+)

            \s+([\d\.\-]+)
            \s+([\d\.\-]+)
            \s+([\d\.\-]+)

            (?:\s+([\d\.\-]+))?
            (?:\s+(CALC|MEAS|ADPT|PSED))?
            (?:\s+(\S+))?
            /xi;
       my $to = $2;
       $to = undef if $to eq '-';
       my $vecid = $sql->CreateObs( $1, $to, $3, [$4,$5,$6],undef,$obs_csdef,$9,$7,$ObsClass,$8);
       # Vector data id is saved until the covariance is read.
       push(@vector,$vecid );
       next;
       }

    if( /^\s*covariance\s/ ) {
       my @cvr = split(' ');
       shift(@cvr);       # Remove the "covariance" line
       my $nvec = @vector;
       die "covariance must follow mvectors\n" if ! $nvec;
       die "Covariance matrix is the wrong size\n" if 
          @cvr != (($nvec*3)*($nvec*3+1))/2;
       foreach (@cvr) { die "Invalid covariance data\n" if ! /^[+-]?\d+\.?\d*$/; }
       my( $i, $j, $i1, $i2, $i3);

       for $j (0..$#vector) { for $i (0..$j ) {
          $i1 = ($j*3*($j*3+1))/2 + $i*3;
          $i2 = $i1 + $j*3 + 1;
          $i3 = $i2 + $j*3 + 2;
          if( $i == $j ) {
            $sql->CreateCovariance( $vector[$i], $vector[$j], 
                       [@cvr[$i1,$i2,$i3,$i2,$i2+1,$i3+1,$i3,$i3+1,$i3+2]] );
            }
          else {
            $sql->CreateCovariance( $vector[$i], $vector[$j], 
                       [@cvr[$i1,$i1+1,$i1+2,$i2,$i2+1,$i2+2,$i3,$i3+1,$i3+2]] );
            }
          }};
       @vector = (); 
       $sql->ClearObsSet();
       $inset = 0;
       next;
       }



    die "Invalid data\n$_\n";
  };
  if( $@ ) {
    print "Error at line $. of $deffile\n$@\n";
    last;
  }

}
$sql->close;




#===============================================================================
#
#   SUBROUTINE:   CreateCrdsys
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

sub CreateCrdsys {
    my ($id, $code ) = @_;
    my ($csprj, $csdef);
    if( $code eq "NZGD49" ) {
            $csprj = undef;
            $csdef = { id=>$id, ort1=>'LATD', ort2=>'LONG', ort3=>'HGHS' };

    }
    elsif( $code =~ /^\w{4}HT\d{4}$/i ) {
            $csprj = undef;
            $csdef = { id=>$id, ort1=>'HGHT', ort2=>'', ort3=>'' };
    }
    else {
            $csprj = Geodetic::NZCircuit::Projection( $2 );
            die "Invalid projection $2" if ! $csprj;
            $csdef = { id=>$id, ort1=>'NRTH', ort2=>'EAST', ort3=>'' };
    }
    return ($csdef, $csprj);
}
