#!/usr/bin/perl
use strict;
use Archive::Zip qw/ :ERROR_CODES /;
use FindBin;
use File::Find;

chdir($FindBin::Bin);

my $source = 'src/packages';
my $target = 'packages';
my @package = ();
my $package;


my $pos=length($source)+1;
foreach my $pkgd ( glob("$source/*") )
{
    next if ! -d $pkgd;
    next if ! -f "$pkgd/ABOUT";
    my $pkg = substr($pkgd,$pos);
    print "Building package $pkg\n";

    my @files=();
    find(sub { push(@files,$File::Find::name) if -f $_ } , $pkgd );

    open(my $df,"$pkgd/ABOUT");
    my $description=join('',<$df>);
    close($df);

    my $pkgfile="$target/$pkg.zip";
    
    my $zip=new Archive::Zip;

   $zip->zipfileComment("SNAP2: $description");
   print "Description: $description";
   foreach my $sourcefile (@files)
   {
      my $target = substr($sourcefile,$pos);
      $target =~ s/^\///;
      print "Adding $sourcefile as $target\n";
      $zip->addFile($sourcefile,$target);
   }
   my $status = $zip->writeToFileNamed($pkgfile);
   print "Cannot create package $pkgfile\n" if $status != AZ_OK;
}
