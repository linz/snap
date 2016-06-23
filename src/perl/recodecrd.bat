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
# Perl script for renaming SNAP station codes in coordinate files

print "\nrecodecrd: Edits SNAP station codes in coordinate files\n\n";

$#ARGV == 2 || die "Syntax: recodecrd code_file input_data_file output_data_file\n";
open(CODES,$ARGV[0] ) || die "Cannot open station code file $ARGV[0]\n";
open(IN,$ARGV[1]) || die "Cannot open input coordinate file $ARGV[1]\n";
open(OUT,">$ARGV[2]") || die "Cannot open output coordinate file $ARGV[2]\n";

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
#Copy first three lines..
$_ = <IN>;
print OUT $_;
$_ = <IN>;
print OUT $_;
$_ = <IN>;
print OUT $_;

while(<IN>) {
   
   if( ! /^(\s*)(\S+)(.*)$/ ) {
      print OUT $_;
      next;
      }

   ($prefix,$code,$data) = ($1,$2,$3);
   $code = uc($code);

   if (!defined($newcode{$code}) || !$newcode{$code} ) {
      $nochange{$code} = 1 if $code !~ /^\!/;
      }
   else {
      $code = $newcode{$code};
   }
 
   print OUT $prefix,$code,$data,"\n"; 
   }   
   
   
close(IN);
close(OUT);   

print "\nTranslated data is in file $ARGV[2]\n";

@nochange = sort keys %nochange;   
if( @nochange > 1 ) { print "\nThe following codes are unchanged\n  ",
                            join("\n  ",@nochange),"\n";}

__END__
:endofperl
