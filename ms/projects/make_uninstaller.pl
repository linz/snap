@ARGV==2 || exit;
print "Building uninstaller $ARGV[1]\n";
open(IN,$ARGV[0]) || exit;
open(OUT,">$ARGV[1]") || exit;
my $ok = 0;
while(<IN>)
{
	if( /^\s+\"ProductCode\"\s*\=\s*\"\d+\:(\{[A-Z0-9-]{36}\})\"\s*$/ )
        {
		print "Found product code $1\n";
		print OUT "\@msiexec /uninstall \"$1\"\n";
		$ok = 1;
		last;
	}
}
print "Failed to determine product code\n" if ! $ok;
close(IN);
close(OUT);


