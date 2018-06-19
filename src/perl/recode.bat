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
# Perl script for renaming SNAP station codes in command files
use File::Copy;

print "\nrecode: Edits SNAP station codes in data files\n\n";

$comment_col = 40;   # Column used for comments showing old codes, 0 for none
$just = -1;          # Use -1 for right justified codes - use 1 for left

$#ARGV >= 2 || die <<EOD;
Syntax: recode code_file input_data_file output_data_file

    or  recode code_file -b files.
  
    replaces files with .bak.  Files can include \@files to read filenames
    from another file.

EOD
my( $codefile, $option) = @ARGV;
my $backup = 0;
open(my $cf,$codefile ) || die "Cannot open station code file $codefile\n";
my @files = ();
if( $option eq '-b' )
{
    $backup = 1;
    foreach my $f (@ARGV[2 .. $#ARGV])
    {
        if( $f =~ /^\@/ )
        {
            my ($list) = ($');
            open(my $flist, $list ) || die "Cannot open filelist $list\n";
            while(my $fn=<$flist>)
            {
                $fn=~ s/^\s*//;
                $fn=~ s/\s*$//;
                push(@files,[$fn.'.bak',$fn]);
            }
        }
        else
        {
            push(@files,[$f.'.bak',$f]);
        }
    }
}
else
{
    push(@files,[$option,$ARGV[2]]);
}

print "Loading code translations from $cf\n";
$nc = 0;
while(<$cf>) {
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
close $cf;
print "$nc translations loaded\n";

foreach my $iof (@files)
{
    my ($inputf, $outputf ) = @$iof;

    if( $backup )
    {
        copy($outputf,$inputf) || die "Cannot create backup file $inputf\n";
    }

    print "\nTranslating codes in $inputf\n";
    #Copy first line..

    open(my $inf, "<$inputf" ) || die "Cannot open $inputf\n";
    open(my $outf, ">$outputf" ) || die "Cannot open $inputf\n";

    $_ = <$inf>;
    print $outf $_;
    
    while(<$inf>) {
       $input = $_;
       chop;
       
       # Remove the comment
       if( m/(\!.*)/ ) {
         $comment = $1;
         $_ = $`;
         }
       else {
         $comment = "";
         }
       
       # Remove leading blank characters
       if( m/(^\s+)/ ) {
          $prefix = $1;
          $_ = $';
          }
       else {
          $prefix = "";
          }
    
       # If finished with a blank line, then print the input and go on to next 
       if( !$_ ) { print $outf $input; next;}
       
       # #data statement - need to know if grouped and if includes inst heights 
       if(m/^\#data/i) {
          $grouped = 0;
          $heights = 1;
          $grouped = 1 if m/\bha\b/i || m/\bdr\n/i || m/\bgrouped\b/i;
          $heights = 0 if m/\bno_heights\b/i;
          @codes = (1);
          if( !$grouped ) {@codes = (1,3+2*$heights);}
          }
          
       # data specification commands are copied without further processing...   
       if( m/^\#/ ){ print $outf $input; next; }
       
       # Now handle data lines....
       
       @data = ($prefix,split(/(\s+)/,$_));
       $code_comments = "";
       for $i (@codes) {
          $oc = $data[$i];
          $oc =~ tr/a-z/A-Z/;
          if (!defined($newcode{$oc}) || !$newcode{$oc} ) {
             $nochange{$oc} = 1;
             next;
             }
          $nc = $newcode{$oc};
          $data[$i] = $nc;
          if( $just ) {
             $jf = $i+$just;
             $jl = length($data[$jf]) 
                   - ($maxin - length($oc)) 
                   + ($maxout - length($nc));
             $jl = 1 if $jf > 0 && $jl < 1;
             $jl = 0 if $jl < 0;
             $data[$jf] = " "x$jl;
             }
          if( $comment_col ) {
             $js = " "x($maxin-length($oc));
             if( $just == -1 ) {$oc = $js.$oc;} else { $oc = $oc.$js; }
             $code_comments .= " $oc";
             }
          }
       $output = join("",@data).$comment;
       if($code_comments) {
          $nc = $comment_col - length($output);   
          $output .= " "x$nc if $nc > 0;
          $output .= ' ! '.$code_comments;
          }
       print $outf $output,"\n"; 
       }   
       
       
    close($inf);
    close($outf);   
    print "\nTranslated data is in file $outputf\n";
};


@nochange = sort keys %nochange;   
if( @nochange > 1 ) { print "\nThe following codes are unchanged\n  ",
                            join("\n  ",@nochange),"\n";}

__END__
:endofperl
