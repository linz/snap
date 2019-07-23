#!/usr/bin/perl 
use strict;
use Getopt::Std;
use File::Find;
use File::Basename;
use Cwd qw(cwd abs_path);

my $syntax=<<EOD;

Syntax: check_tests -b -e [-v|-q] output_dir check_dir [filename ..]

  ouput_dir   Directory containing test output
  check_dir   Directory containing check output
  filename..  Possibly wildcarded filenames to test (default all files)

  -b          Ignore blank lines
  -e          Ignore extra output files
  -c cfgfile  Read configuration from config file (default test.config)
  -f          Print configuration in output
  -v          More output
  -q          No output
  -d file     Apply regex fixes to file and write to output (for debugging only)

EOD

my %opts=@_;
getopts('hqvbfec:d:',\%opts);
die $syntax if $opts{h};

sub filelist
{
    my ($dir,$patterns)=@_;
    my $len=length($dir)+1;
    my $pfx=$dir.'/';
    my @files=();
    find(sub {
            next if -d $_; 
            my $f=$File::Find::name;
            my $found=0;
            foreach my $p (@$patterns)
            {
                if( $_ =~ /$p/ ){ $found=1; last; }
            }
            if( ! $found )
            {
                next;
            }
            if( substr($f,0,$len) eq $pfx )
            {
                $f=substr($f,$len);
                push(@files,$f);
            }
        },
        $dir
        );
    @files=sort(@files);
    return \@files;
}

sub load_fixes
{
    my ($fixes)=@_;
    my @replacements=();
    foreach my $f (@$fixes)
    {
        next if $f !~ /^\s*match_replace_re\:\s+([^\w\s])/i;
        my($delim,$data)=($1,$');
        my ($filere,$re,$replace)=split($delim,$data);
        $re =~ s/\s*$//;
        $re =~ s/\{datetime\}/[0-3]?\\d\\-(JAN|FEB|MAR|APR|MAY|JUN|JUL|AUG|SEP|OCT|NOV|DEC)\\-2\\d\\d\\d\\s+[0-2]?\\d\\:\\d\\d\\:\\d\\d/g;
        $replace=~ s/\s*$//;
        $filere=~ s/\s*$//;
        $re=qr($re);
        $filere=qr($filere) if $filere;
        push(@replacements,[$filere,$re,$replace]);
        $re=qr($re);
    }
    return \@replacements;
}

sub apply_fixes
{
    my($filename,$line,$fixes)=@_;
    foreach my $f (@$fixes)
    {
        my($filere,$re,$replace)=@$f;
        next if $filere && $filename !~ /$filere/;
        next if $line !~ $re;
        eval "\$line=~s/\$re/$replace/g";
        #last;
    }
    return $line;
}

sub split_floats
{
    my ($line)=@_;
    my $floatre=qr/\-?(?:\d+\.|\.\d)\d*(?:e[\-\+]?\d+)?/i;
    my @floats=$line =~ /($floatre)/g;
    return $line if ! @floats;
    $line=~ s/$floatre/ /g;
    $line=~ s/\s+/ /g;
    $line=~ s/^\s+//;
    $line=~ s/\s+$//;
    return $line,@floats;
}

sub compareline
{
    my ($f,$oline,$cline,$cfg)=@_;
    return '' if $oline eq $cline;
    my ($otext,@ovalues)=split_floats($oline);
    my ($ctext,@cvalues)=split_floats($cline);
    # Special case for 180 degree offsets in calculating
    # error ellipses...
    if( $otext ne $ctext )
    {
        $otext =~ s/\b180\b/0/;
        $ctext =~ s/\b180\b/0/;
    }
    if( $otext eq $ctext && scalar(@ovalues) eq scalar(@cvalues))
    {
        my $rtol=$cfg->{reltol};
        my $atol=$cfg->{abstol};
        foreach my $ltol (@{$cfg->{linetol}})
        {
            my ($filere,$linere,$type,$tol)=@$ltol;
            next if $f !~ $filere;
            next if $oline !~ $linere;
            $atol=$tol if $type eq 'A' and $tol > $atol;
            $rtol=$tol if $type eq 'R' and $tol > $rtol;
        }
        foreach my $i (0..$#ovalues)
        {
            my $o=$ovalues[$i];
            my $c=$cvalues[$i];
            my $diff=abs($o-$c);
            next if $diff <= $atol;
            next if $diff <= abs($c)*$rtol;
            # Special case for 180 degree offsets
            next if abs($o) < 370 && abs($c) < 370 && abs($diff-180) < 0.00001;
            # Check for difference of 1 in last decimal place
            my $oa=$o;
            $oa=~s/^\-?\d+\.(\d*)\d/"0."+("0" x length($1))/e;
            my $oc=$c;
            $oc=~s/^\-?\d+\.(\d*)\d/"0.".("0" x length($1))."1"/e;
            my $prec=$oa > $oc ? $oa : $oc;
            #print("DP check $o $c $prec\n");
            next if $diff < $prec*1.1;
            return "Values differ: $o $c";
        }
        return "";
    }
    return "Differ";
}

sub compare
{
    my($outdir,$chkdir,$f,$cfg)=@_;
    $cfg ||= {};
    my $ignoreblanks=$cfg->{ignoreblanks};

    open(my $of,"$outdir/$f") || die "Cannot open $outdir/$f\n";
    my @olines=<$of>;
    close($of);

    open(my $cf,"$chkdir/$f") || die "Cannot open $outdir/$f\n";
    my @clines=<$cf>;
    close($cf);

    my $no=scalar(@olines);
    my $nc=scalar(@clines);
    my $io=0;
    my $ic=0;
    while( $io < $no || $ic < $nc )
    {
        my $o=$olines[$io];
        my $c=$clines[$ic];
        $o =~ s/\s*$//;
        $c =~ s/\s*$//;
        if( $o eq $c )
        {
            $io++;
            $ic++;
            next;
        }
        if( $ignoreblanks && $io < $no && $o !~ /\S/ ){ $io++; next; }
        if( $ignoreblanks && $ic < $nc && $c !~ /\S/ ){ $ic++; next; }

        $o=apply_fixes($f,$o,$cfg->{fixes});
        $c=apply_fixes($f,$c,$cfg->{fixes});
        if( $o eq $c )
        {
            $io++;
            $ic++;
            next;
        }
        if( $ignoreblanks && $io < $no && $o !~ /\S/ ){ $io++; next; }
        if( $ignoreblanks && $ic < $nc && $c !~ /\S/ ){ $ic++; next; }

        my $msg = compareline($f,$o,$c,$cfg);
        if( $msg eq '')
        {
            $io++;
            $ic++;
            next;
        }
        my $lineno=$io+1;
        $lineno .= ':'.($ic+1) if $ic != $io;
        return sprintf("line %s: %s\n        %s\n        %s",$lineno,$msg,$o,$c);
    }
    return "";
}

sub debugfile
{
    my($f,$cfg)=@_;
    open(my $of,"$f") || die "Cannot open $f\n";
    my @olines=<$of>;
    close($of);
    foreach my $o (@olines)
    {
        $o =~ s/\s*$//;
        $o = apply_fixes($f,$o,$cfg->{fixes});
        print "$o\n";
    }
}


my $quiet=$opts{q};
my $verbose=$opts{v} && ! $quiet;
my $ignoreblanks=$opts{b};
my $ignoreextra=$opts{e};
my $cfgfile=$opts{c};
my $printcfg=$opts{f};
my $debugfile=$opts{d};
#@ARGV >= 2 || die $syntax;

my $outdir=(shift @ARGV) || 'out';
die "Invalid output directory $outdir\n" if ! -d $outdir;
my $chkdir=(shift @ARGV) || 'check';
die "Invalid check directory $chkdir\n" if ! -d $chkdir;

my @cfglines=();
$cfgfile='test.config' if ! $cfgfile && -e 'test.config';

if( $cfgfile )
{
    open(my $cfgf,$cfgfile) || die "Cannot open configuration file $cfgfile\n";
    push(@cfglines,<$cfgf>);
    close($cfgf);
}


my $fixes=load_fixes(\@cfglines);
my $reltol=1.0e-10;
my $abstol=1.0e-8;
my $linetol=[];
foreach my $l (@cfglines)
{
    if( $l =~ /^\s*match_(absolute_|relative_)tolerance\:\s+(\S+)\s*$/i )
    {
        my ($type,$tol)=($1,$2);
        $type=substr(uc($type),0,1);
        $tol += 0.0;
        die "Invalid tolerance $tol in $l\n" if $tol <= 0.0;
        if( $type =~ /^A/ )
        {
            $abstol=$tol;
        }
        else
        {
            $reltol=$tol;
        }
    }
    if( $l =~ /^\s*match_line_(absolute_|relative_)tolerance\:\s+(\S+)\s*(\S+)\s*(\S+)\s*$/i )
    {
        my ($type,$tol,$filere,$linere)=($1,$2,$3,$4);
        my $type=substr(uc($type),0,1);
        my $tol=$tol+0.0;
        die "Invalid tolerance $tol in $l\n" if $tol <= 0.0;
        my $filere = qr/$filere/;
        my $linere = qr/$linere/;
        push(@$linetol,[$filere,$linere,$type,$tol]);
    }
    if( $l =~ /^\s*match_ignore_blanks\:\s*(y|yes|t|true)\s*$/i )
    {
        $ignoreblanks=1;
    }
};

my $cfg={
    fixes=>$fixes,
    reltol=>$reltol,
    abstol=>$abstol,
    linetol=>$linetol,
    ignoreblanks=>$ignoreblanks,
};

my $headed=0;
my $scriptpath=abs_path($cfgfile);
$scriptpath=basename(dirname($scriptpath))."/".basename($scriptpath);
if( $printcfg || $verbose )
{
    $headed=1;
    print "Checking tests in $scriptpath\n";
}

if( $printcfg )
{
    print "Comparing $outdir with $chkdir\n";
    print "Absolute numeric tolerance: $abstol\n";
    print "Relative numeric tolerance: $reltol\n";

    foreach my $lt (@$linetol)
    {
        my($filere,$linere,$type,$tol)=@$lt;
        print "Specific ",$type eq 'A' ? 'absolute' : 'relative',
            " tolerance for file ",$filere," line ",$linere," ",$tol,"\n";
    }

    foreach my $f (@$fixes)
    {
        print "Fix:\n";
        printf "  filere:      %s\n",$f->[0];
        printf "  linere:      %s\n",$f->[1];
        printf "  replacement: %s\n",$f->[2];
    }
};

my @patterns=();
foreach my $fn (@ARGV)
{
    $fn = quotemeta($fn);
    $fn =~ s/\\\*/.*/g;
    $fn =~ s/\\\?/./g;
    $fn = '^'.$fn.'$';
    push(@patterns,qr/$fn/i)
}
push(@patterns,qr/./) if ! @patterns;

if( $debugfile )
{
    debugfile($debugfile,$cfg);
    exit();
}

my $outfiles=filelist($outdir,\@patterns);
my $chkfiles=filelist($chkdir,\@patterns);

my @chkmissing=();
my @outmissing=();
my @matched=();
my @unmatched=();

my $of=shift(@$outfiles);
my $cf=shift(@$chkfiles);

while( $of ne '' || $cf ne '' )
{
    if( $of ne $cf )
    {
        my $skipo=$cf eq '' ? 1 :
                  $of eq '' ? 0 :
                  $of lt $cf ? 1 : 0;
        if( $skipo )
        {
            push(@chkmissing,$of);
            $of=shift(@$outfiles);
        }
        else
        {
            push(@outmissing,$cf) if ! $ignoreextra;
            $cf=shift(@$chkfiles);
        }
        next;
    }
    my $diff=compare($outdir,$chkdir,$of,$cfg);
    if( $diff ne "" )
    {
        push(@unmatched,[$of,$diff]);
    }
    else
    {
        push(@matched,$of);
    }
    $of=shift(@$outfiles);
    $cf=shift(@$chkfiles);
}

my $status=(@outmissing || @unmatched) ? 3 : 
            @chkmissing ? 2 : 
            @matched ? 0 : 1;

my $nfail=scalar(@outmissing)+scalar(@unmatched)+scalar(@chkmissing);

if( $verbose )
{
    if( @outmissing )
    {
        print "Not in output directory:\n";
        for my $f (@outmissing)
        {
            print "    $chkdir/$f\n";
        }
    }
    if( @chkmissing )
    {
        print "Not in check directory:\n";
        for my $f (@chkmissing)
        {
            print "    $outdir/$f\n";
        }
    }
    if( @unmatched )
    {
        print "Files differ:\n";
        for my $diff (@unmatched)
        {
            my ($f,$msg)=@$diff;
            print "    $outdir/$f: $msg\n";
        }
    }
}
if( ! $quiet && ($verbose || $nfail))
{
    print "Checking tests in $scriptpath\n" if ! $headed;
    printf "Passed %d tests\n",scalar(@matched);
    printf "Failed %d tests\n",$nfail;
}

exit($status);


__END__

# Example test configuration
# 
# Snap listing files
match_replace_re: ~\.(lst|err)$  ~^(\s*PROGRAM\s+SNAP\s+Version)(?:\s+\S+\s*(?:\-\w+)?)?$    ~$1 0.0.0
match_replace_re: ~\.(lst|err)$  ~^\s*(Version\sdate\:).*?$                        ~$1 1 Jan 2000
match_replace_re: ~\.(lst|err)$  ~^\s*(Run\s+at)(?:\s+{datetime}(?:\s+by\s+\S+)?)?\s*$  ~$1 00:00:00 by user
match_replace_re: ~\.(lst|err)$  ~^(.*?)\s+{datetime}\s*$                               ~$1 1 Jan 2000
# File name path delimiter
match_replace_re: ~\.(lst|err)$  ~\\  ~\/

# Snap CSV files
match_replace_re: ~metadata\.csv$  ~^(\"RUNTIME\"\,).*   ~$1,00:00:00
match_replace_re: ~metadata\.csv$  ~^(\"SNAPVER\"\,).*   ~$1,0.0.0

# Snap coordinate files
match_replace_re: ~\.(newcrd|crd)   ~^(! Updated by SNAP).*   ~$1 ...

# Sinex files
match_replace_re: ~\.snx$   ~SNP\s\d\d\:\d\d\d\:\d\d\d\d\d\sSNP   ~SNP 00:001:00000 SNP
match_replace_re: ~\.snx$   ~^\s*SOFTWARE\s+SNAP.*                ~ SOFTWARE SNAP

# Tolerance (multiple of value, absolute difference)
match_absolute_tolerance: 1.0e-8
match_relative_tolerance: 1.0e-10

# Specific tolerances (match_line_absolute_tolerance/match_line_relative_tolerance)
# command tolerance filere linere

match_line_absolute_tolerance: 0.0001 testgx.*\.lst \"b\"
