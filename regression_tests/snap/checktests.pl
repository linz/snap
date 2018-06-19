#!/usr/bin/perl 
use strict;
use Getopt::Std;
use File::Find;

my $syntax=<<EOD;

Syntax: check_tests -b -e [-v|-q] output_dir check_dir [filename ..]

  ouput_dir   Directory containing test output
  check_dir   Directory containing check output
  filename..  Possibly wildcarded filenames to test (default all files)

  -b          Ignore blank lines
  -e          Ignore extra output files
  -v          More output
  -c cfgfile  Read configuration from config file
  -f          Print configuration in output
  -q          No output

EOD

my %opts=@_;
getopts('hqvbfec:',\%opts);
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
        next if $f !~ /^\s*FIX\s+([^\w\s])/i;
        my($delim,$data)=($1,$');
        my ($re,$replace,$filere)=split($delim,$data);
        $re =~ s/\s*$//;
        $re =~ s/\{datetime\}/[0-3]?\\d\\-(JAN|FEB|MAR|APR|MAY|JUN|JUL|AUG|SEP|OCT|NOV|DEC)\\-2\\d\\d\\d\\s+[0-2]?\\d\\:\\d\\d\\:\\d\\d/g;
        $replace=~ s/\s*$//;
        $filere=~ s/\s*$//;
        $re=qr/$re/;
        $filere=qr/$filere/ if $filere;
        push(@replacements,[$filere,$re,$replace]);
        $re=qr/$re/;
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


my $quiet=$opts{q};
my $verbose=$opts{v} && ! $quiet;
my $ignoreblanks=$opts{b};
my $ignoreextra=$opts{e};
my $cfgfile=$opts{c};
my $printcfg=$opts{f};
#@ARGV >= 2 || die $syntax;

my $outdir=(shift @ARGV) || 'out';
die "Invalid output directory $outdir\n" if ! -d $outdir;
my $chkdir=(shift @ARGV) || 'check';
die "Invalid check directory $chkdir\n" if ! -d $chkdir;

my @cfglines=<DATA>;
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
    if( $l =~ /^\s*(absolute_|relative_)tolerance\s+(\S+)\s*$/i )
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
    if( $l =~ /^\s*line_(absolute_|relative_)tolerance\s+(\S+)\s*(\S+)\s*(\S+)\s*$/i )
    {
        my ($type,$tol,$filere,$linere)=($1,$2,$3,$4);
        my $type=substr(uc($type),0,1);
        my $tol=$tol+0.0;
        die "Invalid tolerance $tol in $l\n" if $tol <= 0.0;
        my $filere = qr/$filere/;
        my $linere = qr/$linere/;
        push(@$linetol,[$filere,$linere,$type,$tol]);
    }
};

my $cfg={
    fixes=>$fixes,
    reltol=>$reltol,
    abstol=>$abstol,
    linetol=>$linetol,
    ignoreblanks=>$ignoreblanks,
};

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
    if( $of eq '' || $of gt $cf )
    {
        push(@outmissing,$cf) if ! $ignoreextra;
        $cf=shift(@$chkfiles);
        next;
    }
    if( $cf eq '' || $cf gt $of )
    {
        push(@chkmissing,$of);
        $of=shift(@$outfiles);
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
            print "    $f\n";
        }
    }
    if( @chkmissing )
    {
        print "Not in check directory:\n";
        for my $f (@chkmissing)
        {
            print "    $f\n";
        }
    }
    if( @unmatched )
    {
        print "Files differ:\n";
        for my $diff (@unmatched)
        {
            my ($f,$msg)=@$diff;
            print "    $f: $msg\n";
        }
    }
}
if( ! $quiet )
{
    printf "Passed %d tests\n",scalar(@matched);
    printf "Failed %d tests\n",$nfail;
}

exit($status);


__DATA__

# Snap listing files
FIX ~^(\s*PROGRAM\s+SNAP\s+Version)(?:\s+\S+\s*)?$    ~$1 0.0.0      ~\.(lst|err)$
FIX ~^\s*(Version\sdate\:).*?$                        ~$1 1 Jan 2000 ~\.(lst|err)$
FIX ~^\s*(Run\s+at)(?:\s+{datetime}(?:\s+by\s+\S+)?)?\s*$  ~$1 00:00:00 by user ~\.(lst|err)$
FIX ~^(.*?)\s+{datetime}\s*$                               ~$1 1 Jan 2000      ~\.(lst|err)$
# File name path delimiter
FIX ~\\                                                    ~\/         ~\.(lst|err)$

# Snap CSV files
FIX ~^(\"RUNTIME\"\,).*                       ~$1,00:00:00   ~metadata\.csv$
FIX ~^(\"SNAPVER\"\,).*                       ~$1,00:00:00   ~metadata\.csv$

# Snap coordinate files
FIX ~^(! Updated by SNAP).*                   ~$1 ...        ~\.(newcrd|crd)$

# Sinex files
FIX ~SNP\s\d\d\:\d\d\d\:\d\d\d\d\d\sSNP       ~SNP 00:001:00000 SNP  ~\.snx$
FIX ~^\s*SOFTWARE\s+SNAP.*                    ~ SOFTWARE SNAP          ~\.snx$

# Tolerance (multiple of value, absolute difference)
ABSOLUTE_TOLERANCE 1.0e-8
RELATIVE_TOLERANCE 1.0e-10

LINE_ABSOLUTE_TOLERANCE 0.0001 testgx.*\.lst \"b\"
