use strict;
use File::Find;
use File::Copy;
use File::Copy::Recursive qw(dircopy);
use File::Path qw(remove_tree);
use FindBin;

my $homedir = $FindBin::Bin;
chdir($homedir);

my $helpname = '';
my @srcdir = ();
my $tmpdir = '';

my $inarg = '';
foreach(@ARGV)
{
   if( $inarg eq '-t' ) { $tmpdir = $_; $inarg=''; }
   elsif( $_ =~ /^\-t$/) { $inarg = $_; }
   elsif( ! $helpname ) { $helpname = $_; }
   else { die "Invalid argument $_\n"; }
}

$helpname  || die <<EOD;

Syntax: help_file_name 

Options:
   -t ...       Temporary folder in which to actually run help compiler
                (as doesn't like network drives)

EOD

my $hhc = "C:/Program Files/HTML Help Workshop/hhc.exe";
$hhc = "C:/Program Files (x86)/HTML Help Workshop/hhc.exe" if ! -x $hhc;
die "Cannot find HTML Help Workshop help compiler\n" if ! -x $hhc;

print "Compiling help file $helpname\n";
print "Using temporary build directory $tmpdir\n" if $tmpdir;

die "Source $helpname.hhp is missing\n" if ! -e $helpname.'.hhp';
die "Source $helpname.hhc is missing\n" if ! -e $helpname.'.hhc';
die "Source $helpname.hhk is missing\n" if ! -e $helpname.'.hhk';

if( $tmpdir )
{
    $tmpdir .= "/buildhelp_$$";
    print "Building in temporary area $tmpdir\n";
    remove_tree($tmpdir);
    dircopy($homedir,$tmpdir);
    chdir($tmpdir);
}
my $chm = $helpname.'.chm';
die "Help compiler $hhc not installed\n" if ! -x $hhc;
system($hhc,$helpname.'.hhp');
die "Failed to compile help file $chm\n" if ! -e $chm;
if( $tmpdir )
{
    print "Copying $chm to $homedir/$chm\n";
    copy($chm,"$homedir/$chm") || die "Failed to copy chm file\n";
    chdir($homedir);
    remove_tree($tmpdir);
}
