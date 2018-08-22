#!/usr/bin/perl
#  Script to run a specific test or all tests

use strict;
use Getopt::Std;
use Cwd qw(cwd abs_path);
use File::Find;
use File::Path qw(make_path remove_tree);
use File::Basename;
use File::Copy "cp";

my $syntax=<<EOD;

perl runtests.pl [options] [testid ...]

Runs regression tests.  Options are:
   -h      Print help
   -c cfg  Config file (default test.config)
   -r      Test release version
   -i      Test installed version
   -3      Test 32 bit version instead of 64 bit (windows only)
   -g      Run with debug options (linux only)
   -o      See command output/error
   -k      Keep working directory of last test
   -v      More verbose output
   -e      More verbose check output

EOD

my %opts;
getopts('vegok3irhjc:',\%opts);
die $syntax if $opts{h};
my $debug=$opts{g};
my $showoutput=$opts{o};
my $keepdir=$opts{k};
my $verbose=$opts{v};
my $check_verbose=$opts{e};
my $junksubdir=$opts{j};

# Scripts should run from configuration directory
my $cfgfile=$opts{c} || 'test.config';

my $config={
    test_dir=>'test',
    out_dir=>'out',
    check_dir=>'check',
    testre=>qr/(\w+).test/,
    configre=>'',
    env=>{},
    filere=>[],
    program=>[],
    command=>[],
    discard=>[],
    debug=>'',
    output=>'',
    error_output=>''
    };

my $nerror=0;
my $cfgdir=dirname(abs_path($cfgfile));
my $scriptdir=dirname(abs_path($0));

chdir($cfgdir);

my $cfgname=basename($cfgfile);
print "Reading configuration from $cfgfile\n" if $verbose;
open(my $cfgf, $cfgname) || die "Cannot open configuration file $cfgname\n";
foreach my $cfgl (<$cfgf>)
{
    chomp($cfgl);
    next if $cfgl =~ /^\s*(\#|$)/;
    if( $cfgl =~ /^\s*(\w+)\:\s+(\S.*?)?\s*$/ )
    {
        my $item=lc($1);
        my $value=$2;
        $value =~ s/\{configdir\}/$cfgdir/g;

        next if $item =~ /^match_/i;
        if( $item =~ /^(test_dir|out_dir|check_dir|testre|configre|output|error_output)$/ )
        {
            $value=qr/^$value$/ if $item eq 'testre' || $item eq 'configre';
            $config->{$item}=$value;
        }
        elsif( $item =~ /^(program|command|discard)$/ )
        {
            push(@{$config->{$item}},$value);
        }
        elsif( $item eq 'filere' )
        {
            my ($re,$ext)=split(' ',$value,2);
            $re=qr/$re/;
            push(@{$config->{filere}},[$re,$ext]);
        }
        elsif( $item eq 'env' )
        {
            my ($e,$v)=split(' ',$value,2);
            print "Setting env $e to $v\n" if $verbose;
            $ENV{$e}=$v;
        }
        else
        {
            print "Invalid configuration item $item in $cfgfile\n";
            $nerror++;
        }
    }
    else
    {
        print "Invalid line in $cfgfile: $cfgl\n";
        $nerror++;
    }
};
close($cfgf);
die "Errors in configuration file $cfgfile\n" if $nerror;

$config->{configre}=qr/^\s*(\w+)\:\s+(\S.*?)\s*$/ if ! $config->{configre};
push(@{$config->{filere}},[qr/^\s*file\:\s+(\S+)(?:\s+(\S+))?(?:\s+(save))?\s*$/i,'']) if ! @{$config->{filere}};
push(@{$config->{command}},'{debug}"{program}" {parameters}') if ! @{$config->{command}};

my $tstdir=$config->{test_dir};
my $outdir=$config->{out_dir};
my $chkdir=$config->{check_dir};
my $wrkbase="runtest_tmp";
my $wrkdir=$wrkbase.($keepdir ? '0000' : $$);
die "Invalid test directory $tstdir\n" if ! -d $tstdir;
die "Invalid check directory $chkdir\n" if ! -d $chkdir;
die "Configuration does not define program\n" if ! @{$config->{program}};

my $linux_config={
    name=>"Linux",
    debug_dir=>'../../linux/debug/install',
    release_dir=>'../../linux/release/install',
    install_dir=>'/usr/share/linz/snap',
    debug_prefix=>'gdb --args '
};

my $win32_config={
   name=>"Windows 32",
   debug_dir=>'../../ms/built/Debugx86',
   release_dir=>'../../ms/built/Releasex86',
   install_dir=>'C:/Program Files (x86)/Land Information New Zealand/SNAP',
};

my $win64_config={
   name=>"Windows 64",
   debug_dir=>'../../ms/built/Debugx64',
   release_dir=>'../../ms/built/Releasex64',
   install_dir=>'C:/Program Files/Land Information New Zealand/SNAP64',
};

my $windows=$^O =~ /^mswin/i;
my $arch=$windows ? $win64_config : $linux_config;
$arch=$win32_config if $windows && $opts{3};
my $build='debug';
$build='release' if $opts{r};
$build='install' if $opts{i};

my $testre=$config->{testre};

my %tests=();
find(sub{ 
        if($File::Find::dir eq $tstdir && $_ =~ /$testre/ )
        {
            $tests{$1} = $_;
        };
    },
    $tstdir
    );

my $subset=0;
if( @ARGV )
{
    $subset=1;
    my %usetests=();
    foreach my $t (@ARGV)
    {
        my $found=0;
        if( $t =~ /(.*)\*$/ )
        {
            my $re = '^'.quotemeta($1);
            foreach my $t1 (keys %tests)
            {
                if( $t1 =~ /$re/ )
                {
                    $usetests{$t1}=$tests{$t1};
                    $found=1;
                }
            }
        }
        elsif( exists($tests{$t}) )
        {
            $usetests{$t}=$tests{$t};
            $found=1;
        }
        die "Invalid test $t requested\n" if ! $found;
    }
    %tests=%usetests;
}

remove_tree($outdir) if -d $outdir;
make_path($outdir) if ! -d $outdir;
my $wrkre='^'.quotemeta($wrkbase).'\d+$';
foreach my $wd (glob($wrkbase.'*'))
{
    remove_tree($wd) if -d $wd && $wd =~ /$wrkre/;
}

my $progdir=$arch->{$build.'_dir'};
my $archname=$arch->{name};

my %programs=();
foreach my $p (@{$config->{program}})
{
    my $progpath=$p.($windows ? '.exe' : '');
    $progpath=abs_path($progdir.'/'.$progpath);
    die "$archname $build program $progpath missing\n" if ! -x $progpath;
    $programs{"program:$p"}=$progpath;
    $programs{program}=$progpath;
}

if( $verbose )
{
    print "Testing $archname $build build\n";
    print "Program directory $progdir\n";
    print "Working directory $wrkdir\n";
}

my $null = $windows ? 'nul' : '/dev/null';
my $out=$config->{output} || $null;
my $err=$config->{error_output} || $null;
$out=$null if $out eq 'null';
$err='&1' if $err eq 'output';
$err=$null if $err eq 'null';
$err='&1' if $err eq 'output' || $err eq $out;

foreach my $test (sort keys %tests)
{
    my $testname=$test;
    my $testfile=$tests{$test};
    print "\n======================================================\n".
          "Running test $testname\n" if $verbose;

    my @tstfiles=([$testfile,$testfile]);
    my %discard=();
    my %var=();
    my $param='';
    my $testcfg="$tstdir/$testfile";
    open(my $cfgf, $testcfg) || die "Cannot open $testcfg\n";
    foreach my $l (<$cfgf>)
    {
        foreach my $filedef (@{$config->{filere}})
        {
            my ($re,$ext)=@$filedef;
            if( $l =~ /$re/ )
            {
                my $fn=$1;
                my $tfn=$2;
                my $save=$3;
                $fn =~ s/\{test\}/$testname/g;
                $tfn =~ s/\{test\}/$testname/g;
                $fn=$fn.$ext if -f "$tstdir/$fn$ext" && ! -f "$tstdir/$fn";
                $tfn ||= $fn;
                push(@tstfiles,[$fn,$tfn,$save]);
            }
        }
        my $re=$config->{configre};
        if( $l =~ /$re/ )
        {
            my $item=$1;
            my $value=$2;
            if( $item eq 'parameters' )
            {
                $param=$param.' '.$value;
            }
            elsif( $item eq 'discard' )
            {
                $discard{$item}=$value;
            }
            elsif( $item eq 'set' )
            {
                my($vi,$vv)=split(' ',$value,2);
                $var{$vi}=$vv;
            }
        }
    }

    close($cfgf);
    print "Using files ".join(" ",map {$_->[0]} @tstfiles)."\n" if $verbose;

    # Clear out work directory
    remove_tree($wrkdir) if -d $wrkdir;
    make_path($wrkdir) if ! -d $wrkdir;

    # Copy input files to working directory
    my %inputfiles=();
    my $missing=0;
    foreach my $tstfile (@tstfiles)
    {
        my ($fn,$tfn,$save)=@$tstfile;
        my $src="$tstdir/$fn";
        my $tgt="$wrkdir/$tfn";
        if( ! -e $src )
        {
            print "Cannot find test file $src\n";
            $missing++;
            next;
        }
        my $odir=dirname($tgt);
        make_path($odir) if ! -d $odir;
        cp($src,$tgt);
        $inputfiles{$tgt}=1 if $save eq '';
    }
    die "Cannot run test - files missing\n" if $missing > 0;

    my %replace=(
        test=>$testname,
        testfile=>$testfile,
        configdir=>$cfgdir,
        debug=>$debug ? $arch->{debug_prefix} : '',
        parameters=>$param,
    );
    my $repfunc=sub {
        my($v)=@_;
        my $maxiter=5;
        $v =~ s/\{(\w+(?:\:\w+)?)\}/$var{$1} || $replace{$1} || $programs{$1}/eg
           while $v =~ /\{\w+\}/ && --$maxiter > 0;
        return $v;
        };

    foreach my $d (@{$config->{discard}})
    {
        my $ds=$repfunc->($d);
        $discard{$ds}=1;
    }

    # Create the  command (although currently handle multiple commands, 
    # haven't yet handled output for them
    

    my $curdir=cwd;
    chdir($wrkdir);
    foreach my $c (@{$config->{command}})
    {
        my $commandline=$c;
        $commandline .= " >$out 2>$err" if ! $debug && ! $showoutput;
        $commandline = $repfunc->($commandline);
        # Run program
        print "Running: ",$commandline,"\n" if $verbose;
        my $rc=system($commandline);
        print "Returns $rc\n" if $rc && $verbose;
    }
    chdir($curdir);

    # Copy output files to output directory

    my $wre=quotemeta($wrkdir);
    $wre=qr/$wre/;
    my $tstout=$outdir;
    $tstout .= "/$testname" unless $junksubdir;
    find( { wanted=>sub {
            next if $inputfiles{$_};
            # Can't check non-ASCII files
            next if ! -T $_;
            my $fn=$_;
            $fn=~ s/$wre[\\\/]//;
            next if $discard{$fn};
            my $ofile="$tstout/$fn";
            my $odir=dirname($ofile);
            make_path($odir) if ! -d $odir;
            cp($_,$ofile);
            print "Saving output file $ofile\n" if $verbose;
        },
        no_chdir=>1},
        $wrkdir
    );
}

remove_tree($wrkdir) if -d $wrkdir && ! $keepdir;
print "Working kept in $wrkdir\n" if -d $wrkdir;

my $params='-b';
$params .= ' -v' if $verbose || $check_verbose;
$params .= ' -e' if $subset;
my $checktests=$scriptdir.'/checktests.pl';
my $rc=system("perl $checktests $params -c $cfgname $outdir $chkdir");
my $status=$rc ? 1 : 0;
exit($status);
