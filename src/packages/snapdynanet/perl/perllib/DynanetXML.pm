use strict;
use XML::Parser;

# Note: Currently not handled..
#   VScale, Pscale, LScale
#
#

# DynanetXML:
#   
#  $dxml = new DynanetXML;
#  @obs = $dxml->ParseObs($obsfile);
#  $dxml->ParseObs($obsfile, \&use_obs_sub);
#  $dxml->ParseStation($stationfile,$geoidfile)

package DynanetXML;

sub new 
{
	my ($class) = @_;

	my $self = { errors=>[] };
	
	return bless $self, $class;
}


sub ParseObs 
{
	my ($self,$obsfile,$useobs) = @_;
	my $obs;
	if( $useobs )
	{
		die "DynanetXML::ParseObs useobs parameter must be function ref\n" 
			if ref($useobs) ne 'CODE';
		$self->{obssub} = $useobs;
	}
	else
	{
		$obs = [];
		$self->{obs} = $obs;
	}
	die "Dynanet obs file $obsfile not found\n" if ! -r $obsfile;
	my $parser = XML::Parser->new( Style=>'Stream', Pkg=>'DynanetXML::ObsParser' );
	$DynanetXML::ObsParser::user = $self;
	eval 
	{
		$parser->parsefile($obsfile);
	};
	$self->HandleError($@) if $@;

	
	delete $self->{obssub};
	delete $self->{obs};
	undef $DynanetXML::ObsParser::user;
	
	return $obs;
}


sub ParseStation
{
	my ($self,$stnfile,$geoidfile) = @_;
	my $stns = [];
	$self->{stations} = $stns;
	delete $self->{geoid};

	die "Dynanet station file $stnfile not found\n" if ! -r $stnfile;
	if( $geoidfile )
	{
		$self->ParseGeoidFile($geoidfile);
	}

	my $parser = XML::Parser->new( Style=>'Stream', Pkg=>'DynanetXML::CrdParser' );
	$DynanetXML::CrdParser::user = $self;
	eval 
	{
		$parser->parsefile($stnfile);
	};
	$self->HandleError($@) if $@;
	
	delete $self->{geoid};
	delete $self->{stations};
	undef $DynanetXML::CrdParser::user;
	
	return $stns;
}
	
	
sub Errors
{
	my ($self,$clear) = @_;
	my $e = $self->{errors};
	$self->{errors} = [] if $clear;
	return @$e;
}

sub ConvertHPAngle
{
   my ($angle) = @_;
   my $sign = $angle < 0 ? -1 : 1;
   $angle = abs($angle);
   my $d = int($angle);
   $angle = ($angle - $d) * 100;
   my $m = int($angle);
   $angle = ($angle - $m) * 100;
   return $sign * ($d + $m/60.0 + $angle/3600.0);
}

sub ProcessObs
{
	my( $self, $obs ) = @_;
	if( $self->{obssub} ) { $self->{obssub}->($obs); }
	else { push(@{$self->{obs}}, $obs ) }
}

sub ProcessStation
{
	my ($self,$stn) = @_;
	if( exists $self->{geoid} )
	{
		my $code = $stn->{code};
		$self->HandleError("Geoid information missing for $code")
			if ! exists $self->{geoid}->{$code};
		$stn->{geoid} = $self->{geoid}->{$code} || [0,0,0];
	}
	push(@{$self->{stations}},$stn);
}

sub HandleError
{
	my( $self, $error ) = @_;
	push(@{$self->{errors}}, $error );
}

sub ParseGeoidFile
{
	my($self,$geoidfile) = @_;
	open(my $g,"<", $geoidfile) || die "Cannot open Dynanet geoid file $geoidfile\n";
	my $gdata = {};
	while(<$g>)
	{
		my @f = split;
		next if @f != 4;
		$gdata->{$f[0]} = [$f[2],$f[3],$f[1]];
	}
	close($g);
	$self->{geoid} = $gdata;
}

package DynanetXML::ObsParser;

use Data::Dumper;

    use vars qw/
      $user
      $roottag
      $roottype
      $isvalid
      $obstag 
      $curobs 
      $curitem 
      @stack 
      $curtag 
      $lasttext 
      %isarray 
      %isrepeat 
      %ignore 
      $obsid
      /;

    BEGIN
    {
    %isarray = (
       Directions => 1,
       GPSCovariance => 2,
       PointCovariance => 2,
    );

    %ignore = (
       ClusterPoint => 1,
       GPSBaseline => 1,
    );
   
    %isrepeat = (
       First=>1,
       Ignore=>1,
    );

    $obsid=0;
    $isvalid = 0;

    $roottag = 'DnaXmlFormat';
    $roottype = {'Measurement File'=>1, 'Combined File'=>1};
    $obstag = 'DnaMeasurement';
    }

    
    sub StartTag {
      my ($e, $name) = @_;
      $curtag = $name;

      if($name eq $roottag )
      {
          my $type = $_{type};
          die "Invalid $roottag type \"$type\" for a DynanetXML file\n"
                if ! $roottype->{$type};
          $isvalid = 1;
      }
      if($isvalid && $name eq $obstag) {
         $obsid++;
         $curobs = [{}];
         $curitem = $curobs->[0];
         @stack = ();
         }
     
      return if ! $curobs;
      StartNewObs($name);
      StartArray($name);
      return if &Process("Start$name");

    }

    sub EndDocument 
    {
	$user->HandleError("$roottag missing") if ! $isvalid;
    }
    
    sub EndTag {
      my ($e, $name) = @_;
      return if ! $curobs;
      return if $ignore{$name};
      EndArray($name);
      if ( $lasttext =~ /\S/ )
      {
         if(exists $curitem->{$name})
         {
             $user->HandleError("Obs $obsid: Dropping value of repeated $curtag");
         }
         else
         { 
             $curitem->{$name} = $lasttext;
         }
      }
      &Process("End$name");
      undef($curobs) if $name eq $obstag;
    }
    
    
    sub Text {
      my ($e) = @_;
      $lasttext = $_;
    }

    sub StartNewObs
    {
       my($name) = @_;
       return if ! $isrepeat{$name};
       return if @stack;
       return if ! exists $curitem->{$name};

       $curitem = {};
       push(@$curobs,$curitem);
       
    }

    sub StartArray
    {
       my ($tag) = @_;
       return if ! $isarray{$tag};
       push(@stack,$curitem);
       $curitem = {};
       push(@{$curobs->[0]->{$tag}},$curitem);
    }

    sub EndArray
    {
       my ($tag) = @_;
       return if ! $isarray{$tag};
       $curitem = pop(@stack);
    }

    sub Process
    {
       no strict 'refs';
       my ($proc) = @_;
       my $package = (caller())[0];
       my $sub = $package.'::'.$proc;
       return 0 if ! defined(&$sub);
       &$sub();
       return 1;
    }


    sub EndDnaMeasurement
    {
       no strict 'refs';
       # print Dumper($curobs);
       my $type = $curobs->[0]->{Type};
       my $package = (caller())[0];
       my $sub = $package.'::'.'ProcObs'.$type;
       if( defined($sub))
       {
          my @obs = &$sub($curobs);
	  foreach my $o (@obs) { $user->ProcessObs($o); }
       }
       else
       {
          $user->HandleError("Obs $obsid: No handler defined for '$type' observations");
       }
    }


    sub BaseObs
    {
        my($curobs,$type) = @_;
        my $obsdata = $curobs->[0];
        return { 
	  _id => $obsid,
          type=>$type,
          from => $obsdata->{First},
          fromhgt => $obsdata->{InstHeight} || 0.0,
          to => [],
          date => 'unknown',
	  ignore => $obsdata->{Ignore},
          };
    }

    sub SimpleVectorObs 
    {
        my($curobs,$type) = @_;
        my $obs = BaseObs(@_);
        my $obsdata = $curobs->[0];
	my $to = {};
        $obs->{to}->[0] = $to;
        $to->{to} = $obsdata->{Second}; 
        $to->{tohgt} = $obsdata->{TargHeight} || 0.0; 
	$to->{type} = $type;
	$to->{value} = $obsdata->{Value};
	$to->{sd} = $obsdata->{StdDev};
	return $obs;
    }

    sub SimplePointObs
    {
        my $obs = SimpleVectorObs(@_);
        delete $obs->{to}->[0]->{to};
        delete $obs->{to}->[0]->{tohgt};
	return $obs;
    }

    sub FixAngle
    {
       my($obs) = @_;
       foreach my $t (@{$obs->{to}} )
       {
          $t->{value} = &DynanetXML::ConvertHPAngle($t->{value});
          $t->{sd} /= 3600.0;
       }
       return $obs;
    }

    # Ellipsoidal longitude,latitude,height - why has this a <Second>

    sub ProcObsQ { return FixAngle(SimplePointObs( $_[0], 'LN' )); }
    sub ProcObsP { return FixAngle(SimplePointObs( $_[0], 'LT' )); }
    sub ProcObsR { return SimplePointObs( $_[0], 'EH' ); }
    sub ProcObsH { return SimplePointObs( $_[0], 'OH' ); }

    # Check mapping of distances - there are some issues with these I think..

    # Ellipsoidal distance
    sub ProcObsE { return SimpleVectorObs( $_[0], 'ED' ); }
    # Mean sea level arc
    sub ProcObsM { return SimpleVectorObs( $_[0], 'MD' ); }
    # Chord distance
    sub ProcObsC { return SimpleVectorObs( $_[0], 'HD' ); }
    # Slope distance
    sub ProcObsS { return SimpleVectorObs( $_[0], 'SD' ); }
  
    sub ProcObsL { return SimpleVectorObs( $_[0], 'LV' ); }

    # Astronomic azimuth
    sub ProcObsK { return FixAngle(SimpleVectorObs( $_[0], 'AZ' )); }
    # Geodetic azimuth
    sub ProcObsB { return FixAngle(SimpleVectorObs( $_[0], 'AZ' )); }


    sub ProcObsV { return FixAngle(SimpleVectorObs( $_[0], 'ZD' )); }
    # Convert Vertical angle to Zenith Distance 
    sub ProcObsZ { 
	my $obs = FixAngle(SimpleVectorObs( $_[0], 'ZD' )); 
	$obs->{to}->[0]->{value} = 90 - $obs->{to}->[0]->{value};
        $obs->{comment} = "Converted from vertical angle";
        return $obs;
	}
       
    # Horizontal angle
    # Distribute error between RA and target..
    sub ProcObsA {
        my ($curobs) = @_;
        my $obs = BaseObs($curobs,'HA');
	my $obsdata = $curobs->[0];
        my $sd = $obsdata->{StdDev} * sqrt(2);
	my $ra = { 
            to=>$obsdata->{Second},
            tohgt=>$obsdata->{TargHeight} || 0.0,
            value=>0.0,
            sd=>$sd };
        my $to = {
	    to=>$obsdata->{Third},
            tohgt=>$obsdata->{TargHeight} || 0.0,
            value=>$obsdata->{Value},
            sd=>$sd };
	push(@{$obs->{to}},$ra);
	push(@{$obs->{to}},$to);
	return FixAngle($obs);
	}

    # Set of directions
    # Distribute error between RA and each target
    sub ProcObsD {
        my($curobs) = @_;
        my $obs = SimpleVectorObs($curobs,'HA');
	my $obsdata = $curobs->[0];
        my $directions = $obsdata->{Directions};
	foreach my $d (@$directions) {
            push(@{$obs->{to}}, {
               to=>$d->{Target},
               tohgt => $d->{TargHeight} || $obsdata->{TargHeight},
               value => $d->{Value},
	       sd=>$d->{StdDev},
               });
           }
        return FixAngle($obs);
        }

   sub SetCvrIJ
   {
      my($cvr,$i,$j,$value) = @_;
      my $ij = $j < $i ? ($i*($i+1))/2+$j : ($j*($j+1))/2+$i;
      $cvr->[$ij] = $value;
   }

   sub BuildCovariance 
   {
       my($s,$m) = @_;
       my $cvr = [];
       my ($i,$j,$ij);
       my $npt = @$s;
       foreach $i (0..$npt-1)
       {
           my $si = $s->[$i];
	   SetCvrIJ($cvr,$i*3,$i*3, $si->{SigmaXX});
	   SetCvrIJ($cvr,$i*3,$i*3+1, $si->{SigmaXY});
	   SetCvrIJ($cvr,$i*3+1,$i*3+1, $si->{SigmaYY});
	   SetCvrIJ($cvr,$i*3,$i*3+2, $si->{SigmaXZ});
	   SetCvrIJ($cvr,$i*3+1,$i*3+2, $si->{SigmaYZ});
	   SetCvrIJ($cvr,$i*3+2,$i*3+2, $si->{SigmaZZ});
       }
       $ij = 0;
       foreach $i (0..$npt-2) {
       foreach $j ($i+1..$npt-1)
       {
           my $mi = $m->[$ij]; $ij++;
	   SetCvrIJ($cvr,$i*3,$j*3, $mi->{m11});
	   SetCvrIJ($cvr,$i*3,$j*3+1, $mi->{m12});
	   SetCvrIJ($cvr,$i*3,$j*3+2, $mi->{m13});
	   SetCvrIJ($cvr,$i*3+1,$j*3, $mi->{m21});
	   SetCvrIJ($cvr,$i*3+1,$j*3+1, $mi->{m22});
	   SetCvrIJ($cvr,$i*3+1,$j*3+2, $mi->{m23});
	   SetCvrIJ($cvr,$i*3+2,$j*3, $mi->{m31});
	   SetCvrIJ($cvr,$i*3+2,$j*3+1, $mi->{m32});
	   SetCvrIJ($cvr,$i*3+2,$j*3+2, $mi->{m33});
       }
       }
       return $cvr;
   }

   sub ProcGPSBaselines
   {
      my ($curobs) = @_;
      my $obs = BaseObs($curobs,'GB');
      my $invalid = 0;

      my $to = $obs->{to};
      my $obsdata;
      foreach $obsdata ( @$curobs )
      {
	 if( $obsdata->{First} ne $obs->{from} ) {
             $invalid = 1;
	     last;
             }
         push(@$to, {
             to => $obsdata->{Second},
             tohgt => $obsdata->{TargHeight} || 0.0,
             value => [$obsdata->{X}, $obsdata->{Y}, $obsdata->{Z}],
             });
      }
      if( $invalid ) 
      {
         $user->HandleError("Obs $obsid: Ignoring covariances for GPS baseline set with inconsistent instrument");
          my @set;
          foreach my $o (@$curobs)
          {
              my $co = [$o];
              my $obs = ProcGPSBaselines($co);
	      $obs->{comment} = "Baseline covariances discarded";
              push(@set,$obs);
          }
          return @set;
      }
      $obsdata = $curobs->[0];
      $obs->{cvr} = BuildCovariance($curobs,$obsdata->{GPSCovariance});
      return $obs;
   }

   sub ProcObsG { return ProcGPSBaselines(@_); }
   sub ProcObsX { return ProcGPSBaselines(@_); }

   sub ProcObsY { 
      my($curobs) = @_;
      my $obsdata = $curobs->[0];
      my $crdtype = $obsdata->{Coords};
      if( $crdtype eq 'XYZ' )
      {
          my $obs = BaseObs($curobs,'GX');
          delete $obs->{from};
          delete $obs->{fromhgt};
          my $to = $obs->{to};
          foreach my $o (@$curobs)
          {
		push(@$to,{
	             to => $o->{First},
	             tohgt => $o->{InstHeight} || 0.0,
	             value => [$o->{X}, $o->{Y}, $o->{Z}],
		    });
          }
          $obs->{cvr} = BuildCovariance($curobs,$obsdata->{PointCovariance});
          return $obs;
      }

      if( $crdtype eq 'LLH' )
      {
          my $obs = BaseObs($curobs,'EP');
          delete $obs->{from};
          delete $obs->{fromhgt};
          my $to = $obs->{to};
          foreach my $o (@$curobs)
          {
		push(@$to,{
	             to => $obsdata->{First},
	             tohgt => $obsdata->{InstHeight} || 0.0,
	             value => [$obsdata->{X}, $obsdata->{Y}, $obsdata->{Z}],
		    });
          }
          $obs->{cvr} = BuildCovariance($curobs,$obsdata->{PointCovariance});
          return $obs;
      }
   
  
      $user->HandleError("Obs $obsid: Ignoring Point cluster observations with $crdtype Coordinates");
                  
      return undef; 
      }


package DynanetXML::CrdParser;

    use vars qw/
      $user
      $roottag
      $roottype
      $isvalid
      $obstag 
      $curobs 
      $curitem 
      @stack 
      $curtag 
      $lasttext 
      %isarray 
      %isrepeat 
      %ignore 
      $obsid
      /;

    BEGIN
    {
    %isarray = (
    );

    %ignore = (
       StationCoord => 1,
    );
   
    %isrepeat = (
    );

    $obsid=0;
    $isvalid = 0;

    $roottag = 'DnaXmlFormat';
    $roottype = {'Station File'=>1, 'Combined File'=>1};
    $obstag = 'DnaStation';
    }

    
    sub StartTag {
      my ($e, $name) = @_;
      $curtag = $name;

      if($name eq $roottag )
      {
          my $type = $_{type};
          die "Invalid $roottag type \"$type\" for a DynanetXML coordinate\n"
                if ! $roottype->{$type};
          $isvalid = 1;
      }
      if($isvalid && $name eq $obstag) {
         $obsid++;
         $curobs = [{}];
         $curitem = $curobs->[0];
         @stack = ();
         }
     
      return if ! $curobs;
      StartNewObs($name);
      StartArray($name);
      return if &Process("Start$name");

    }

    sub EndDocument 
    {
	$user->HandleError("$roottag missing") if ! $isvalid;
    }

    sub EndTag {
      my ($e, $name) = @_;
      return if ! $curobs;
      return if $ignore{$name};
      EndArray($name);
      if ( $lasttext =~ /\S/ )
      {
         if(exists $curitem->{$name} && $lasttext ne $curitem->{$name})
         {
             $user->HandleError("Stn $obsid: Dropping value of repeated $curtag");
         }
         else
         { 
             $curitem->{$name} = $lasttext;
         }
      }
      &Process("End$name");
      undef($curobs) if $name eq $obstag;
    }
    
    
    sub Text {
      my ($e) = @_;
      $lasttext = $_;
    }

    sub StartNewObs
    {
       my($name) = @_;
       return if ! $isrepeat{$name};
       return if @stack;
       return if ! exists $curitem->{$name};

       $curitem = {};
       push(@$curobs,$curitem);
       
    }

    sub StartArray
    {
       my ($tag) = @_;
       return if ! $isarray{$tag};
       push(@stack,$curitem);
       $curitem = {};
       push(@{$curobs->[0]->{$tag}},$curitem);
    }

    sub EndArray
    {
       my ($tag) = @_;
       return if ! $isarray{$tag};
       $curitem = pop(@stack);
    }

    sub Process
    {
       no strict 'refs';
       my ($proc) = @_;
       my $package = (caller())[0];
       my $sub = $package.'::'.$proc;
       return 0 if ! defined(&$sub);
       &$sub();
       return 1;
    }

    sub EndDnaStation
    {
        my $dstn = $curobs->[0];
	my $crdsys = $dstn->{Type};
        my $crd = [ $dstn->{XAxis},
                     $dstn->{YAxis},
	             $dstn->{Height} ];
	if( $crdsys eq 'LLH' ) { 
             $crd->[0] = &DynanetXML::ConvertHPAngle($crd->[0]);
             $crd->[1] = &DynanetXML::ConvertHPAngle($crd->[1]);
             }
	elsif( $crdsys eq 'UTM' ) { $crdsys = 'UTM_'.$dstn->{HemisphereZone};}
	else { die "Unusable coordinate system type $crdsys\n"; }

        my $stn = {
		code => $dstn->{Name},
		name => $dstn->{Description},
		crd => $crd,
		crdsys => $crdsys,
		constraint => $dstn->{Constraints}
		};
	$user->ProcessStation($stn);
    }


1;

