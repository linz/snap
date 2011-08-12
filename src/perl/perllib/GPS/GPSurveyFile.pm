
package GPS::GPSurveyFile;

use strict;
use FileHandle;
use GPS;


sub new {
    my($class,$filename) = @_;
    my $fh = new FileHandle $filename;
    return undef if ! $fh;
    my $line = $fh->getline;
    return undef if $line !~ /^Project Name\:\s*(\S.*)\n/i;
    my $project = $1;
    my $self = { file=>$filename, project=>$project, fh=>$fh };
    return bless $self, $class;
}

sub DESTROY {
    my($self) = @_;
    my $fh = $self->{fh};
    close($fh) if $fh;
}

sub GetObs {
    my($self)=@_;
    my $fh = $self->{fh};
    my $line;
    my ($from,$to,$rnxfrom,$rnxto,$crdfrom,$crdto,@vector, @covar);
    my ($solntype, $solnsts, $occtime, $eph, $tropmodel );
    my ($proctime,$software);
    my ($start_time, $stop_time);
    my ($start_time_gps, $stop_time_gps);

    while( $line = $fh->getline ) {
        $line =~ s/\s+$//;

        last if $line =~ /Project Name\:/i;
        my ($record,$data) = $line =~ /^([^\:]+\:)\s+(\S.*?)\s*$/;
        my (@data) = split(' ',$data);

        $from = $data if $record =~ /^From Station\:/i;
        $to = $data if $record =~ /^To Station\:/i;
        $rnxfrom = $data[0] if $record =~ /^Data File\:/i && ! $to;
        $rnxto = $data[0] if $record =~ /^Data File\:/i && $to;
        @vector = @data[1,3,5] if $record =~ /^Baseline Components/i;
        $solntype = join(' ',@data) if $record =~ /^Solution Type/i;
        $solnsts = $data[0] if $record =~ /Solution Acceptability/i;
        $eph = $data[0] if $record =~ /^Ephemeris\:/i;
        $tropmodel = $data[0] if $record =~ /^Model\:/;
        $start_time_gps = $data if $record =~ /^Start time\:$/i;
        $stop_time_gps = $data if $record =~ /^Stop time\:$/i;

        if( $record =~ /^Occupation time/i) {
            my($h,$m,$s) = split(/\:/,$data[0]);
            $occtime = sprintf("%.2f",$h*60+$m+$s/60);
	    }
        if( $record =~ /^WGS 84 Position\:/i ) {
            my $lat = $data[0] + $data[1]/60.0 + $data[2]/3600.0;
            $lat = -$lat if $data[3] eq 'S';
            $lat = sprintf("%.5f",$lat);
            $line = $fh->getline;
            @data = split(' ',$line);
            my $lon = $data[0] + $data[1]/60.0 + $data[2]/3600.0;
            $lon = -$lon if $data[3] eq 'W';
            $lon = sprintf("%.5f",$lon);
            $line = $fh->getline;
            @data = split(' ',$line);
            my $crd = [ $lat, $lon, $data[0] ];
            $crdfrom = $crd if ! $to;
            $crdto = $crd if $to;
        }
        if( $record =~ /^Aposteriori Covariance Matrix/ ) {
            @covar = ($data[0]);
            $line = $fh->getline;
            @data = split(' ',$line);
            push(@covar,$data[0],$data[1]);
            $line = $fh->getline;
            @data = split(' ',$line);
            push(@covar,$data[0],$data[1],$data[2]);

        }

        if( $record =~ /^Processed\:/i ) {
            $proctime = "$data[1]/$data[2]/$data[3] $data[4]:00";
            $line = $fh->getline;
            $line =~ s/^\s*//; $line =~ s/\s*$//;
            $software = $line;
        }

    }
    return undef if ! $to;

    my ($sw,$ss) = $start_time_gps =~ /\(\s*(\d+)\s+(\d+\.\d+)\s*\)/;
    my ($ew,$es) = $stop_time_gps =~ /\(\s*(\d+)\s+(\d+\.\d+)\s*\)/;
    my $duration = ($ew-$sw)*(7*24*60) + ($es-$ss)/60;
    $start_time = &GPS::WeekToDate( $sw, $ss );
    $stop_time = &GPS::WeekToDate( $ew, $es );
 
    my $fromcode = uc(substr($rnxfrom,0,4));
    my $tocode = uc(substr($rnxto,0,4));


    return { from=>{ name=>$from, 
                     crd=>$crdfrom, 
                     code=>$fromcode}, 
             fromrnx=>$rnxfrom,
             to=>  { name=>$to, 
                     crd=>$crdto, 
                     code=>$tocode},
             tornx=>$rnxto,
             vector=>\@vector, 
             covar=>\@covar,
             date=>$start_time,
             GPSurvey=> {
                proctime=>$proctime, 
                software=>$software,
                type=>$solntype, 
                status=>$solnsts,
                starttime=>$start_time, 
                stoptime=>$stop_time,
                duration=>$duration,
                occtime=>$occtime, 
                ephemeris=>$eph,
                tropomodel=>$tropmodel }
            };

}

1;
