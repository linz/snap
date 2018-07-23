#!/usr/bin/perl
use strict;

open(my $f, '<snaphelp.contents') || die "Cannot open snaphelp.contents\n";
open(my $wf, '>wxhelp.map') || die "Cannot open wxhelp.map\n";

my $item=0;
while( my $line=<$f> )
{
    chomp($line);
    my($id,$desc,$url)=split(/\t/,$line);
    $url=~s/\s//g;
    $url=~s/^help\///;
    print $wf "$item $url ; $desc\n";
    $item++;
}
close($f);
close($wf);
