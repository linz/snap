#!/bin/perl -p

BEGIN { undef $/ };

s/Version\s.*$/Version/m;
s/Version\sdate\:\s.*$/Version date:/m;
s/^\"RUNTIME\"\,[^\,]*/"RUNTIME",/m;
s/^\"SNAPVER\"\,[^\,]*/"SNAPVER",/m;
s/(\d{7}e[+-])0(\d\d)/$1$2/g;

my $runtime = $1 if /Run\sat\s(.*)$/m;
$runtime = quotemeta($runtime);

s/$runtime$//gm;


