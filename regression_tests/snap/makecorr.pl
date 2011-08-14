while(<>){
push(@cvr,/([\-+\d\.]+)/g);
print $_;
}
my $n = 0;
while(@cvr)
{
my @row = splice(@cvr,0,$n+1);
$se[$n] = sqrt(pop(@row));
if( @row )
{
  push(@crl,join( " ", map { sprintf("%.8f",$row[$_]/($se[$_]*$se[$n])) } (0..$#row) )."\n");
}
$n++;
}

print join(" ",map { sprintf("%.8f",$_) } @se),"\n";
print @crl;
