while(<>)
{
   next if ! /^E/;
   my @f = split(/\|/);
   my $en = $f[5];
   $_ = <>;
   last if ! $_;
   next if ! /^D/;
   my @f = split(/\|/);
   my $rf = $f[4];
   my $en2 = $f[5];
   $en =~ s/m[EN]//g;
   $en =~ s/\s+/ /g;
   $en =~ s/^\s//g;
   $en =~ s/\s$//g;

   $en2 =~ s/m[EN]//g;
   $en2 =~ s/\s+/ /g;
   $en2  =~ s/^\s//g;
   $en2  =~ s/\s$//g;

   print "$rf $en $en2\n" if $en ne $en2;
}
