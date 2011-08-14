my @files = qw/snp crd dat cfg/;
my ($from,$to) = @ARGV;
die "Need input and output job numbers\n" if ! $from || ! $to;
foreach $f (map {'snapspec'.$from.'.'.$_} @files) { die "Cannot find $f\n" if ! -e $f };
foreach $f (map {'snapspec'.$to.'.'.$_} @files) { die "$f already exists\n" if -e $f };

my $rex = 'snapspec'.quotemeta($from);
my $rep = 'snapspec'.$to;
foreach my $e (@files)
{
   open(I,'<snapspec'.$from.'.'.$e);
   open(O,'>snapspec'.$to.'.'.$e);
   while(<I>){ s/$rex/$rep/g; print O $_; }
   close(I);
   close(O);
}
