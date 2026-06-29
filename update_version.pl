#!/usr/bin/perl
use strict;
use FindBin;
use FileHandle;
use Getopt::Std;
use POSIX;

my $syntax=<<EOD;

perl update_version.pl [options]

To update files use one of:
   -k keep existing version but update files
   -i for minor update, 
   -I for major update,
   -R for new release.
   -a include uncommitted files in git index in version update

Requires all changed files to be added to git index.  If -a not specified
then all changes must be committed.

EOD

my %opts;
getopts("kRIiaG",\%opts);
my $keep= $opts{k};
my $release = $opts{R};
my $major = $opts{I};
my $minor = $opts{i};
my $add_index = $opts{a};
my $update = $keep || $release || $major || $minor;

if( $update && system('git diff --quiet HEAD') )
{
    if( system('git diff --quiet') )
    {
        die "Cannot update version - unchanged files not added or committed in repository\n";
    }
    die "Cannot update version - unchanged files not committed (use -a option or git commit)\n"
        if ! $add_index;
}

my $versionfile='VERSION';
my $snapversionfile='src/snaplib/snapversion.h';
my $vf=new FileHandle("<$versionfile") || die "Cannot open version file $versionfile\n";
my $version=join('',$vf->getlines);
$vf->close;
$version=~s/\s*$//;

$version =~ /^(\d+)\.(\d+)\.(\d+)$/
   || die "Invalid version $version in $versionfile\n";
print "Current version is $version\n";
my $v1=$1 || 1;
my $v2=$2 || 0;
my $v3=$3 || 0;

my $newversion=$version;
if ( $update )
{
    if( ! $keep ) 
    { 
        if( $release ){ $v1++; $v2=0; $v3=0; }
        elsif( $major ) { $v2++; $v3=0; } 
        else { $v3++; }
        $newversion="$v1.$v2.$v3";
    }
    if( ! $keep )
    {
        if( open(my $gtf,"git tag |"))
        {
            while (my $tag=<$gtf>)
            {
                $tag=~s/\s//g;
                die "Git tag $tag already defined\n" if $tag eq $newversion;
            }
        }
    }
    print "Updating version to $newversion\n";
    my $vf=new FileHandle(">$versionfile");
    $vf->print($newversion);
    $vf->close;
    $vf=new FileHandle(">$snapversionfile");
    $vf->print("#define SNAPVERSION \"$newversion\"\n");
    $vf->close;
}

if( $update )
{
    system("git commit -a -m \"Updating version to $newversion\"")
        if system('git diff --quiet HEAD');
    system("git tag -a -f $newversion -m \"Version $newversion\"");
}

print $syntax if ! $update;
