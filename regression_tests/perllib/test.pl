use lib '..';

use SNAP::CrdFile;

print SNAP::CrdFile::BasePath(),"\n";
my $garbage;
my $f = \$garbage;
bless $f, 'SNAP::CrdFile';
print $f->CoordSysList();

