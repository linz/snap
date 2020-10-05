#!/usr/bin/perl

use strict;
use File::Find;
use FindBin;
use IPC::Open3;
use Win32;
use version;

our $pyprog=$FindBin::Bin.'/testpython.py';

our $testpaths=[
  ['{OSGEO4W}','/bin/python.exe','/apps/Python27'],
  ['C:/Program Files/QGIS*','/bin/python.exe','/apps/Python27'],
  ['{LOCALAPPDATA}/Microsoft/AppV/Client/Integration/*/Root','/bin/python.exe','/apps/Python27'],
  ['C:/ProgramData/App-V/*/*/Root','/bin/python.exe','/apps/Python27'],
  ['D:/App-V/*/*/Root','/bin/python.exe','/apps/Python27'],
  ];
our $sitelib='/Lib/site.py';
our $savepath=$ENV{PATH};
our @needmodules=('osgeo.ogr','psycopg2','numpy');
our %needed=map {$_=>1} @needmodules;
our @wantmodules=('PyQt4','scipy');
our $pypath_template="{pyhome}/Lib;{pyhome}/DLLs;{pyhome}/Lib/plat-win;{pyhome}/Lib/site-packages";
our $verbose;

sub testpython
{
    my($pyexe)=@_;
my $command=join(" ",'"'.$pyexe.'"','"'.$pyprog.'"',@needmodules,@wantmodules);
	my $result=`$command`;
	my @missing=split(' ',$result);
    my $version=shift @missing;
	return $version,\@missing;
}

sub testpath
{
    my($testpath)=@_;
    my($baseglob,$relpyexe,$relpyhome)=@$testpath;
    print "Testing $baseglob.$relpyexe\n" if $verbose;
    $baseglob=~s/\{(\w+)\}/$ENV{$1}/eg;
    $baseglob=~s/[\\\/]$//;
    my @option=(0,'','','');
    my $optversion;
    foreach my $exe (glob('"'.$baseglob.$relpyexe.'"'))
    {
        next if ! -x $exe;
        print "Found $exe\n" if $verbose;
        my $basepath=substr($exe,0,-length($relpyexe));
        my $pyhome=$basepath.$relpyhome;
        next if ! -d $pyhome;
        next if ! -f $pyhome.$sitelib;
        my $pydir = $exe;
        $pydir =~ s/[\\\/][^\\\/]*$//;
        $pydir=Win32::GetShortPathName($pydir);
        $pyhome=Win32::GetShortPathName($pyhome);
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
        @option=($status,$pydir,$pyhome,$pypath,$version,$missingstr);
    }
    return @option;
}

$verbose=1 if $ARGV[0] eq '-v';
my ($pydir,$pyhome,$pypath,$version,$missing);
foreach my $t (@$testpaths)
{
    my ($status,$ppydir,$ppyhome,$ppypath,$pversion,$pmissing)=testpath($t);
    print "Found: $status $ppydir $ppyhome $ppypath $pversion $pmissing\n" if $verbose;
    next if ! $status;
    ($pydir,$pyhome,$pypath,$version,$missing)=($ppydir,$ppyhome,$ppypath,$pversion,$pmissing);
    last if $status==2;
}
print "$version\n$pydir\n$pyhome\n$pypath\n$missing\n" if $pydir || $verbose;

