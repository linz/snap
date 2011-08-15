#!/bin/perl -p

BEGIN { undef $/ };

s/Version\s.*$/Version/m;
s/Version\sdate\:\s.*$/Version date:/m;
s/^\"RUNTIME\"\,[^\,]*/"RUNTIME",/m;
s/^\"SNAPVER\"\,[^\,]*/"SNAPVER",/m;

my $runtime = $1 if /Run\sat\s(.*)$/m;
$runtime = quotemeta($runtime);

s/$runtime$//gm;


