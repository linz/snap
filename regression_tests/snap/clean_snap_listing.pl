#!/bin/perl -p

BEGIN { our $runtime };

s/(PROGRAM\s+SNAP\s+Version)\s.*\S/$1/;
s/Version\sdate\:\s.*$/Version date:\n/;
s/^\"RUNTIME\"\,[^\,]*/"RUNTIME",/;
s/^\"SNAPVER\"\,[^\,]*/"SNAPVER",/;
# For SINEX
s/SNP\s\d\d\:\d\d\d\:\d\d\d\d\d\sSNP/SNP 00:001:00000 SNP/;
s/SNAP\s+version\s+\d+\.\d\.\d+\s+date\s+.*\S/SNAP/;
# Numeric formats .. exponent digits
s/(\d{7}e[+-])0(\d\d)/$1$2/g;
# -0.0000
s/\-(0\.0+)\b/ $1/g;
s/(\s|\[)(\-?\d+\.\d+)e\-(\d\d)/$3 > 12 ? $1."0.00" : $1.sprintf("%.*f",($3 > 5 ? 2 : 7-$3),$2).'e-'.$3/eg if /^\s*\"values?\"\:\s/;
s/(\s|\[)(\-?0\.0+)e(\+|\-)(\d\d)/$1."0.00"/eg if /^\s*\"values?\"\:\s/;

if(/^\s*Run\s+at\s+(.*?)(?:\s+by\s+(.*)?)?\s*$/)
{
    $runtime = quotemeta($1);
    $user = quotemeta($2);
    s/\s+by\s+.*//;
}
s/$runtime$//g if $runtime;


