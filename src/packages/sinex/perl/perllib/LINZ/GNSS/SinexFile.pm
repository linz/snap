=head1 LINZ::GNSS::SinexFile

  Package to extract basic information from a SINEX file.

  This is currently not written as a general purpose SINEX file reader/writer.

  The initial current purpose is simply to read station coordinate/covariance information.
  Be aware of simplifications made.

  Initial implementation was for reading only returned stations using the stations() function.
  This has been enhanced to handle sites with multiple marks and solutions, and to calculate
  coordinates at a specific epoch, as well as limited output of SINEX file.  The initial stations()
  function is deprecated in favour of sitecodes() and site().

=cut

# Although the stations() function is deprecated this module has not yet been updated to reflect
# the more correctly structured use of site and mark.

use strict;

package LINZ::GNSS::SinexFile;

use LINZ::GNSS::Time qw/yearday_seconds/;
use LINZ::Geodetic::Ellipsoid;
use File::Which;
use PerlIO::gzip;
use Carp;


=head2 $sf=LINZ::GNSS::SinexFile->new($filename, %options)

Open and scan a SINEX file

Options can include

=over

=item full_covariance

Obtain the full coordinate covariance matrix.

=item need_covariance

Set to 0 if covariance information is not required, but will
be loaded if it is present.

=item skip_covariance

Set to 1 to ignore convariance information.

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

Note: this is deprecated in favour of sitecodes() and site().

Returns a list of station solutions in the SINEX file.  May be called in a scalar or array context.
Note this only returns stations for which coordinates have been calculated in the solution.
It does not return the full coordinate covariance matrix - just the covariance for the 
each station X,Y,Z coordinates.

Each station is returned as a hash with keys

=over

=item code

The four character code of the station (the SINEX site code)

=item solnid

The solution id for the site - each site can have multiple soutions

=item name

The monument id from the site/id section

=item description

The description of the station from the site/id section

=item epoch 

The mean epoch of the solution

=item start_epoch 

The start epoch of the solution

=item end_epoch 

The end epoch of the solution

=item ref_epoch 

The reference epoch of the solution coordinate

=item prmoffset

The offset of the X parameter in the full coordinate covariance matrix

=item xyz

An array hash with the xyz coordinate

=item vxyz

An array hash with the xyz coordinate

=item covar

An array hash of array hashes defining the 3x3 covariance matrix

=back

Note: currently the covariance information does not include velocity covariances.

=cut 

sub stations
{
    my($self)=@_;
    my $stations=$self->{solnstns};
    return wantarray ? @$stations : $stations;
}


=head2 @codes=$sf->sitecodes()

Return a list of sitecode of sites in the file.  Each site can be obtained with the 
site() function, which provides a marks() function, which provides a solutions() 
function.

=cut


sub sitecodes()
{
    my($self)=@_;
    my @codes = sort(keys(%{$self->{stations}}));
    return wantarray ? @codes : \@codes;
}

=head2 $site=$sf->site($code,$mark)

Return data for a site.  The mark (point) at the site
may also be specified, otherwise all marks are return.  
The object returned is a LINZ::GNSS::SinexFile::Site object.

=cut

sub site
{
    my($self,$code,$mark)=@_;
    $mark .= ':' if $mark ne '';
    my $stndata=$self->{stations}->{$code};
    croak("Code $code not in SINEX file\n") if ! $stndata;
    my @solutions=();
    foreach my $solnid (keys %$stndata)
    {
        next if $mark ne '' && substr($solnid,0,length($mark)) ne $mark;
        push(@solutions,$stndata->{$solnid});
    }
    croak("No information found for $code $mark\n") if ! @solutions;
    return LINZ::GNSS::SinexFile::Site->new($code,\@solutions);
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

This will be empty if the SINEX file does not contain a SOLUTION/STATISTICS block.

=cut

sub stats
{
    my($self)=@_;
    return $self->{stats};
}

=head2 $startdate,$enddate=$sf->obsDates()

Returns the start and end date of observations in the SinexFile (as timestamps)

=cut

sub obsDates
{
    my($self)=@_;
    return $self->{obs_start_date},$self->{obs_end_date};
}

sub _open
{
    my ($self)=@_;
    my $filename=$self->{filename};
    return if ! $filename;

    my $stats={};
    my $station={};

    my $sf;
    if( $filename =~ /\.Z$/ )
    {
        my $compress=which('compress');
        croak("Cannot find compress program to open SINEX file\n") if ! $compress;
        my $cmd="$compress -c -d \"$filename\" |";
        open( $sf, $cmd ) || croak("Cannot open SINEX file $filename\n");
    }
    else
    {
        my $mode="<";
        if( $filename =~ /\.gz$/ )
        {
            $mode="<:gzip";
        }
        open( $sf, $mode, $filename ) || croak("Cannot open SINEX file $filename\n");
    }
    return $sf;
}

sub _scan
{
    my($self,%options) = @_;
    my $sf=$self->_open();
    return if ! $sf;
    
    $self->{stats}={};
    $self->{solnprms}={};
    $self->{stations}={};

    my $header=<$sf>;
    croak($self->{filename}." is not a valid SINEX file\n") if $header !~ /^\%\=SNX\s(\d\.\d\d)\s+/;
    my $version=$1;
    $self->{obs_start_date}=$self->_sinexDateToTimestamp(substr($header,32,12));
    $self->{obs_end_date}=$self->_sinexDateToTimestamp(substr($header,45,12));

    my $cvropt=exists $options{need_covariance} && ! $options{need_covariance};
    $cvropt=1 if $options{skip_covariance};

    my $blocks={
        'SITE/ID'=>0, 
        'SOLUTION/STATISTICS'=>1, # Statistics are treated as optional
        'SOLUTION/EPOCHS'=>0,
        'SOLUTION/ESTIMATE'=>0,
        'SOLUTION/MATRIX_ESTIMATE L COVA'=>$cvropt,
    };
    while( my $block=$self->_findNextBlock($sf) )
    {
        $blocks->{$block}=1 if
            ($block eq 'SITE/ID' && $self->_scanSiteId($sf)) ||
            ($block eq 'SOLUTION/STATISTICS' && $self->_scanStats($sf)) ||
            ($block eq 'SOLUTION/EPOCHS' && $self->_scanEpochs($sf)) ||
            ($block eq 'SOLUTION/ESTIMATE' && $self->_scanSolutionEstimate($sf)) ||
            ($block eq 'SOLUTION/MATRIX_ESTIMATE L COVA' &&
                ($options{skip_covariance} || $self->_scanCovar($sf,$options{full_covariance})));
    }

    foreach my $block (sort keys %$blocks)
    {
        if( ! $blocks->{$block} )
        {
            croak($self->{filename}." does not contain a $block block\n");
        }
    }

    close($sf);
}

sub _findNextBlock
{
    my($self,$sf)=@_;
    my $block;
    while( my $line=<$sf> )
    {
        next if $line !~ /^\+/;
        my $block=substr($line,1);
        $block=~ s/\s+$//;
        return $block;
    }
    return '';
}

sub _trim
{
    my($value)=@_;
    return '' if ! defined $value;
    $value=~s/\s+$//;
    $value=~s/^\s+//;
    return $value;
}

sub _scanSiteId
{
    my ($self,$sf)=@_;
    my $siteids={};
    $self->{siteids}=$siteids;
    my $prms=$self->{solnprms};
    while( my $line=<$sf> )
    {
        my $ctl=substr($line,0,1);
        last if $ctl eq '-';
        next if $ctl eq '*';
        if( $ctl eq '+' )
        {
            carp("Invalid SINEX file - SITE/ID not terminated\n");
            last;
        }
        croak("Invalid SITE/ID line $line in ".$self->{filename}."\n")
        if $line !~ /^
             \s([\s\w]{4})  # point id
             \s([\s\w]{2})  # point code
             \s([\s\w]{9})  # monument id
             \s\w           # Observation techniques
             \s(.{22})      # description
             \s([\s\-\d]{3}[\-\s\d]{3}[\-\s\d\.]{5}) # longitude DMS
             \s([\s\-\d]{3}[\-\s\d]{3}[\-\s\d\.]{5}) # latitude DMS
             \s([\s\d\.\-]{7})                       # height
             \s*$/ix;

        my ($mcode,$ptid,$name,$description)=
           (_trim($1),_trim($2),_trim($3),_trim($4));

        $siteids->{$mcode}->{$ptid}={name=>$name,description=>$description};
    }
    return 1;
}

sub _scanStats
{
    my( $self, $sf ) = @_;
    my $stats={ nobs=>0, nprm=>0, dof=>0, seu=>1.0 };
    $self->{stats} = $stats;

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

sub _getStation
{
    my($self,$mcode,$solnid) = @_;
    my $stations=$self->{stations};
    if( ! exists($stations->{$mcode}) || ! exists($stations->{$mcode}->{$solnid}) )
    {
        my $newstn= {
            code=>$mcode,
            solnid=>$solnid,
            epoch=>0,
            start_epoch=>0,
            end_epoch=>0,
            ref_epoch=>0,
            prmoffset=>0,
            estimated=>0,
            xyz=>[0.0,0.0,0.0],
            vxyz=>[0.0,0.0,0.0],
            covar=>[[0.0,0.0,0.0],[0.0,0.0,0.0],[0.0,0.0,0.0]]
            };
        $stations->{$mcode}->{$solnid} = $newstn;
    }
    return $stations->{$mcode}->{$solnid};
}

sub _compileStationList
{
    my($self)=@_;
    my $stations=$self->{stations};
    # Copy across information from site ids
    my $siteids=$self->{siteids};
    foreach my $stn (values %$stations)
    {
        foreach my $stn (values %$stn)
        {
            my $code=$stn->{code};
            my $solnid=$stn->{solnid};
            $solnid =~ s/\:.*//;
            my $siteid=$siteids->{$code}->{$solnid};
            if( $siteid )
            {
                while(my ($k,$v)=each %$siteid){ $stn->{$k}=$v; }
            }
        }
    }

    # Form a list of stations with estimated coordinates
    my @solnstns;
    foreach my $k (%$stations)
    {
        push(@solnstns,grep {$_->{estimated}} values %{$stations->{$k}});
    }

    # Form parameter numbers for solution coordinates in covariance matrix
    # and copy back to stations

    $stations={};
    my $prmoffset=0;
    @solnstns=sort {_cmpstn($a) cmp _cmpstn($b)} @solnstns;
    foreach my $sstn (@solnstns)
    {
        $sstn->{prmoffset}=$prmoffset;
        $prmoffset += 3;
        $stations->{$sstn->{code}}->{$sstn->{solnid}}=$sstn;
    }
    $self->{stations}=$stations;
    $self->{solnstns}=\@solnstns;
    $self->{nparam}=$prmoffset;
}


sub _scanSolutionEstimate
{
    my ($self,$sf)=@_;
    my $stations=$self->{stations};
    my $prms=$self->{solnprms};
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
             \s([\s\w]{4}|[\s\-]{4})  # point id
             \s([\s\w]{2}|[\s\-]{2})  # point code
             \s([\s\w]{4}|[\s\-]{4})  # solution id
             \s(\d\d\:\d\d\d\:\d\d\d\d\d) #parameter epoch
             \s([\s\w\/]{4})  # param units
             \s([\s\w]{1})  # param cnstraints
             \s([\s\dE\+\-\.]{21})  # param value
             \s([\s\dE\+\-\.]{11})  # param stddev
             \s*$/ix;

        my ($id,$ptype,$mcode,$solnid,$epoch,$value)=
           (_trim($1)+0,_trim($2),_trim($3),_trim($4).':'._trim($5),_trim($6),_trim($9)+0);

        next if $ptype !~ /^(STA|VEL)([XYZ])$/;
        my $isxyz = $1 eq 'STA';
        my $pno=index('XYZ',$2);

        my $stn=$self->_getStation($mcode,$solnid);
        if( $isxyz )
        {
            $stn->{xyz}->[$pno]=$value;
            $stn->{estimated}=1;
            $stn->{ref_epoch}=$self->_sinexDateToTimestamp($epoch);
            $prms->{$id}={stn=>$stn,pno=>$pno};
        }
        else
        {
            $stn->{vxyz}->[$pno]=$value;
        }
    }
    $self->_compileStationList();
    return 1;
}

sub _scanCovar
{
    my ($self,$sf,$fullcovar) = @_;
    my $fcvr=[[]];
    my $prms=$self->{solnprms};
    my $solnstns=$self->{solnstns};
    my $nparam=$self->{nparam};

    $self->{xyzcovar}=$fcvr;
    if( $fullcovar )
    {
        foreach my $iprm (0..$nparam-1)
        {
            $fcvr->[$iprm]=[(0)x($iprm+1)]
        }
    }

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
            /ix;
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
    return 1;
}

sub _sinexDateToTimestamp
{
    my($self,$datestr)=@_;
    my($y,$doy,$sec)=split(/\:/,$datestr);
    $y += 1900;
    $y += 100 if $y < 1980;
    return yearday_seconds($y,$doy)+$sec;
}

sub _scanEpochs
{
    my ($self,$sf)=@_;
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
            \s([\s\w\-]{4})  # solution id
            \s(\w)           # to be determined!
            \s(\d\d\:\d\d\d\:\d\d\d\d\d) # start epoch
            \s(\d\d\:\d\d\d\:\d\d\d\d\d) # end epoch
            \s(\d\d\:\d\d\d\:\d\d\d\d\d) # mean epoch
            /x;
        my ($code,$solnid,$startepoch,$endepoch,$meanepoch)=(_trim($1),_trim($2).':'._trim($3),$5,$6,$7);
        my $stn=$self->_getStation($code,$solnid);
        $stn->{epoch}=$self->_sinexDateToTimestamp($meanepoch);
        $stn->{start_epoch}=$self->_sinexDateToTimestamp($startepoch);
        $stn->{end_epoch}=$self->_sinexDateToTimestamp($endepoch);
    }
    return 1;
}


=head2 $sf->filterStationsOnly($filteredsnx)

Creates a new version of the SINEX file containing only the station 
coordinate data.

=cut

sub filterStationsOnly
{
    my( $self, $filteredsnx ) = @_;
    my $source=$self->{filename};

    open( my $tgt, ">$filteredsnx" ) || croak("Cannot open output SINEX $filteredsnx\n");
    my $src=$self->_open();

    my $solnstns=$self->{solnstns};
    my $prms=$self->{solnprms};
    my $prmmap={};
    foreach my $k (keys %$prms)
    {
        my $prm=$prms->{$k};
        my $id=$prm->{stn}->{prmoffset}+$prm->{pno};
        $prmmap->{$k}=$id+1;
    }

    my $nprm=scalar(@$solnstns)*3;

    my $section='';
    while( my $line=<$src> )
    {
        if( $line =~ /^\%\=SNX/ )
        {
            substr($line,60,5)=sprintf("%05d",$nprm);
            substr($line,68)="S           \n";
        }
        elsif( $line =~ /^\+(.*?)\s*$/ )
        {
            $section=$1;
        }
        elsif( $line =~ /^\-/ )
        {
            $section='';
        }
        elsif( $line =~ /^\*/ )
        {
        }
        elsif( $section =~ /SOLUTION\/(ESTIMATE|APRIORI)/ )
        {
            $line =~ /^
               \s([\s\d]{5})  # param id
               /x;
            my $id=_trim($1)+0;
            next if ! exists $prmmap->{$id};
            substr($line,1,5)=sprintf("%5d",$prmmap->{$id});
        }
        elsif( $section =~ /SOLUTION\/MATRIX_(ESTIMATE|APRIORI)\s+L\s+COVA/ )
        {
            my $zero=sprintf("%21.14E",0.0);
            my $vcv=[];
            foreach my $i (0..$nprm-1)
            {
                $vcv->[$i]=[];
                foreach my $j (0..$i)
                {
                    $vcv->[$i]->[$j]=$zero;
                }
            }

            for( ; $line && $line !~ /^\-/; $line=<$src> )
            {
                if( $line =~ /^\*/ )
                {
                    next;
                }
                $line =~ /^
                    \s([\s\d]{5})
                    \s([\s\d]{5})
                    \s([\s\dE\+\-\.]{21})
                    (?:\s([\s\dE\+\-\.]{21}))?
                    (?:\s([\s\dE\+\-\.]{21}))?
                    /x;
                my ($p0,$p1,$cvc)=($1+0,$2+0,[$3,$4,$5]);
                next if ! exists $prmmap->{$p0};

                my $rc0=$prmmap->{$p0}-1;
                foreach my $ip (0,1,2)
                {
                    my $p1i=$p1+$ip;
                    next if $cvc->[$ip] eq '';
                    next if ! exists $prmmap->{$p1i};
                    my $rc1=$prmmap->{$p1i}-1;
                    if( $rc1 < $rc0 ){ $vcv->[$rc0]->[$rc1]=$cvc->[$ip]; }
                    else { $vcv->[$rc1]->[$rc0]=$cvc->[$ip]; }
                }
            }

            foreach my $i (1 .. $nprm)
            {
                my $ic=$i-1;
                for(my $j0=1; $j0 <= $i; $j0+=3 )
                {
                    printf $tgt " %5d %5d",$i,$j0;
                    foreach my $k (0 .. 2)
                    {
                        my $j=$j0+$k;
                        last if $j > $i;
                        print $tgt " ".$vcv->[$ic]->[$j-1];
                    }
                    print $tgt "\n";
                }
            }
            $section='';
        }
        print $tgt $line;
    }
    close($src);
    close($tgt);
}

=head1 LINZ::GNSS::SinexFile::Site

  Solution information for a site.  Returned by SinexFile->site($code)

=cut

# Note: currently only solution and coordinate information for a site
# has been implemented.
#
# This is a minimal implementation that will hopefully support a 
# more complete implementation without needing to break the API.

package LINZ::GNSS::SinexFile::Site;

use Carp;

sub new
{
    my($class,$code,$solutions)=@_;
    my $marks={};
    my $self={code=>$code,marks=>$marks,mark=>undef};
    bless $self,$class;
    my %markdata;
    foreach my $soln (@$solutions)
    {
        my $markid=$soln->{solnid};
        $markid=~ s/\:.*//;
        push(@{$markdata{$markid}},$soln);
    }
    my $firstid=undef;
    my $lastid=undef;
    foreach my $markid (keys %markdata)
    {
        $marks->{$markid}=LINZ::GNSS::SinexFile::Mark->new($self,$markid,$markdata{$markid});
        $firstid=$markid if ! defined($firstid);
        $lastid=$markid;
    }
    $self->{mark}=$marks->{$firstid} if $firstid eq $lastid;
    return $self;
}

=head2 my $code=$site->code()

Return the code identifying the site

=cut

sub code
{
    my($self)=@_;
    return $self->{code};
}

=head2 my @marks=$site->marks()

Return an array of marks (or array ref, depending on context)

=cut

sub marks
{
    my($self)=@_;
    my @marks=values %{$self->{marks}};
    return wantarray ? @marks : \@marks;
}

=head2 my @marks=$site->mark( $markid )

Return the specified mark for a site.  If no markid is specified, then
return the mark if there is only one defined, else raise an exception

=cut

sub mark
{
    my($self,$markid)=@_;
    if( $markid eq '')
    {
        return $self->{mark} if defined($self->{mark});
        croak("No unique mark defined for site "+$self->code);
    }
    croak("Mark $markid not defined for site "+$self->code) if ! exists $self->{marks}->{$markid};
    return $self->{marks}->{$markid};
}

=head1 LINZ::GNSS::SinexFile::Mark

Solution information for a site.  Returned by SinexFile::Site->mark($markid)

=cut

# Note: currently only solution and coordinate information for a site
# has been implemented.
#
# This is a minimal implementation that will hopefully support a 
# more complete implementation without needing to break the API.

package LINZ::GNSS::SinexFile::Mark;

use LINZ::GNSS::Time qw/seconds_decimal_year/;
use Carp;

sub new
{
    my($class,$site,$markid,$solutions)=@_;
    my @solutions=sort {$a->{start_epoch} <=> $b->{start_epoch}} @$solutions;
    my $name=$solutions[0]->{name};
    my $self={site=>$site,markid=>$markid,name=>$name,solutions=>\@solutions};
    return bless $self,$class;
}

=head2 my $site=$mark->site()

Returns the site with which the mark is associated.

=cut

sub site
{
    my($self)=@_;
    return $self->{site};
}

=head2 my $code=$mark->code()

Return the code identifying the site

=cut

sub code
{
    my($self)=@_;
    return $self->{site}->code;
}

=head2 my $markid=$mark->markid()

Return the specific id of the mark

=cut

sub markid
{
    my($self)=@_;
    return $self->{markid};
}

=head2 my $code=$mark->name()

Return the name of the mark

=cut

sub name
{
    my($self)=@_;
    return $self->{name};
}

=head2 my $solution=$mark->solution($epoch,$extrapolate)

Returns the solution that applies at an epoch.  

If $extrapolate is non zero then solutions can be extrapolated past the final solution date.
If $extrapolate is > 1 then the solution may be interpolated between defined epochs (by using 
the previous epoch).
If $extrapolate is > 2 then the first solution may also be extrapolated before its start date.

=cut

sub solution
{
    my($self,$date,$extrapolate)=@_;
    my $lastsoln;
    my $inrange=0;
    foreach my $soln (@{$self->{solutions}})
    {
        $lastsoln=undef if $extrapolate < 2;
        next if $soln->{start_epoch} > $date;
        return $soln if $soln->{end_epoch} >= $date;
        $lastsoln=$soln;
    }
    return $lastsoln if $lastsoln && $extrapolate;
    return $self->{solutions}->[0] if $extrapolate > 2;
    croak("No solution found for requested mark and date\n");
}

=head2 my $xyz=$site->xyz($epoch,$extrapolate)

Calculates the XYZ coordinate for the site (and mark if ambiguous) at an epoch.

Can extrapolate solutions (see $site->solution() for info on the $extrapolate parameter).

=cut

sub xyz
{
    my($self,$date,$extrapolate)=@_;
    my $solution=$self->solution($date,$extrapolate);
    croak("No solution available for requested date\n") if ! $solution;
    my $yeardiff=seconds_decimal_year($date)-seconds_decimal_year($solution->{ref_epoch});
    my $xyz=[0.0,0.0,0.0];
    foreach my $i (0..2)
    {
        $xyz->[$i]=$solution->{xyz}->[$i]+$solution->{vxyz}->[$i]*$yeardiff;
    }
    return $xyz;
}

1;
