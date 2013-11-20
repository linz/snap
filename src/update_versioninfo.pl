use strict;
use FindBin;
use File::Find;
use FileHandle;
use Getopt::Std;
use POSIX;

my %opts;
getopts("iIl",\%opts);
my $update = $opts{i} || $opts{I};
my $major = $opts{I};
my $dolog = $opts{l};

my $nzmapconv = $ARGV[0] eq 'nzmapconv';

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
    next if $nzmapconv && $file !~ /\/nzmapconv\//i;
    next if ! $nzmapconv && $file =~ /\/nzmapconv\//i;

    my $fh = new FileHandle("<$file");
    my $data = join('',$fh->getlines);
    $fh->close;
    $data =~ /^\s*\#define\s+VERSION\s+\"(\d+\.\d+\.\d+)\"/m;
    my $version = $1;
    print "File $localname version $version\n";

    if( $update )
    {
        $version =~ /(\d+)\.(\d+)\.(\d+)/;
        my ($v1, $v2, $v3) = ($1, $2, $3);
        if( $major ) { $v2++; $v3=0; } else { $v3++; }
        $version="$v1.$v2.$v3";
        print "Updating to $version\n";
        $data =~ s/^(\s*\#define\s+VERSION\s+\")(\d+\.\d+\.)(\d+)(")/$1.$version.$3/em;
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

