package LINZ::GNSS::Time;
################################################################################
## $Id: Time.pm 633 2008-08-28 22:28:25Z jpalmer $
##
## AUSPOS system is a collaborative project between Geosciences Australia and
## Land Information New Zealand.
##
## Authors: Mike Moore     (Michael.Moore@ga.gov.ga)
##          Jeremy Palmer  (jpalmer@linz.govt.nz)
##
## TODO:
##
##
################################################################################

=head1 LINZ::GNSS::Time

LINZ::GNSS::Time - provides miscellaneous time functions and constants

The functions and constants can be exported with

    use LINZ::GNSS::Time qw/time_elements .../;

The following components can also  be exported

    $SECS_PER_DAY
    $SECS_PER_HOUR
    $SECS_PER_WEEK
    $GNSSTIME0
    $TIMERE

=cut

use strict;
use warnings;

use Carp qw(croak carp);
use base qw(Exporter);
use Time::Local;

our @EXPORT = qw(
    time_elements
    alpha2hour
    hour2alpha
    datetime_seconds
    seconds_datetime
    ymdhms_seconds
    seconds_ymdhms
    start_time_utc_day
    start_time_gnss_week
    seconds_decimal_year
    decimal_year_seconds
    localdaytime_seconds
    gnssweek_seconds
    year_seconds
    yearday_seconds
    seconds_yearday
    get_week_start
    is_leap_year
    seconds_julianday
    julianday_seconds
    parse_gnss_date
    seconds_decimal_yr
    decimal_yr_seconds
);

our @EXPORT_OK = qw(
    $SECS_PER_DAY
    $SECS_PER_HOUR
    $SECS_PER_WEEK
    $GNSSTIME0
    $TIMERE
);

our $GNSSTIME0      = timegm( 0, 0, 0, 6, 0, 80 );
our $SECS_PER_HOUR = 60 * 60;
our $SECS_PER_DAY  = $SECS_PER_HOUR * 24;
our $SECS_PER_WEEK = $SECS_PER_DAY * 7;
our @alpha_hours   = qw(a b c d e f g h i j k l m n o p q r s t u v w x);
our $DATERE        = qr/
                     (\d{4})
                     (?:\-|\s+)
                     (?:
                        (\d\d)(?:\-|\s+)(\d\d)|
                        (\d\d\d)
                     )
                     (?:
                     (?:\s+|T\s*)
                     (\d\d?)(?:\:|\s+)
                     (\d\d)(?:\:|\s+)
                     (\d\d)
                     )?/x;
our $TIMERE        = qr/^\s*(\d\d?):(\d{2}):(\d{2})\s*$/;

my @MONTH_DAYS     = ( 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 );
my %START_YR_TIMES = ();

=head2 ($year,$gnssweek,$doy,$wday,$hour) = time_elements($time);

Returns a set of time components for an epoch

=cut

sub time_elements {
    my $time = shift;
    my ($year, $yday, $doy, $gnss_week, $wday, $hour);
    ($year, $yday, $wday, $hour) = (gmtime($time))[ 5, 7, 6, 2 ];
    $year += 1900;
    $doy = sprintf( "%03d", $yday + 1 );
    $gnss_week = int( ( $time - $GNSSTIME0 ) / $SECS_PER_WEEK );
    return $year, $gnss_week, $doy, $wday, $hour;
}

# Convert the alphabetic version of the hour into the corresponding
# numerical version (2 digits).

sub alpha2hour {
    my $houralpha = shift;
    my $count = 0;

    until ($alpha_hours[$count] eq $houralpha ){
        ++$count;
    }
    return sprintf "%02d",$count;
}

sub hour2alpha {
    my $hour = shift;
    return $alpha_hours[$hour];
}

=head2 $seconds=datetime_seconds($date_string)

Converts a date string to timestamp.  The string can be formatted as 

=over

=item "YYYY-MM-DD"

=item "YYYY-DDD"

=item "YYYY-MM-DDThh:mm:ss"

=item "YYYY-DDDThh:mm:ss"

=back

The hypen, colon, and T delimiters can be replaced with spaces.

=cut

sub datetime_seconds {
    my $date = shift;
    my $local = shift;
    return unless $date;
    my ($year,$mon,$day,$doy,$hr,$min,$sec) = $date =~ $DATERE;
    croak "Datetime string is malformed: $date" unless defined $year;
    $mon--; $year -= 1900;
    my $offset=0;
    if( defined($doy) )
    {
        $mon=0; $day=1; $offset=($doy-1)*$SECS_PER_DAY;
    }
    my $seconds;
    if ($local) {
        $seconds = timelocal($sec, $min, $hr, $day, $mon, $year);
    }
    else {
        $seconds = timegm($sec, $min, $hr, $day, $mon, $year);
    }
    return $seconds+$offset;
}

=head2 $datetime=seconds_datetime($epoch,$local)

Converts an epoch to a date time string formatted YYYY-MM-DD HH:MM:SS.
If $local is 1 then the epoch is converted to a local time, otherwise
it is GMT(UTC)

=cut

sub seconds_datetime {
    my $time = shift;
    my $local = shift;
    return "" unless defined $time;
    my ($sec, $min, $hr, $day, $mon, $year);
    if ($local) {
        ($sec, $min, $hr, $day, $mon, $year) = localtime($time);
    }
    else {
        ($sec, $min, $hr, $day, $mon, $year) = gmtime($time);
    }
    $mon++; $year += 1900;
    return sprintf "%04d-%02d-%02d %02d:%02d:%02d",
        $year,$mon,$day,$hr,$min,$sec;
}

=head2 $seconds=ymdhms_seconds($y,$m,$d,$hh,$mm,$ss,$local)

Returns the epoch seconds from the numeric date components.  Note
that the year includes the century, and the month is between 1 and 12.

=cut

sub ymdhms_seconds {
    my($year,$mon,$day,$hour,$min,$sec,$local)= @_;
    $year -= 1900;
    $mon--;
    my $seconds;

    if ($local) {
        $seconds = timelocal(0, $min, $hour, $day, $mon, $year)+$sec;
    }
    else {
        $seconds = timegm(0, $min, $hour, $day, $mon, $year)+$sec;
    }
    return $seconds;
}

=head2 ($year,$month,$day,$hour,$min,$sec)=seconds_ymdhms($seconds,$local)

Returns a timestamp split into component parts.  The year includes the century
and the months are numbered 1 to 12.  Uses gmtime unless $local evaluates to true.

=cut

sub seconds_ymdhms
{
    my($seconds,$local)=@_;
    my ($sec, $min, $hr, $day, $mon, $year);
    if( $local )
    {
        ($sec, $min, $hr, $day, $mon, $year) = localtime($seconds);
    }
    else
    {
        ($sec, $min, $hr, $day, $mon, $year) = gmtime($seconds);
    }
    $mon++;
    $year+=1900;
    return ($year,$mon,$day,$hr,$min,$sec);
}



sub year_seconds {
    my $year = shift;
    my $local = shift;
    my $seconds;
    if ($year >= 0 && $year < 100) {
        if ($year < 49) {
            $year += 100;
        }
    }
    else {
        $year -= 1900;
    }
    if ($local) {
        $seconds = timelocal(0,0,0,1,0,$year);
    }
    else {
        $seconds = timegm(0,0,0,1,0,$year);
    }
    return $seconds;
}

=head2 $jday=seconds_julianday($seconds)

Returns the julian day corresponding to a timestamp.

=cut

sub seconds_julianday
{
    my ($seconds)=@_;
    # 40587 is modified julian date of 1 Jan 1970 - reference epoch
    # for timestamp
    my $jday=int($seconds/(60*60*24))+40587;
    return $jday;
}

=head2 $seconds=julianday_seconds($jday)

Returns the timestamp corresponding to a julian day

=cut

sub julianday_seconds
{
    my ($jday)=@_;
    return ($jday-40587)*(60*60*24);
}

=head2 $dyear=seconds_decimal_year($seconds)

Returns the date in decimal years corresponding to a timestamp

=cut

sub seconds_decimal_year {
    my $time = shift;
    my ($sec, $min, $hr, $day, $mon, $year) = gmtime($time);
    my $yr_start1 = $START_YR_TIMES{$year}
        || ($START_YR_TIMES{$year} = timegm(0, 0, 0, 1, 0, $year));
    my $yr_start2 = $START_YR_TIMES{$year+1}
        || ($START_YR_TIMES{$year+1} = timegm(0, 0, 0, 1, 0, $year+1));
    my $decimal_days = ($time - $yr_start1) / ($yr_start2 - $yr_start1);
    return $year + 1900 + $decimal_days;
}

sub seconds_decimal_yr { return seconds_decimal_year(@_); }

=head2 $seconds=decimal_year_seconds($dyear)

Returns the timestamp corresponding to a date in decimal years.

=cut

sub decimal_year_seconds {
    my $decimal_yr = shift;
    my $year = int($decimal_yr);
    my $dec_part = $decimal_yr - $year;
    $year -= 1900;
    my $yr_start1 = $START_YR_TIMES{$year}
        || ($START_YR_TIMES{$year} = timegm(0, 0, 0, 1, 0, $year));
    my $yr_start2 = $START_YR_TIMES{$year+1}
        || ($START_YR_TIMES{$year+1} = timegm(0, 0, 0, 1, 0, $year+1));
    return $yr_start1 + ($yr_start2  - $yr_start1) * $dec_part;
}
sub decimal_yr_seconds { return decimal_year_seconds(@_); }

sub start_time_utc_day {
    my $time = shift;
    my ($hour, $mday, $wday, $mon, $year)
        = (gmtime( $time ) )[ 2, 3, 6, 4, 5 ];
    my $startofUTCdaytime = timegm( 0, 0, 0, $mday, $mon, $year );
    return $startofUTCdaytime;
}

sub start_time_gnss_week {
    my $time = shift;
    my $gnss_week = int( ( $time - $GNSSTIME0 ) / $SECS_PER_WEEK );
    my $start_time = $GNSSTIME0 + $gnss_week * $SECS_PER_WEEK;
}

=head2 $secs=gnss_week($weekno)

Converts a GNSS week to the timestamp for the start of the week

=cut 

sub gnssweek_seconds {
    my $gnss_week = shift;
    return $gnss_week * $SECS_PER_WEEK + $GNSSTIME0;
}

sub localdaytime_seconds {
    my $day_secs = shift;
    my ( $hours, $mins, $secs ) = $day_secs =~ $TIMERE;
    my $daysecs = $hours * $SECS_PER_HOUR + $mins * 60 + $secs;
    my $current_time = time;
    my ( $sec, $min, $hour, $mday, $mon, $year ) = localtime($current_time);
    my $sod = timelocal( 0, 0, 0, $mday, $mon, $year );
    my $time = $sod + $daysecs;
    if ( $time < $current_time ) {
        $time += $SECS_PER_DAY;
    }
    return $time;
}

sub get_week_start {
    my $time = shift;
    my ( $mday, $wday, $mon, $year ) = ( gmtime($time) )[ 3, 6, 4, 5 ];
    if ( $wday != 7 ) {
        $time = $time - $wday * $SECS_PER_DAY;
    }
    ($mday, $mon, $year) = (gmtime( $time ) )[ 3, 4, 5 ];
    return timegm( 0, 0, 0, $mday, $mon, $year );
}

sub is_leap_year {
   my $year = shift;
   return 0 if $year % 4;
   return 1 if $year % 100;
   return 0 if $year % 400;
   return 1;
}

=head2 my $seconds=yearday_seconds($year,$dayno)

Determines the timestamp for the start of the day specified by year and day number.

=cut 

sub yearday_seconds
{
    my($year,$day) = @_;
    return year_seconds($year)+($day-1)*$SECS_PER_DAY;
}

=head2 my ($year,$dayno)=seconds_yearday($seconds)

Converts a timestamp to a year and day number

=cut 

sub seconds_yearday
{
    my( $seconds ) = @_;
    my($year,$dayno) = (time_elements($seconds))[0,2];
    return ($year,$dayno);
}

=head2 my $start, $end = LINZ::GNSS::Time::session_startend($year,$session)

Determines the start and end epochs for a session defined by a year, and a session as DDDS
where S is 0 for the whole day, or a-x for individual hours.

Note that the session covers the times $start <= $t < $end - $end is not part of the 
interval.

=cut 

sub session_startend
{
    my($year,$session) = @_;

    carp("session $session");
    if(lc($session) !~ /^(\d{1,3})([0a-x])/)
    {
        croak("Invalid session id $session");
    }
    my($day,$sessionid)=($1,$2);
    carp("session $session: day $day: sessionid: $sessionid");
    my($hour,$length);
    if( $sessionid eq '0' )
    {
        $hour=0;
        $length=24;
    }
    else
    {
        $hour=alpha2hour($sessionid);
        $length=1;
    }
    my $start = yearday_seconds($year,$day)+$hour*$SECS_PER_HOUR;
    my $end = $start + $length*$SECS_PER_HOUR;
    return ($start,$end);
}

=head2 $seconds=parse_gnss_date($datestr,$local)

Parses a date in one of the following formats:

   yyyy-mm-dd    Year, month, day
   dd-mm-yyyy    Day, month, year
   yyyymmdd      Year, nonth, day
   yyyy-ddd      Year, day number
   wwww-d        GPS week, day
   ssssssssss    Unix time stamp
   jjjjj         Julian day
   now           Right now
   now-ddd       ddd days before now
   today         Start of current UTC day
   today-ddd     ddd days before today
   yyyy.yyyy     decimal years

If the date cannot be interpreted then croaks.

=cut

sub parse_gnss_date
{
    my($datestr)=@_;
    my $seconds;
    eval {
        # yyyy mm dd
        if( $datestr=~ /^((?:19|20)\d\d)\W+([01]?\d)\W+([0123]?\d)$/ )
        {
            $seconds=ymdhms_seconds($1,$2,$3,0,0,0);
        }
        # dd mm yyyy
        elsif( $datestr=~ /^([0123]?\d)\W+([01]?\d)\W+((?:19|20)\d\d)$/ )
        {
            $seconds=ymdhms_seconds($3,$2,$1,0,0,0);
        }
        # yyyymmdd
        elsif( $datestr=~ /^((?:19|20)\d\d)([01]\d)([0123]\d)$/ )
        {
            $seconds=ymdhms_seconds($1,$2,$3,0,0,0);
        }
        # wwww d  (week day)
        elsif( $datestr=~/^(\d\d\d\d)(?:w\s*|\W+)([0-6])$/i )
        {
            $seconds=gnssweek_seconds($1)+$2*$SECS_PER_DAY;
        }
        # yyyy ddd
        elsif( $datestr=~/^((?:19|20)\d\d)\W+(\d{1,3})$/ )
        {
            $seconds=yearday_seconds($1,$2);
        }
        # ssssssssss
        elsif( $datestr =~ /^\d{10}$/ )
        {
            $seconds=$datestr+0;
        }
        # ddddd
        elsif( $datestr =~ /^\d{5}$/ )
        {
            $seconds=julianday_seconds($datestr);
        }
        # yyyy.yyy
        elsif( $datestr =~ /^[12]\d\d\d\.\d+$/ )
        {
            $seconds=decimal_year_seconds($datestr);
        }
        # now-#
        elsif( lc($datestr) =~ /^now(?:\-(\d+))?$/ )
        {
            $seconds=time()-($1 // 0)*$SECS_PER_DAY;
        }
        # today-#
        elsif( lc($datestr) =~ /^today(?:\-(\d+))?$/ )
        {
            my($sec,$min,$hour)=gmtime();
            my $daysecs=$sec+$min*60+$hour*3600;
            $seconds=time()-$daysecs-($1 // 0)*$SECS_PER_DAY;
        }
        else
        {
            $seconds=datetime_seconds($datestr);
        }
    };
    if( $@ )
    {
        croak("Invalid date specified: $datestr");
    }
    return $seconds;
}
    



1;
