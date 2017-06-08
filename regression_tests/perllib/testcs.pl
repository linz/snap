use lib '../../src/perl/perllib';
use LINZ::Geodetic::CoordSysList;

my $csdef='../../src/coordsys/coordsys.def';
my $cslist=LINZ::Geodetic::CoordSysList->newFromCoordSysDef($csdef);
foreach my $code ($cslist->coordsys())
{
    print "Coordinate system: $code\n";
    eval
    {
        my $cs=$cslist->coordsys($code);
    };
    if( $@ )
    {
        print "   ... failed to load: $@\n";
    }
}

