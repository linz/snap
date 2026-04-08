#!/usr/bin/perl

use strict;
use File::Find;
use FindBin;
use IPC::Open3;
use Win32;
use version;

our $pyprog=$FindBin::Bin.'/testpython.py';

our $testpaths=[
  ['C:/Program Files/QGIS*','/bin/python.exe','/apps/Python*'],
  ];
our $sitelib='/lib/site.py';
our $savepath=$ENV{PATH};
our $wantversion=qr/^3\./;
our @needmodules=('osgeo.ogr','numpy');
our %needed=map {$_=>1} @needmodules;
our @wantmodules=('PyQt5','scipy');
our $pypath_template="{pyhome}/Lib;{pyhome}/DLLs;{pyhome}/Lib/site-packages;{pyhome}/Lib/site-packages/win32;{pyhome}/Lib/site-packages/win32/lib;{pyhome}/Lib/site-packages/Pythonwin";
our $verbose;

sub testpython
{
    my($pyexe)=@_;
    my $command=join(" ",'"'.$pyexe.'"','"'.$pyprog.'"',@needmodules,@wantmodules);
    print "Running $command\n" if $verbose;
	my $result=`$command`;
    print "Result: $result\n" if $verbose;
	my @missing=split(' ',$result);
    my $version=shift @missing;
	return $version,\@missing;
}

sub cmpVersions
{
    my($a,$b)=@_;
    my @partsa=split(/\./, $a);
    my @partsb=split(/\./, $b);
    while(@partsa && @partsb)
    {
        my $aversion=shift @partsa;
        my $bversion=shift @partsb;
        return $aversion <=> $bversion if $aversion != $bversion;
    }
    return 1 if @partsa;
    return -1 if @partsb;
    return 0;
}


sub cmpPathsWithVersion
{
    my @partsa=split(/([\d\.]+)/, $a);
    my @partsb=split(/([\d\.]+)/, $b);
    while(@partsa && @partsb)
    {
        my $aversion=shift @partsa;
        my $bversion=shift @partsb;
        my $cmp =cmpVersions($aversion, $bversion) if $aversion=~/[\d\.]+/ && $bversion=~/[\d\.]+/ && $aversion ne $bversion;
        return $cmp if defined $cmp;
        return $aversion cmp $bversion if $aversion ne $bversion;
    }
    return 1 if @partsa;
    return -1 if @partsb;
    return 0;
}

sub testpath
{
    my($testpath)=@_;
    my($baseglob,$relpyexe,$relpyhome)=@$testpath;
    print "Testing $baseglob$relpyexe\n" if $verbose;
    $baseglob=~s/\{(\w+)\}/$ENV{$1}/eg;
    $baseglob=~s/[\\\/]$//;
    my @option=(0,'','','');
    my $optversion;
    my @basepaths=glob('"'.$baseglob.'"');
    @basepaths=reverse sort cmpPathsWithVersion @basepaths;
    foreach my $basepath (@basepaths)
    {
        print "Found $basepath\n" if $verbose;
        if( ! -d $basepath)
        {
            print "Error - $basepath not a directory\n" if $verbose;
            next;
        }
        $basepath = Win32::GetShortPathName($basepath);
        my $exe = $basepath.$relpyexe;
        if( ! -x $exe ) 
        {
            print "Error - cannot find executable $exe\n" if $verbose;
            next;
        }
        print "Found $exe\n" if $verbose;
        my $pyhome=undef;
        print "Searching $basepath$relpyhome\n" if $verbose;
        foreach my $homedir (glob($basepath.$relpyhome))
        {
            print "Testing python home $homedir\n" if $verbose;
            if( ! -d $homedir)
            {
                print "Not a directory\n" if $verbose;
                next;
            }
            if( ! -f $homedir.$sitelib)
            {
                print "No $homedir.$sitelib\n" if $verbose;
                next;
            }
            $pyhome=Win32::GetShortPathName($homedir);
            last;
        }
        next if ! $pyhome;
        my $pydir = Win32::GetShortPathName($exe);
        $pydir =~ s/[\\\/][^\\\/]*$//;
        $pydir =~ s~\/~\\~g;
        $pyhome =~ s~\/~\\~g;
        my $pypath=$pypath_template;
        $pypath =~ s/\{pyhome\}/$pyhome/eg;
        $pypath =~ s/\{pydir\}/$pydir/eg;
        $ENV{PATH} = "$pydir;$pyhome\\scripts;".$savepath;
        $ENV{PYTHONHOME} = $pyhome;
        $ENV{PYTHONPATH} = $pypath;
        print "PYDIR: $pydir\nPYHOME $pyhome\nPYPATH $pypath\n" if $verbose;

        my ($version, $missing)=testpython($exe);
        next if ! $version;
        next if $version !~ $wantversion;
        my $missingstr=join(" ",@$missing);
        print "Version $version: missing $missingstr\n" if $verbose;
        my ($status)=2;
        foreach my $m (@$missing)
        {
            $status=0;
            last if $needed{$m};
            $status=1;
        }
        print "Status: $status\n" if $verbose;
        return ($status,$pydir,$pyhome,$pypath,$version,$missingstr) if $status==2;
        next if $optversion && version->parse($version) <= $version->parse($optversion);
        @option=($status,$pydir,$pyhome,$pypath,$version,$missingstr) if ! $option[0];
    }
    return @option;
}

$verbose=1 if $ARGV[0] eq '-v';
my ($pydir,$pyhome,$pypath,$version,$missing);
my $status=0;
foreach my $t (@$testpaths)
{
    my ($pstatus,$ppydir,$ppyhome,$ppypath,$pversion,$pmissing)=testpath($t);
    next if $pstatus <= $status;
    $status=$pstatus;
    ($pydir,$pyhome,$pypath,$version,$missing)=($ppydir,$ppyhome,$ppypath,$pversion,$pmissing);
    last if $pstatus==2;
}
my $reqmodulestr=join(", ",@needmodules);
if( ! $status)
{
    print("Python with required modules ($reqmodulestr) not found\n");
    $ENV{PATH} = $savepath;
    exit 1;
}
$ENV{PATH} = "$pydir;$pyhome\\scripts;".$savepath;
$ENV{PYTHONHOME} = $pyhome;
$ENV{PYTHONPATH} = $pypath;
print "$version\n$pydir\n$pyhome\n$pypath\n$missing\n";

