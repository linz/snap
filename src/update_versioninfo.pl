use strict;
use FindBin;
use File::Find;
use FileHandle;
use Getopt::Std;
use POSIX;

my %opts;
getopts("il",\%opts);
my $update = $opts{i};
my $dolog = $opts{l};

my $nzmapconv = $ARGV[0] eq 'nzmapconv';

my @files;
find( sub { push(@files,$File::Find::name) if /^versioninfo.c(?:pp)?$/i }, $FindBin::Bin);

my $buildtime = strftime("%Y-%m-%d %H:%M",localtime);


my $log = new FileHandle("$FindBin::Bin/versions.log","a");
$log->print("\nBuild: $buildtime\n") if $dolog;

foreach my $file (@files)
{
	next if $nzmapconv && $file !~ /\/nzmapconv\//i;
	next if ! $nzmapconv && $file =~ /\/nzmapconv\//i;

	print "Updating version in $file\n";
	my $fh = new FileHandle("<$file");
	my $data = join('',$fh->getlines);
	$fh->close;
	$data =~ s/^(\s*\#define\s+VERSION\s+\")(\d+\.\d+\.)(\d+)(")/$1.$2.($3+$update).$4/em;
	my $version = $2.($3+$update);

	if( $update )
	{
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
$log->close;

