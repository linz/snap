#!/bin/perl -p

BEGIN { undef $/ };

s/Version\s.*$/Version/m;
s/Version\sdate\:\s.*$/Version date:/m;
s/^\"RUNTIME\"\,[^\,]*/"RUNTIME",/m;
s/^\"SNAPVER\"\,[^\,]*/"SNAPVER",/m;
s/(\d{7}e[+-])0(\d\d)/$1$2/g;
# For SINEX
s/SNP\s\d\d\:\d\d\d\:\d\d\d\d\d\sSNP/SNP 00:001:00000 SNP/;
s/SNAP\s+version\s+\d+\.\d\.\d+\s+date\s+.*$/SNAP/m,;

my $runtime = $1 if /Run\sat\s(.*)$/m;
$runtime = quotemeta($runtime);

s/$runtime$//gm;


