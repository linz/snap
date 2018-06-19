#!/usr/bin/perl
use strict;
use FindBin;
use File::Find;
use FileHandle;
use Getopt::Std;
use POSIX;

my %opts;
getopts("iIlR",\%opts);
my $update = $opts{i} || $opts{I};
my $release = $opts{R};
my $major = $opts{I};
my $dolog = $opts{l};

my $versionfile='VERSION';
my $vf=new FileHandle("<$versionfile") || die "Cannot open version file $versionfile\n";
my $version=join('',$vf->getlines);
$vf->close;
$version=~s/\s*$//;

$version =~ /^\d+\.\d+\.\d+$/
   || die "Invalid version $version in $versionfile\n";
print "Current version is $version\n";

my $newversion='';
if( $update )
{
    my ($v1, $v2, $v3) = ($1, $2, $3);
    if( $release ){ $v1++; $v2=0; $v3=0; }
    elsif( $major ) { $v2++; $v3=0; } 
    else { $v3++; }
    $newversion="$v1.$v2.$v3";
    print "Updating version to $newversion\n";
    my $vf=new FileHandle(">$versionfile");
    $vf->print($newversion);
    $vf->close;
}

my @files;
my $root = $FindBin::Bin;
find( sub { push(@files,$File::Find::name) if /^versioninfo.c(?:pp)?$/i }, $root);

my $buildtime = strftime("%Y-%m-%d %H:%M",localtime);


my $log;

if( $dolog )
{
    $log = new FileHandle("$FindBin::Bin/versions.log","a");
    $log->print("\nBuild: $buildtime\n") if $dolog;
}



foreach my $file (@files)
{
    my $localname=substr($file,length($root));

    my $fh = new FileHandle("<$file");
    my $data = join('',$fh->getlines);
    $fh->close;
    $data =~ /^\s*\#define\s+VERSION\s+\"(\d+\.\d+\.\d+)\"/m;
    my $fversion = $1;
    print "File $localname version $fversion out of sync with $version\n"
        if $fversion ne $version;

    if( $update )
    {
        print "Updating $localname to $newversion\n";
        $data =~ s/^(\s*\#define\s+VERSION\s+\")(\d+\.\d+\.\d+)(")/$1.$newversion.$3/em;
        $fh = new FileHandle(">$file");
        $fh->print($data);
        $fh->close;
    }

    if( $dolog )
    {
        my $program = $1 if $file =~ /\/([^\/]*)\/[^\/]*$/;
        $log->print("  $program: $version\n");
    }
}
$log->close if $log;

if( ! $update )
{
    print "Use\n   -i for minor update, or\n   -I for major update or,\n   -R for new release.\nUse -l to write to version.log\n";
}
