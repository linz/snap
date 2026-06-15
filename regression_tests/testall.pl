#!/usr/bin/perl
use strict;
use Cwd qw(cwd abs_path);
use File::Basename;

my $wd=dirname(abs_path($0));
chdir($wd);
my $param=join(' ',@ARGV);

my $fail=0;

# Run any per-suite generator scripts before globbing for test configs.
# Generators create derived suites (e.g. snapspec_k/) alongside the
# suite that owns the script, so the derived test.config files exist
# by the time the glob below picks them up.
foreach my $gen (glob('*/gen_test_configs.py'))
{
    system("python3 $gen");
}

foreach my $f (glob('*/test.config'))
{
    my $rc=system("perl ./runtests.pl -c $f $param");
    $fail=1 if $rc;
}
exit($fail);
