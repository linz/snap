#!/usr/bin/perl
use strict;
use Cwd qw(cwd abs_path);
use File::Basename;

my $wd=dirname(abs_path($0));
chdir($wd);
my $param=join(' ',@ARGV);

my $fail=0;
foreach my $f (glob('*/test.config'))
{
    my $rc=system("perl ./runtests.pl -c $f $param");
    $fail=1 if $rc;
}
exit($fail);
