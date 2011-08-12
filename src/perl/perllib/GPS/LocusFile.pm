
package GPS::LocusFile;

use Geodetic::Ellipsoid;

use strict;
use FileHandle;
use GPS;


sub new {
    my($class,$filename) = @_;
    my $fh = new FileHandle $filename;
    return undef if ! $fh;
    my($line,$units,$ellipsoiddef,$refdate);
    $line = $fh->getline;
    die "Invalid file header\n" if $line !~ /^Processed Vectors/i;

    while( $line = $fh->getline ) {
       last if $line !~ /^\_\_\_\_/$;
       chomp;
       my $v1 = substr($line,0,60);
       my $v2 = substr($line,60);
       for ($v1,$v2) {
           my( $code,$value ) = split(/\:\s+/,$_,2);
           if( lc($code) eq date ) {
              my($mn,$dy,$yr) = $value =~ /(\w+)/g;
              $yr += 2000 if $yr < 90;
              $yr += 1900 if $yr < 1000;
              $refdate = [$dy,$mn,$yr];
              next;
              }
            if( lc($code) eq 'horizontal coordinate system' ) {
              $ellipsoiddef = $value;
              next;
              }
            if( lc($code) eq 'linear units of measure' ) {
              $units = $value;
              }
            }
       }

    die "Invalid ellipsoid definition in $filename\n" 
       if $ellipsoiddef !~ /^w.*g.*s.*84$/i;
    die "Invalid units in $filename\n" if $units !~ /^meters$/i;
    my $ellipsoid = new Geodetic::Ellipsoid( 6378137.0, 298.257223563, "WGS 1984", "WGS84" );
    my $project = "GPS data from $filename";
    my $self = { file=>$filename, 
                 project=>$project, 
                 fh=>$fh,
                 ellipsoid=>$ellipsoid,
                 stnlist=>{}
                 };
    return bless $self, $class;
}

sub DESTROY {
    my($self) = @_;
    my $fh = $self->{fh};
    close($fh) if $fh;
}

sub CreateStation {
    my ($self, $code, $xyz ) = @_;
    my $crd = $self->{ellipsoid}->geog( $xyz );
    return { code=>$code, crd=>$crd };
    }

sub GetObs {
    my($self)=@_;
    my $fh = $self->{fh};
    my $nextline = $self->{nextline};
    my $stnlist = $self->{stnlist};
    my $inbaseline = 0;
    my ($from, $to, $vec, $cvr, $date );
    my ($laststn);
    for( ; $nextline; $nextline = $fh->getline ){
       next if $nextline !~ /^\@(.)(.*)/;
       my( $code, $data ) = ($1, $2);
       last if $code eq '+' && $inbaseline;
       $inbaseline = 1 if $code eq '+';
       chomp;
       if($code =~ /[\+\-\#]/ ) {
          my $stncode = substr($data, 0, 16 );
          my ( @xyz ) = split(' ',substr($data,16));
          if( $code eq '+' || $code eq '#' ) {
             $laststn = $stnlist->{$stncode};
             if( ! $laststn ) {
                $laststn = $self->CreateStation( $stncode, \@xyz );
                $stnlist->{$stncode} = $laststn;
                }
             $from = $laststn if $code eq '+';
             }
          else {
             $to = $stnlist->{$stncode};
             $vec = \@xyz;
             }
          }
       elsif( $code eq '4' || $code eq '1') {
          $data =~ s/^\s+//;
          $data =~ s/\s+$//;
          $laststn->{name} = $data;
          }
       elsif( $code eq '&' ) {
          my ($seu) = split(' ',$data);
          $laststn->{fixed} = 1 if $seu <= 0.0;
          }

       elsif( $code eq '=' ) {
          my($seu,@covar) = split(' ',$data);
          foreach(@covar) { $_ *= $seu*$seu; }
          # Convert upper triangle to lower triangle storage
          my $tmp = $covar[2]; $covar[2] = $covar[3]; $covar[3] = $tmp;
          $cvr = \@covar;
          }
       elsif( $code eq '*' ) {
          my( $mon, $day, $year, $hour, $min, $sec ) = 
             $data =~ /(\d+)\/(\d+)\/(\d+)\s+(\d+)\:(\d+)\:(\d+)/;
          $year += 2000 if $year < 50;
          $year += 1900 if $year < 150;
          $date = "$day/$mon/$year $hour:$min:$sec";
          }
       
       }
    $self->{nextline} = $nextline;

    return undef if ! $from || ! $to;

    return { from=>$from,
             to=>$to,
             vector=>$vec, 
             covar=>$cvr,
             date=>$date
           };
}

1;
