#!/usr/bin/perl

my $file='wxWidgets-2.8.12/src/msw/window.cpp';
my $newfile=$file.'.new';
my $bakfile=$file.'.bak';

open(my $of, $file) || die "Cannot open $file\n";
binmode($of);
open(my $nf, ">", $newfile) || die "Cannot open $newfile\n";
binmode($nf);

while( my $l=<$of> )
{
    if( $l =~ /if \!defined __WXWINCE__ \&\& \!defined NEED_PBT_H/ )
    {
        $l =~ s/\!defined NEED_PBT_H/defined NEED_PBT_H/;
    }
    print $nf $l;
}
close($of);
close($nf);
rename($file,$bakfile) || die "Cannot rename $file\n";
rename($newfile,$file) || die "Cannot rename $newfile\n";
