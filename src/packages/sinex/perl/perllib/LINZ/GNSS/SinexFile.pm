=head1 LINZ::GNSS::SinexFile

  Package to extract basic information from a SINEX file.

  This is currently not written as a general purpose SINEX file reader/writer.

  The current purpose is simply to read station coordinate/covariance information.
  All sorts of assumptions are made - essentially hope this works with Bernese generated
  SINEX files from ADDNEQ...

=cut

use strict;


package LINZ::GNSS::SinexFile;

use LINZ::GNSS::Time qw/yearday_seconds/;
use LINZ::Geodetic::Ellipsoid;
use Carp;


=head2 $sf=LINZ::GNSS::SinexFile->new($filename, %options)

Open and scan a SINEX file

Options can include

=over

=item full_covariance

Obtain the full coordinate covariance matrix.

=back

=cut 

sub new
{
    my( $class, $filename, %options ) = @_;
    my $self=bless { filename=>$filename }, $class;
    $self->_scan(%options) if $filename ne '';
    return $self;
}

=head2 $stns=$sf->stations()

Returns a list of stations in the SINEX file.  May be called in a scalar or array context.
Note this only returns stations for which coordinates have been calculated in the solution.
It does not return the full coordinate covariance matrix - just the covariance for the 
each station X,Y,Z coordinates.

Each station is returned as a hash with keys

=over

=item code

The four character code of the station (the SINEX site code)

=item solnid

The solution id for the site - each site can have multiple soutions

=item epoch 

The mean epoch of the solution

=item prmoffset

The offset of the X parameter in the full coordinate covariance matrix

=item xyz

An array hash with the xyz coordinate

=item covar

An array hash of array hashes defining the 3x3 covariance matrix

=back

=cut 

sub stations
{
    my($self)=@_;
    my @codes=sort {$a->{code} cmp $b->{code}} values %{$self->{stations}};
    my @stations  = map {sort {$a->{solnid} cmp $b->{solnid}} values %$_} @codes;
    @stations = sort {_cmpstn($a) cmp _cmpstn($b)} @stations;
    return wantarray ? @stations : \@stations;
}

=head2 $stn=$sf->station($code,$solnid)

Returns an individual station from the file by its code.  The station is returned in the same
way as for the stations() function.  If the solnid is omitted then fails if there is more than
one.

=cut 

sub station
{
    my($self,$code,$solnid)=@_;
    croak("Invalid station $code requested from SINEX file\n") 
       if ! exists($self->{stations}->{$code});
    if( $solnid eq '' )
    {
        my @solns=keys(%{$self->{station}->{solnid}});
        croak("Requested station $code doesn't have a unique solution\n");
    }
    else
    {
        croak("Invalid solution id $solnid for station $code requested from SINEX file\n") 
       if ! exists($self->{stations}->{$code}->{$solnid});
    }
    return $self->{stations}->{$code}->{$solnid};
}

=head2 $covar=$sf->covar()

Returns the lower triangle of the coordinate covariance matrix for the stations returned by $sf->stations().
The station order is defined by $stn->{prmoffset}.
Only valid if the sinex file was loaded with the full_covariance option.

=cut

sub covar
{
    my($self)=@_;
    return $self->{xyzcovar};
}

=head2 $sf->stats()

Returns a hash containing the solution statistics.  The values contained are

=over

=item nobs

The number of observations

=item nprm

The number of parameters

=item ndof

The degrees of freedom

=item seu

The standard error of unit weight

=back

=cut

sub stats
{
    my($self)=@_;
    return $self->{stats};
}


sub _scan
{
    my($self,%options) = @_;
    my $filename=$self->{filename};
    return if ! $filename;

    my $stats={};
    my $station={};

    open( my $sf, "<$filename" ) || croak("Cannot open SINEX file $filename\n");
    binmode($sf);
    my $header=<$sf>;
    croak("$filename is not a valid SINEX file\n") if $header !~ /^\%\=SNX\s(\d\.\d\d)\s+/;
    my $version=$1;
    $self->_scanStats($sf);
    $self->_scanStationXYZ($sf,$options{full_covariance});
    close($sf);
}

sub _findBlock
{
    my($self,$sf,$block,$die) = @_;
    seek($sf,0,0);
    while( my $line=<$sf> )
    {
        next if $line !~ /^\+/;
        my $fblock=substr($line,1);
        $fblock=~ s/\s+$//;
        return 1 if $fblock eq $block;
    }
    croak("Cannot find block $block in SINEX file ".$self->{filename}."\n") if $die;
    return 0;
}

sub _trim
{
    my($value)=@_;
    return '' if ! defined $value;
    $value=~s/\s+$//;
    $value=~s/^\s+//;
    return $value;
}

sub _scanStats
{
    my( $self, $sf ) = @_;
    my $stats={ nobs=>0, nprm=>0, dof=>0, seu=>1.0 };
    $self->{stats} = $stats;

    $self->_findBlock($sf,'SOLUTION/STATISTICS',1);
    while( my $line=<$sf> )
    {
        my $ctl=substr($line,0,1);
        last if $ctl eq '-';
        if( $ctl eq '+' )
        {
            carp("Invalid SINEX file - SOLUTION/STATISTICS not terminated\n");
            last;
        }
        next if $ctl ne ' ';
        my $item=_trim(substr($line,1,30));
        my $value=_trim(substr($line,32,22));
        $stats->{nobs}=$value+0 if $item eq 'NUMBER OF OBSERVATIONS';
        $stats->{dof}=$value+0 if $item eq 'NUMBER OF DEGREES OF FREEDOM';
        $stats->{nprm}=$value+0 if $item eq 'NUMBER OF UNKNOWNS';
        $stats->{seu}=sqrt($value+0) if $item eq 'VARIANCE FACTOR';
    }
    return 1;
}

# Key for station sorting
sub _cmpstn 
{ 
    return $_[0]->{code}.' '.$_[0]->{solnid} 
};

sub _scanStationXYZ
{
    my( $self, $sf, $fullcovar ) = @_;
    my $stations={};
    $self->{stations}=$stations;

    $self->_findBlock($sf,'SOLUTION/ESTIMATE',1);

    my $prms={};
    my @solnstns=();
    while( my $line=<$sf> )
    {
        my $ctl=substr($line,0,1);
        last if $ctl eq '-';
        next if $ctl eq '*';
        if( $ctl eq '+' )
        {
            carp("Invalid SINEX file - SOLUTION/ESTIMATE not terminated\n");
            last;
        }
        croak("Invalid SOLUTION/ESTIMATE line $line in ".$self->{filename}."\n")
        if $line !~ /^
             \s([\s\d]{5})  # param id
             \s([\s\w]{6})  # param type
             \s([\s\w]{4})  # point id
             \s([\s\w]{2})  # point code
             \s([\s\w]{4})  # solution id
             \s(\d\d\:\d\d\d\:\d\d\d\d\d) #parameter epoch
             \s([\s\w]{4})  # param units
             \s([\s\w]{1})  # param cnstraints
             \s([\s\dE\+\-\.]{21})  # param value
             \s([\s\dE\+\-\.]{11})  # param stddev
             \s*$/x;

        my ($id,$ptype,$mcode,$solnid,$epoch,$value)=
           (_trim($1)+0,_trim($2),_trim($3),_trim($4).':'._trim($5),_trim($6),_trim($9)+0);

        next if $ptype !~ /^STA([XYZ])$/;
        if( ! exists($stations->{$mcode}) || ! exists($stations->{$mcode}->{$solnid}) )
        {
            my $newstn= {
                code=>$mcode,
                solnid=>$solnid,
                epoch=>0,
                prmoffset=>0,
                xyz=>[0.0,0.0,0.0],
                covar=>[[0.0,0.0,0.0],[0.0,0.0,0.0],[0.0,0.0,0.0]]
                };
            $stations->{$mcode}->{$solnid} = $newstn;
            push(@solnstns,$newstn);
        }

        my $stn=$stations->{$mcode}->{$solnid};
        my $pno=index('XYZ',$1);

        $stn->{xyz}->[$pno]=$value;
        $prms->{$id}={stn=>$stn,pno=>$pno};
    }

    my $fcvr=[[]];
    $self->{xyzcovar}=$fcvr;
    if( $fullcovar )
    {
        my $prmoffset=0;
        foreach my $sstn (sort {_cmpstn($a) cmp _cmpstn($b)} @solnstns)
        {
            $sstn->{prmoffset}=$prmoffset;
            $prmoffset += 3;
        }
        foreach my $iprm (0..$prmoffset-1)
        {
            $fcvr->[$iprm]=[(0)x($iprm+1)]
        }
    }

    $self->_findBlock($sf,'SOLUTION/MATRIX_ESTIMATE L COVA',1);
    while( my $line=<$sf> )
    {
        my $ctl=substr($line,0,1);
        last if $ctl eq '-';
        next if $ctl eq '*';
        if( $ctl eq '+' )
        {
            carp("Invalid SINEX file - SOLUTION/MATRIX_ESTIMATE not terminated\n");
            last;
        }
        croak("Invalid SOLUTION/MATRIX_ESTIMATE line $line in ".$self->{filename}."\n")
        if $line !~ /^
            \s([\s\d]{5})
            \s([\s\d]{5})
            \s([\s\dE\+\-\.]{21})
            (?:\s([\s\dE\+\-\.]{21}))?
            (?:\s([\s\dE\+\-\.]{21}))?
            /x;
        my ($p0,$p1,$cvc)=($1+0,$2+0,[_trim($3),_trim($4),_trim($5)]);
        next if ! exists $prms->{$p0};
        my $prm=$prms->{$p0};
        my $stn=$prm->{stn};
        my $pno=$prm->{pno};
        my $rc0=$stn->{prmoffset}+$pno;
        my $covar=$stn->{covar};

        foreach my $ip (0,1,2)
        {
            my $p1i=$p1+$ip;
            next if $cvc->[$ip] eq '';
            next if ! exists $prms->{$p1i};
            $prm=$prms->{$p1i};
            my $stnt=$prm->{stn};
            if( $fullcovar )
            {
                my $rc1=$stnt->{prmoffset}+$prm->{pno};
                if( $rc1 < $rc0 ){ $fcvr->[$rc0]->[$rc1]=$cvc->[$ip]+0; }
                else { $fcvr->[$rc1]->[$rc0]=$cvc->[$ip]+0; }
            }
            next if $stnt ne $stn;
            my $pno1=$prm->{pno};
            $covar->[$pno]->[$pno1]=$cvc->[$ip]+0;
            $covar->[$pno1]->[$pno]=$cvc->[$ip]+0;
        }
    }

    $self->_findBlock($sf,'SOLUTION/EPOCHS',1);
    while( my $line=<$sf> )
    {
        my $ctl=substr($line,0,1);
        last if $ctl eq '-';
        next if $ctl eq '*';
        if( $ctl eq '+' )
        {
            carp("Invalid SINEX file - SOLUTION/EPOCHS not terminated\n");
            last;
        }
        croak("Invalid SOLUTION/EPOCH line $line in ".$self->{filename}."\n")
        if $line !~ /^
            \s([\s\w]{4})  # point id
            \s([\s\w]{2})  # point code
            \s([\s\w]{4})  # solution id
            \s(\w)           # to be determined!
            \s(\d\d\:\d\d\d\:\d\d\d\d\d) # start epoch
            \s(\d\d\:\d\d\d\:\d\d\d\d\d) # end epoch
            \s(\d\d\:\d\d\d\:\d\d\d\d\d) # mean epoch
            /x;
        my ($code,$solnid,$meanepoch)=(_trim($1),_trim($2).':'._trim($3),$7);
        my ($y,$doy,$sec)=split(/\:/,$meanepoch);
        $y += 1900;
        $y += 100 if $y < 1980;
        my $epoch=yearday_seconds($y,$doy)+$sec;
        $stations->{$code}->{$solnid}->{epoch}=$epoch
           if exists $stations->{$code} && exists $stations->{$code}->{$solnid};
    }


    return 1;
 }

1;
