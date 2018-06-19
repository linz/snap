#!/usr/bin/perl
#  Script to run a specific test or all tests

use strict;
use Getopt::Std;
use Cwd qw(cwd abs_path);
use File::Path qw(make_path remove_tree);
use File::Basename;
use File::Copy;

my $tstdir='in';
my $outdir='out';
my $chkdir='check';
my $wrkdir="wrk$$";
my $crdsysdef="../test_coordsys/coordsys.def";

my $syntax=<<EOD;

perl runtests.pl [options] [testid ...]

Runs regression tests.  Options are:
   -h   Print help
   -r   Test release version
   -i   Test installed version
   -g   Run with debug options (linux only)
   -k   Keep working directory of last test ($wrkdir)
   -3   Test 32 bit version instead of 64 bit (windows only)
   -v   More verbose output

EOD

my %opts;
getopts('vgk3irh',\%opts);
die $syntax if $opts{h};

my $windows=$^O =~ /^mswin/i;

my $linux_versions={
    name=>"Linux",
    program=>"snap",
    debug=>'../../linux/debug/install',
    release=>'../../linux/release/install',
};

my $win32_versions={
   name=>"Windows 32",
   program=>"snap.exe",
   debug=>'../../ms/built/Debugx86',
   release=>'../../ms/built/Releasex86',
   installed=>'C:/Program Files (x86)/Land Information New Zealand/SNAP',
};

my $win64_versions={
   name=>"Windows 64",
   program=>"snap.exe",
   debug=>'../../ms/built/Debugx86',
   debug=>'../../ms/built/Debugx64',
   release=>'../../ms/built/Releasex64',
   installed=>'C:/Program Files/Land Information New Zealand/SNAP64',
};

my $subset=0;
my $version='debug';
my $versions=$windows ? $win64_versions : $linux_versions;
$versions=$win32_versions if $windows && $opts{3};
$version='release' if $opts{r};
$version='installed' if $opts{i};
my $debug=$opts{g};
my $keepdir=$opts{k};
my $verbose=$opts{v};
my @cmdprefix=();
@cmdprefix=('gdb','--args') if $opts{g};

my @tests=glob("$tstdir/test*.snp");
if( @ARGV )
{
    @tests=();
    $subset=1;
    foreach my $t (@ARGV)
    {
        $t = 'test'.$t if $t !~ /^test/;
        $t .= '.snp' if $t !~ /\.snp$/;
        $t = "$tstdir/$t";
        my @files=glob($t);
        die "Invalid test $t requested\n" if scalar(@files) == 0;
        push(@tests,@files);
    }
}
@tests=sort(@tests);


# Clear out output directory

remove_tree($outdir) if -d $outdir;
make_path($outdir) if ! -d $outdir;

# Should we put in option to run make (maybe leave to make test)

my $snapdir=$versions->{$version};
my $vername=$versions->{name};
my $snapprog=abs_path($snapdir.'/'.$versions->{program});
die "$vername $version program $snapprog missing\n" if ! -x $snapprog;

$crdsysdef=abs_path($crdsysdef);
$ENV{COORDSYSDEF}=$crdsysdef;

if( $verbose )
{
    print "Testing $vername $version\n";
    print "SNAP directory $snapdir\n";
}

my %typeext={coordinate_file=>'.crd',data_file=>'.dat',include=>'.inc'};
my $null = $windows ? 'nul' : '/dev/null';

foreach my $test (@tests)
{
    my $testname=$test;
    $testname =~ s/.*\///;
    print "\n======================================================\n".
          "Running test $testname\n" if $verbose;

    my @tstfiles=($testname);
    my @params=();
    my $tstcfg=$test;
    my $cfg=$test;
    $cfg .= '.cfg' if -e $test.'.cfg';
    open(my $cfgf, $cfg) || die "Cannot open $test\n";
    foreach my $l (<$cfgf>)
    {
        if($l =~ /^\s*(coordinate_file|data_file|include)\s+(\S+)/i)
        {
            my $ext=$typeext{$1};
            my $fn=$2;
            $fn=$fn.$ext if -f "$tstdir/$fn$ext" && ! -f "$tstdir/$fn";
            push(@tstfiles,$fn);
            if( $l =~ /\bformat\=(\S+)/ )
            {
                my $ext='.dtf';
                my $fn=$1;
                $fn=$fn.$ext if -f "$tstdir/$fn$ext" && ! -f "$tstdir/$fn";
                push(@tstfiles,$fn) if -f "$tstdir/$fn";
            }
        }
        elsif($l =~ /\s\@(\S+)/)
        {
            my $fn=$1;
            push(@tstfiles,$fn) if -f "$tstdir/$fn";
        }
        if($l =~ /^\!?\s*file\:\s+(\S.*?)\s*$/ )
        {
            push(@tstfiles,split(' ',$1));
        }
        elsif($l =~ /^\!?\s*parameters\:\s*(.*?)\s*$/ )
        {
            @params=split(' ',$1);
        }
    }
    close($cfgf);
    print "Using files ".join(" ",@tstfiles)."\n" if $verbose;

    # Clear out work directory
    remove_tree($wrkdir) if -d $wrkdir;
    make_path($wrkdir) if ! -d $wrkdir;

    # Copy input files to working directory
    my %inputfiles=();
    foreach my $fn (@tstfiles)
    {
        die "Cannot find test file $tstdir/$fn" if ! -f "$tstdir/$fn";
        my $ofn="$wrkdir/$fn";
        my $odir=dirname($ofn);
        make_path($odir) if ! -d $odir;
        copy("$tstdir/$fn","$wrkdir/$fn");
        $inputfiles{"$wrkdir/$fn"}=1;
    }

    # Create the SNAP command
    my @command=@cmdprefix;
    push(@command,$snapprog,@params,$testname);

    # Run SNAP
    my $curdir=cwd;
    chdir($wrkdir);
    # system(@command);
    # But want to redirect, so using system with string
    my $commandline='"'.join('" "',@command).'" > '.$null.' 2>&1';
    print "Running: ",$commandline,"\n" if $verbose;
    system($commandline);
    chdir($curdir);

    # Copy output files to output directory

    foreach my $fn (glob("$wrkdir/*"))
    {
        # Skip input files and untestable binary file
        next if -d $fn;
        next if exists $inputfiles{$fn};
        next if $fn =~ /\.bin$/;
        # For the moment not copying new file
        next if $fn =~ /\.new$/;
        # Copy all other files
        my $ofn=$outdir.substr($fn,length($wrkdir));
        # Want to get to copy but for the moment use clean
        copy($fn,$ofn);
        #system("$^X clean_snap_listing.pl $fn > $ofn");
    }
}

remove_tree($wrkdir) if -d $wrkdir && ! $keepdir;

my $params='-b';
$params .= ' -v' if $verbose;
$params .= ' -e' if $subset;
system("perl checktests.pl $params $outdir $chkdir");
