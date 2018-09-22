#!/usr/bin/perl
use strict;
use FindBin;
use File::Find;
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

sub guid
{
    my $char='0123456789ABCDEF';
    my $char2='89AB';
    my $uid=sprintf("%X",time);
    while( length($uid) < 30 )
    {
        $uid.=substr($char,int(rand(16.0)),1);
    }
    my $vchar=substr($char2,int(rand(4.0)),1);
    my $guid=substr($uid,0,8).'-'. substr($uid,8,4).
            '-4'.substr($uid,12,3).'-'.$vchar.substr($uid,15,3).
            '-'.substr($uid,18,12);
    return $guid;
}

sub update_ms_vdproj
{
    my($pjfile,$version,$update)=@_;
    my $localname=$pjfile;
    $localname=~s/.*[\\\/]//;
    my $fh=new FileHandle($pjfile,"r");
    binmode($fh);
    die "Cannot open MS Project file $pjfile\n" if ! $fh;
    my @pjdata=<$fh>;
    $fh->close;

    my $npv=-1;
    my $npk=-1;
    my $npd=-1;
    my $pjver='';
    my $pduid='';
    my $pkuid='';
    my $pvre=qr/^(\s*\"ProductVersion\"\s*=\s*\"8\:)(\d+\.\d+\.\d+)(\"\s*)$/;
    my $pkre=qr/^(\s*\"PackageCode\"\s*=\s*\"8\:\{)([0-9A-F\-]{36})(\}\"\s*)$/;
    my $pdre=qr/^(\s*\"ProductCode\"\s*=\s*\"8\:\{)([0-9A-F\-]{36})(\}\"\s*)$/;
    foreach my $i (0..$#pjdata)
    {
        my $line=$pjdata[$i];
        if( $line =~ /$pvre/ )
        {
            $npv=$i;
            $pjver=$2;
        }
        elsif( $line =~ /$pkre/ )
        {
            $npk=$i;
            $pkuid=$2;
        }
        elsif( $line =~ /$pdre/ )
        {
            $npd=$i;
            $pduid=$2;
        }
    }

    die "Missing data in MS project file $pjfile\n" if $npv < 0 || $npk < 0 || $npd < 0;

    if( ($pjver ne $version) && $update )
    {
        $pjver=$version;
        $pkuid=guid();
        $pduid=guid();
        $pjdata[$npv]=$1.$pjver.$3 if $pjdata[$npv]=~/$pvre/;
        $pjdata[$npd]=$1.$pduid.$3 if $pjdata[$npd]=~/$pdre/;
        $pjdata[$npk]=$1.$pkuid.$3 if $pjdata[$npk]=~/$pkre/;
    
        my $fh=new FileHandle($pjfile,"w");
        die "Cannot write to $pjfile\n" if ! $fh;
        binmode($fh);
        $fh->print(@pjdata);
        $fh->close();
        print "Updated $localname\n";
        
        my $uninst=$pjfile;
        $uninst=~ s/[^\\\/]*$/uninstall_snap.bat/;
        if( $uninst ne $pjfile )
        {
            $fh=new FileHandle($uninst,"w");
            die "Cannot write to $uninst\n" if ! $fh;
            binmode($fh);
            $fh->print("\@msiexec /uninstall \"{$pduid}\r\n");
            print "Updated uninstall_snap.bat\n";
        }
    }
    elsif( $pjver ne $version )
    {
        print "Project file $localname version $pjver out of sync with $version\n";
    }
}

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

my $versionfile='src/VERSION';
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

my @pjfiles;
find( sub { push(@pjfiles,$File::Find::name) if /\.vdproj$/i }, 'ms/projects');

foreach my $pjfile (@pjfiles)
{
    update_ms_vdproj($pjfile,$newversion,$update);
}

if( $update )
{
    system("git commit -a -m \"Updating version to $newversion\"")
        if system('git diff --quiet HEAD');
    system("git tag -a -f $newversion -m \"Version $newversion\"");
}

print $syntax if ! $update;
