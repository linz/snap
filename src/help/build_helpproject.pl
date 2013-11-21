use strict;
use File::Find;
use File::Copy;
use File::Copy::Recursive qw(dircopy);
use File::Path qw(remove_tree);
use FindBin;
use HTML::LinkExtor;
use URI;


my $hhc = "C:/Program Files/HTML Help Workshop/hhc.exe";
$hhc = "C:/Program Files (x86)/HTML Help Workshop/hhc.exe" if ! -x $hhc;
die "Cannot find HTML Help Workshop help compiler\n" if ! -x $hhc;

my $helpname = '';
my @srcdir = ();
my $homedir = $FindBin::Bin;
my $tmpdir = '';
my $helpfile = '';
my $buildhelp = 0;
my @copyto = ();
my $skipre = '';

my $inarg = '';
foreach(@ARGV)
{
   if( $inarg eq '-x' ) { $skipre .= '|' . quotemeta($_); }
   elsif( $inarg eq '-h' ) { $homedir = $_; $inarg=''; }
   elsif( $inarg eq '-t' ) { $tmpdir = $_; $inarg=''; }
   elsif( $inarg eq '-c' ) { push(@copyto,$_); $inarg=''; }
   elsif( $_ =~ /^\-[cxht]$/) { $inarg = $_; }
   elsif( $_ eq '-b' ) { $buildhelp = 1; }
   elsif( ! $helpname ) { $helpname = $_; }
   else { push(@srcdir,$_); }
}

@srcdir  || die <<EOD;

Syntax: help_file_name base_directory(ies)

Options:
   -h homedir   Home directory.  Processing will be based
                on this directory
   -b           Build the help file
   -c target    Copy compiled file to target
   -x ...       Following args are excluded directories
   -t ...       Temporary folder in which to actually run help compiler
                (as doesn't like network drives)

EOD

print "Compiling help file $helpname\n";
print "Using source directories\n  ",join("\n  ",@srcdir),"\n";
print "Using temporary build directory $tmpdir\n" if $tmpdir;
print "Exclude pattern: $skipre\n";
print "Copying to\n  ",join("\n  ",@copyto),"\n";

chdir($homedir);

$skipre = substr($skipre,1) if $skipre ne '';

my $htmlfiles = {};
my $otherfiles = {};

find( sub{ 
        return if ! /\.html?$/i; 
        my $name = $File::Find::name; 
        return if $skipre && $name =~ /$skipre/;
        $htmlfiles->{$name}->{filename} = $name; 
        }, @srcdir );

my $keywordlist = {};
my $references = {};
my $external = {};


foreach my $htmlfile ( map {$htmlfiles->{$_}} sort keys %$htmlfiles )
{
   ProcessHtmlFile( $htmlfile );
}

&MergePluralKeywords();

&BuildKeywordFile( $helpname.".hhk" ); 

my $contentsfiles = &BuildContents( $helpname.".contents", $helpname.".hhc" );

my @filelist = sort keys %$htmlfiles;
push( @filelist, sort keys %$otherfiles );

unlink($helpname.'.hhp');
&WriteFilesIntoProject( $helpname.".projectbase", $helpname.".hhp", \@filelist );

die "Failed to create help file source $helpname.hhp\n" if ! -e $helpname.'.hhp';

if( $buildhelp )
{
    my $incopy=0;
    if( $tmpdir && $tmpdir ne $homedir )
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
    foreach my $copydir (@copyto)
    {
        if( ! -d $copydir  )
        {
            print "Invalid target $copydir for copy\n";
        }
        else
        {
            copy($chm,$copydir.'/'.$chm);
        }
    }
}

sub ProcessHtmlFile
{
    my( $file ) = @_;
    my $filename = $file->{filename};

    if( ! open(H,"<$filename") )
    {
       print "Cannot open HTML file $filename\n";
       return;
    }
    my $html = join('',<H>);
    close(H);

    my $title = $1 if $html =~ /\<title\>(.*?)\</si;
    $file->{title} = $title;
    print "Missing title in $filename\n" if ! $title;
    my $pageheader = $1 if $html=~ /\<h1.*?\>(.*?)<\/h1/si;
    $pageheader =~ s/\<.*?\>//g;

    if( lc($title) ne lc($pageheader) )
    {
        print "$filename: Title doesn't match h1 header\n    title: $title\n    h1:    $pageheader\n";
    }
    $file->{header} = $pageheader;
    &GetKeywords($file,$html);
    &GetLinks($file,$html);
}

sub GetLinks
{
    my($file,$html) = @_;
    my $p = new HTML::LinkExtor;
    $p->parse($html);

    my $rooturl = "http://base.nowhere/";
    my $filename = $file->{filename};
    my $base = new URI($rooturl.$filename);

    foreach my $links ( $p->links ) {
       my( $tag, %attr ) = @$links;
       foreach my $attr (keys %attr) 
       {
          my $ref = $attr{$attr};
          if( $ref eq '' ) {
              next;
              }
          my $uri = URI->new_abs($ref,$base);
          $uri->query('');
          $uri->fragment('');
          $ref = $uri->as_string;
          $ref =~ s/\?\#$//;

          if( substr($ref,0,length($rooturl)) eq $rooturl ) 
          {
              $ref = substr($ref,length($rooturl));
              $references->{$filename}->{$ref} = 1;
              $file->{references}->{$ref} = $ref; 
              $otherfiles->{$ref} = 1 if $ref !~ /\.html?$/i;
              if( ! -f $ref || ! -r $ref )
              {
                 print "Missing reference $filename -> $ref\n";
              }
          }
          else 
          {
              $file->{external}->{$ref} = $ref;
              $external->{$filename}->{$ref}++;
          }
       }
    }
}

sub GetKeywords
{
    my ($file, $html) = @_;
    my %kw;
    while( $html =~ /\<meta\s+name\=\"keywords\"\s+content\=\"(.*?)\"/sig )
    {
        my $kw = lc($1);
        foreach my $k ( split(/\,\s*/,$kw) )
        {
           $k =~ s/^\s+//;
           $k =~ s/\s+$//;
           $kw{$k} = 1;
        }
    }
    my $source = $file->{filename};
    my $title = $file->{title};
    print "No keywords in $source\n" if ! %kw;
    return if ! $title || ! %kw;
    foreach my $k (keys %kw)
    {
        push( @{$keywordlist->{$k}},{name=>$title, url=>$source} );
    }
}

sub BuildKeywordFile
{
    my( $file ) = @_;
    open( F,">$file");

    print F <<'EOD';
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<HTML>
<HEAD>
<meta name="GENERATOR" content="Microsoft&reg; HTML Help Workshop 4.1">
<!-- Sitemap 1.0 -->
</HEAD><BODY>
<UL>
EOD

    foreach my $k ( sort keys %$keywordlist )
    {

        my @refs = @{$keywordlist->{$k}};
        print F <<"EOD";
	<LI> <OBJECT type=\"text/sitemap\">
		<param name=\"Name\" value=\"$k\">
EOD
        foreach my $ref (sort {$a->{name} cmp $b->{name}} @refs) { print F <<"EOD"; }
		<param name=\"Name\" value=\"$ref->{name}\">
		<param name=\"Local\" value=\"$ref->{url}\">
EOD

         print F <<"EOD";
		</OBJECT>
EOD
    }
    print F <<'EOD';
</UL>
</BODY></HTML>
EOD
    close(F);
}

sub MergePluralKeywords
{
   foreach my $k( keys %$keywordlist ) {
      next if $k !~ /(.*)s$/;
      my $k2 = $1;
      next if ! exists $keywordlist->{$k2};
      print "Merging \"$k\" and \"$k2\"\n";
      push(@{$keywordlist->{$k2}},@{$keywordlist->{$k}});
      delete $keywordlist->{$k};
      }
}

sub WriteFilesIntoProject 
{
   my($projectbase,$projectfile,$filelist) = @_;
   
   open(HHP,"<$projectbase") || die "Cannot open $projectbase\n";
   
   my @project = ();
   
   my $inrec;
   while($inrec = <HHP>)
   {
      push(@project,$inrec);
      last if $inrec =~ /\[FILES\]/;
   }
   die "Invalid help project base file $projectbase\n" if $inrec !~ /\[FILES\]/;
   
   
   push( @project, map {"$_\n"} @$filelist );
   
   push(@project,"\n");
   
   while($inrec = <HHP>)
   {
      last if $inrec =~ /^\s*\[/;
   }
   
   
   while($inrec)
   {
      push(@project,$inrec);
      $inrec = <HHP>;
   }
   
   close(HHP);
   
   open(HHP,">$projectfile") || die "Cannot open $projectfile for writing\n";
   
   print HHP @project;
   
   close(HHP);
}


sub BuildContents 
{
   my ($contents_file,$hhc_file) = @_;

open(C,"<$contents_file") || die "Cannot open $contents_file\n";
open(H,">$hhc_file") || die "Cannot open $hhc_file\n";

my $dlevel = 0;

print H <<'EOD';
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<HTML>
<HEAD>
</HEAD><BODY>
<OBJECT type="text/site properties">
	<param name="ImageType" value="Folder">
</OBJECT>
EOD

my %contentsfiles = ();
while(<C>) {
   chomp;
   my( $level, $title, $url ) = split(/\t/);
   next if ! $level || $title eq '';
   while( $dlevel < $level ) { print H "\t"x$dlevel,"<UL>\n"; $dlevel++; }
   while( $dlevel > $level ) { $dlevel--; print H "\t"x$dlevel,"</UL>\n"; }
   my $prefix = "\t"x$dlevel;
   print H "$prefix<LI> <OBJECT type=\"text/sitemap\">\n";
   print H "$prefix\t<param name=\"Name\" value=\"$title\">\n";
   if( $url )
   {
	print H "$prefix\t<param name=\"Local\" value=\"$url\">\n";
        my $srcfile = $url;
        $contentsfiles{$srcfile} = $title;
   } 
   print H "$prefix\t</OBJECT>\n"
   }

while( $dlevel-- ) { print H "\t"x$dlevel,"</UL>\n"; }

print H <<'EOD';
</BODY></HTML>
EOD

foreach my $f (sort keys %contentsfiles )
{
   if( ! exists $htmlfiles->{$f} )
   {
      print "Invalid contents file $f\n";
   }
   elsif( lc(&ContentsTitle($htmlfiles->{$f})) ne lc($contentsfiles{$f}) )
   {
      print "Contents file description doesn't match title for $f\n";
      print "    title:    ",$htmlfiles->{$f}->{title},"\n";
      print "    contents: ",$contentsfiles{$f},"\n";
   }
}

foreach my $f (sort keys %$htmlfiles )
{
   print "File $f not in  contents\n" if ! exists $contentsfiles{$f};
}

close(H);
close(C);

}

sub ContentsTitle
{
    my( $h ) = @_;
    my $title = $h->{title};
    my $filename = $h->{filename};
    
    if( $filename =~ /files\/cmdcfg/ && $title =~ /^The\s+(.*)\s+command$/i )
    {
         $title = $1;
    }
    return $title;
}

