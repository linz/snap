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
  ['{LOCALAPPDATA}/Microsoft/AppV/Client/Integration/*/Root','/bin/python.exe','/apps/Python27'],
  ['C:/ProgramData/App-V/*/*/Root','/bin/python.exe','/apps/Python27'],
  ['D:/App-V/*/*/Root','/bin/python.exe','/apps/Python27'],
  ];
our $sitelib='/Lib/site.py';
our $savepath=$ENV{PATH};
our @needmodules=('osgeo.ogr','psycopg2','numpy');
our %needed=map {$_=>1} @needmodules;
our @wantmodules=('PyQt4','scipy');

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
    $baseglob=~s/\{(\w+)\}/$ENV{$1}/eg;
    $baseglob=~s/[\\\/]$//;
    my @option=(0,'','','');
    my $optversion;
    foreach my $exe (glob('"'.$baseglob.$relpyexe.'"'))
    {
        next if ! -x $exe;
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
        $ENV{PATH} = "$pydir;$pyhome\\scripts;".$savepath;
        $ENV{PYTHONHOME} = $pyhome;
        my ($version, $missing)=testpython($exe);
        # print("\n$pydir\n",join(" ",$version,@$missing),"\n");
        my ($status)=2;
        foreach my $m (@$missing)
        {
            $status=0;
            last if $needed{$m};
            $status=1;
        }
        my $missingstr=join(" ",@$missing);
        return ($status,$pydir,$pyhome,$version,$missingstr) if $status==2;
        next if $optversion && version->parse($version) <= $version->parse($optversion);
        @option=($status,$pydir,$pyhome,$version,$missingstr);
    }
    return @option;
}
my ($pydir,$pyhome,$version,$missing);
foreach my $t (@$testpaths)
{
    my ($status,$ppydir,$ppyhome,$pversion,$pmissing)=testpath($t);
    next if ! $status;
    ($pydir,$pyhome,$version,$missing)=($ppydir,$ppyhome,$pversion,$pmissing);
    last if $status==2;
}
print("$version\n$pydir\n$pyhome\n$missing\n") if $pydir;

