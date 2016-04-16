@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
perl -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl
:WinNT
perl -x -S %0 %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#! /usr/bin/perl
#line 15
# Perl script for renaming station codes in contract station files

print "\nrecodestn: Edits station codes in contract files\n\n";


$#ARGV == 2 || die "Syntax: recodestn code_file input_data_file output_data_file\n";
open(CODES,$ARGV[0] ) || die "Cannot open station code file $ARGV[0]\n";
open(IN,$ARGV[1]) || die "Cannot open input contract station file $ARGV[1]\n";
open(OUT,">$ARGV[2]") || die "Cannot open output contract station file $ARGV[2]\n";

print "Loading code translations from $ARGV[0]\n";
$nc = 0;
while(<CODES>) {
   chomp;
   s/[\!\#].*//;
   my( $in, $out ) = split;
   next if $out eq '';
   $in =~ tr/a-z/A-Z/;
   $newcode{$in} = $out;
   $maxin  = length($in) if length($in) > $maxin;
   $maxout = length($out) if length($out) > $maxout;
   $nc++;
   }
close CODES;
print "$nc translations loaded\n";


print "\nTranslating codes in $ARGV[1]\n";
#Copy first line..
$_ = <IN>;
print OUT $_;

while(<IN>) {
   ($code,$data) = split(/\,/,$_,2);
   $code = $newcode{uc($code)} if $newcode{uc($code)};
   print OUT $code,",",$data;
   }

close(IN);
close(OUT);

print "\nTranslated data is in file $ARGV[2]\n";

__END__
:endofperl
