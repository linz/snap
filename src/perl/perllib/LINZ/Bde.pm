################################################################################
#
# $Id: Bde.pm 1481 2011-11-16 00:06:30Z ccrook $
#
# linz_bde_loader -  LINZ BDE loader for PostgreSQL
#
# Copyright 2011 Crown copyright (c)
# Land Information New Zealand and the New Zealand Government.
# All rights reserved
#
# This program is released under the terms of the new BSD license. See the 
# LICENSE file for more information.
#
################################################################################
use strict;

package LINZ::BdeFile;
use Encode;
use IO::Zlib;
use FileHandle;

use fields qw/path name fh table nflds flds override_flds output_fields output_fldids shapefld srid opened closed encoding data fixlon start_time end_time archive_files/;

# Class variables

our $bdedir='.';
our $bdecopy = '';
our $lonoffset = 160.0;
our $srid = 4167; 
our $encoding = 'cp-1252';
our $override_columns = 
{
   ct01 => 'type=string count=integer',
   ct02 => 'type=string count=integer',
};

sub set_bde_path {
  $bdedir = $_[0];
}

sub set_srid_longitude_offset {
  $srid = $_[0];
  $lonoffset = $_[0];
}

sub set_source_encoding {
  $encoding = $_[0];
  }

#############

sub open
{
  my( $class, $fname) = @_;

  my $file = '';
  foreach my $fpat ( qw{ %f %f.crs %f.crs.gz %d/%f %d/%f.crs %d/%f.crs.gz} )
  {
     $file = $fpat;
     $file =~ s/\%f/$fname/;
     $file =~ s/\%d/$bdedir/;
     last if -r $file;
     $file = '';
  }

  die "Invalid BDE file $fname\n" if ! $file;

  my $self = fields::new($class);
  my $name = lc($fname);

  $name =~ s/.*[\\\/]//;
  $name =~ s/\..*//;
  $self->{name} = $name;
  $self->{path} = $file;
  $self->{fh} = undef;
  $self->{closed} = 0;
  $self->{opened} = 0;
  $self->{encoding} = $encoding;
  $self->{table} = undef;
  $self->{start_time} = undef;
  $self->{end_time} = undef;
  $self->{override_flds} = $override_columns->{$name};
  $self->{archive_files} = undef;

  return $self;
}

sub _open {
  my( $self) = @_;
  return $self if $self->{opened};

  my $path = $self->{path};

  my $fh = ($path =~ /\.gz/) ? IO::Zlib->new($path,"rb") : new FileHandle $path;
  die "Cannot open BDE file $path\n" if ! $fh;
  $self->{fh} = $fh;
  $self->{opened} = 1;


  my @shape = ();
  my @fldnames = ();
  my @fields = ();
  if( $self->{override_flds} )
  {
      @fields = map { s/\=.*// } split(' ',$self->{override_flds});
  }

  my %headerfields = ();

  while( $_ = $self->_getline ) 
  {
      last if /\{CRS-DATA\}/i;
      if( /^TABLE\s+(\S+)\s*$/ )
      {
          $headerfields{TABLE} = 1;
          $self->{table} = lc($1);
      }
      elsif( /^COLUMN\s+(\S+)\s+(\S+)/  )
      {
          $headerfields{COLUMN} = 1;
          if( ! @fields )
          {
            push(@fldnames,lc($1));
            push(@shape,$2 =~ /geometry/i ? 1 : 0);
          }
      }
      elsif( /^(START|END)\s+(\d{4}-\d\d-\d\d\s\d\d\:\d\d\:\d\d)\s*$/ )
      {
          $headerfields{$1} = 1;
          if( $1 eq 'START' ){ $self->{start_time} = $2; }
          else { $self->{end_time} = $2; }
      }
  }
  die "Invalid BDE file $path - header missing or incorrect\n" 
     if grep {! $headerfields{$_} } qw/TABLE COLUMN START END/;

  # If the fields are overridden, then process the override list
  if( @fields )
  {
     foreach (@fields ) 
     {
         my ($v,$t) = split;
         push(@fldnames,lc($v));
         push(@shape,$t =~ /geometry/i ? 1 : 0);
     }
  }
  $self->{flds} = \@fldnames;
  $self->{nflds} = scalar(@fldnames);
  $self->{shapefld} = \@shape;

  $self->{srid} = $srid;
  $self->{fixlon} = undef;
  if( $lonoffset != 0 )
  {
     my $re = qr/((?>\d+)(?:\.\d*)?)\s/;
     $self->{fixlon} = sub {
         $_[0] =~ s/$re/($1+$lonoffset).' '/eg;
         return $_[0];
         }
  }

  $self->{fh} = $fh;
  return $self;
}

sub DESTROY 
{
  my ($self) = @_;
  $self->close();
}

sub close {
  my ($self) = @_;
  close($self->{fh}) if $self->{fh} && ! $self->{closed};
  $self->{closed} = 1;
  }

sub path { my ($self) = @_; return $self->{path}; }

sub name { my ($self) = @_; return $self->{name}; }

sub table { my ($self) = @_; $self->_open if ! $self->{table}; return $self->{table}; }

sub start_time { my ($self) = @_; $self->_open() if ! $self->{start_time}; return $self->{start_time}; }

sub end_time { my ($self) = @_; $self->_open() if ! $self->{end_time}; return $self->{end_time}; }

sub fields {
  my($self) = @_;
  $self->_open if ! $self->{flds};
  return wantarray ? @{$self->{flds}}: $self->{flds};
  }

sub archive_files {
  my($self,@files) = @_;
  @files = @{$files[0]} if ref($files[0]);
  @files = $self->fields if ! @files && ! $self->{archive_files};
  $self->{archive_files} = \@files if @files;
  my @sfiles = @{$self->{archive_files}};
  return wantarray ? @sfiles : \@sfiles;
  }

sub output_fields {
  my($self,@flds) = @_;
  @flds = @{$flds[0]} if ref($flds[0]);
  @flds = $self->fields if ! @flds && ! $self->{output_fields};
  $self->{output_fields} = \@flds if @flds;
  my @sflds = @{$self->{output_fields}};
  return wantarray ? @sflds : \@sflds;
  }

sub _error {
  my($self,$line,$error) = @_;
  my $fn = $self->{path};
  my $lineno = $self->{fh}->input_line_number;
  die "Error processing $fn at line $line\n$lineno\n$error\n";
  }

sub _fixshape {
  my($self,$data) = @_;
  $data = substr($data,2);
  $data = $self->{fixlon}->($data) if $self->{fixlon};
  $data = "SRID=".$self->{srid}.";".$data;
  return $data;
  }

sub _getline {
  my($self) = @_;
  return decode($self->{encoding},$self->{fh}->getline());
  }

sub data {
  my($self) = @_;
  return $self->{data};
  }

sub next {
  my( $self ) = @_;

  $self->{data} = [];
  return 0 if $self->{closed};
  $self->_open;

  my @result;
  my $line = $self->_getline;
  return 0 if ! $line;

  while (1) {
    while ( $line !~ /\|$/s ) {
      my $extra = $self->_getline;
      $self->_error($line,"Incomplete or incorrectly terminated record") 
        if $extra eq '';
      $line .= $extra;
    }
    @result = $line =~ /((?>(?>\\.|[^\\\|]*)*))\|/g;
    last if @result >=  $self->{nflds};
    my $extra = $self->_getline;
    $self->_error($line,"Incomplete or incorrectly terminated record") 
        if $extra eq '';
    $line .= $extra;
  }
  $self->_error($line,"Too many fields in record") 
    if scalar(@result) > $self->{nflds};

  my $data = [];
  if( ! $self->{output_fldids} )
  {
      my $ids;
      if( $self->{output_fields} )
      {
          my $flds = $self->fields;
          $ids = [];
          foreach my $f ( @{$self->{output_fields}} )
          {
              my $id = -1;
              $f = lc($f);
              for my $i (0..$#$flds)
              {
                  next if lc($flds->[$i]) ne $f;
                  $id = $i;
                  last;
              }
              push(@$ids,$id);
          }
      }
      else
      {
          $ids = [0..$#{$self->{fields}}];
      }
      $self->{output_fldids} = $ids;
  }

  foreach my $i ( @{$self->{output_fldids}} )
  {
     if( $i < 0 )
     {
         push(@$data,'');
         next;
     }
     my $d = $result[$i];
     $d =~ s/\\(.)/$1/g;
     if( $self->{shapefld}->[$i] && $d ne '')
     {
        $d = $self->_fixshape($d);
     }
     push(@$data,$d);
  }
  $self->{data} = $data;
  return 1;
}


sub _bdecopy
{
    return $bdecopy if $bdecopy;
    my $image;
    my $program = 'bde_copy';
    if ($^O =~ /MSWin32/)
    {
      $image = (caller)[1];
      $image =~ s/[^\\\/]*$//;
      $image .= "bde_copy/$program.exe";
      $image = undef if ! -x $image;
    }
    if (!$image)
    {
      require File::Which;
      $image =  File::Which::which($program);
    }
    die "BdeFile::copy - bde_copy program is unavailable\n" if ! -x $image;
    $bdecopy = $image;
    return $bdecopy;
}

sub copy
{
    my($self,$outputfile,@options) = @_;

    my $exe = _bdecopy();
    my $opts = ref($options[0]) ? $options[0] : {@options};
    my @copyopts;
    my $cfgtmp;
    my $cfg = $opts->{config};
    if( $cfg =~ /\n/)
    {
        use File::Temp;
        my $htmp;
        ($htmp,$cfgtmp) = File::Temp::tempfile();
        print $htmp $cfg;
        CORE::close($htmp);
        $cfg = $cfgtmp;
    }
    push(@copyopts,'-c',$cfg) if $cfg;

    my @addfiles;
    push(@addfiles,$opts->{addfile}) if $opts->{addfile};
    push(@addfiles,@{$self->{archive_files}}) 
        if $self->{archive_files} && $opts->{use_archive};
    my $addfile = join('+',@addfiles);
    push(@copyopts,'-p',$addfile) if $addfile;

    push(@copyopts,'-a') if $opts->{append};
    push(@copyopts,'-z') if $opts->{compress};

    my $flds = join(':',split(' ',$self->{override_flds}));
    push(@copyopts,'-f',$flds) if $flds;

    my $where = join(':',split(' ',$opts->{filter}));
    push(@copyopts,'-w',$where) if $where;

    $flds = $opts->{output_fields};
    if( $flds )
    {
        if( ! ref($flds) )
        {
            my @f = $flds =~ /\w+/g;
            $flds = \@f;
        }
        $self->output_fields($flds);
    }

    if( $self->{output_fields} )
    {
        $flds = join(':',@{$self->{output_fields}});
        push(@copyopts,'-o',$flds) if $flds;
    }

    my $log = $opts->{log_file} || $outputfile.".log";

    system(
        $exe,
        @copyopts,
        $self->{path},
        $outputfile,
        $log
    );
    unlink($cfgtmp) if $cfgtmp;
    my $nrec=0;
    my $nerrors=0;
    my @warnings=();
    my @errors=();
    my @fields=();
    my $status='unknown';

    if (CORE::open(my $logf, $log ))
    {
        my $nextline = '';
        while(my $line = $nextline || $logf->getline )
        {
            $nextline = '';
            if( $line =~ /^(Warning|Error)\:\s(.*)/si )
            {
                my ($type,$message)= (lc($1),$2);
                while( $line = $logf->getline )
                {
                    if( $line =~ /^\w+\:\s/ )
                    {
                        $nextline=$line;
                        last;
                    }
                    else
                    {
                        $message .= $line if $line =~ /\S/;
                    }
                }
                if( $type eq 'error' ) { push(@errors,$message); }
                else { push(@warnings,$message);}
            }
            elsif ( $line =~ /^TableName\:\s+(.*?)\s*$/i )
            {
                $self->{table} = $1
            }
            elsif ( $line =~ /^BdeStartTime\:\s+(.*?)\s*$/i )
            {
                $self->{start_time} = $1
            }
            elsif ( $line =~ /^BdeEndTime\:\s+(.*?)\s*$/i )
            {
                $self->{end_time} = $1
            }
            elsif ( $line =~ /^OutputCount\:\s+(\d+)/i )
            {
                $nrec = $1
            }
            elsif ( $line =~ /^ErrorCount\:\s+(\d+)/i )
            {
                $nerrors = $1
            }
            elsif ( $line =~ /^ResultStatus\:\s+(\w+)/ )
            {
                $status = $1
            }
            elsif ( $line =~ /^OutputFields\:\s+(\S+)/ )
            {
                @fields = split(/\|/,$1);
            }
            elsif ( $line =~ /^InputFields\:\s+(\S+)/ )
            {
                my @f = split(/\|/,$1);
                $self->{flds} = \@f;
                $self->{nflds} = scalar(@f);
            }
        }
        CORE::close($logf);
    }
    unlink($log) if $opts->{log_file} || ! $opts->{keep_log};
    my $success = $status eq 'success' && -r $outputfile;
    my $result = 
    {
        success => $success,
        status => $status,
        nrec => $nrec,
        nerrors => $nerrors,
        warnings => \@warnings,
        errors => \@errors,
        fields => \@fields,
    };

    return $result;
}


package LINZ::BdeDataset;

use fields qw{ path files name level archive };

sub new
{
   my ($class, $path, $level,$archive) = @_;
   die "Invalid LINZ::BdeDataset path $path specified\n" if ! -d $path;
   die "Invalid LINZ::BdeDataset level $level specified\n" if $level !~ /^[05]$/;
   my $self = fields::new($class);
   $self->{path} = $path;
   my $name = $path;
   $name =~ s/.*[\/\\]//;
   $self->{name} = $name;
   $self->{level} = $level;
   $self->{archive} = $archive;
   $self->{files} = undef;
   return $self;
}

sub name { return $_[0]->{name}; }

sub path { return $_[0]->{path}; }

sub level { return $_[0]->{level}; }

sub _files 
{
   my ($self) = @_;
   if( ! $self->{files} )
   {
      my $files = {};
      my $p = $self->{path};
      opendir(my $dh, $p);
      foreach my $d ( grep { /\.crs(?:\.gz)?$/i && -f "$p/$_" } readdir($dh) )
      {
         my $fn = lc($d);
         $fn =~ s/\..*//;
         $files->{$fn} = $d;
      }
      closedir($dh);
      $self->{files} = $files;
    }
    return $self->{files};
}

sub files
{
   my($self) = @_;
   my @f = sort keys %{$self->_files};
   return wantarray ? @f : \@f;
}

sub has_file
{
   my( $self,$f) = @_;
   return exists $self->_files->{lc($f)}
}

sub open
{
   my( $self, $f) = @_;
   my $fn = $self->_files->{lc($f)};
   die "Invalid Bde file $f requested from dataset $self->{name}\n" if ! $fn;
   my $file = LINZ::BdeFile->open($self->{path}."/".$fn);
   if( $self->{archive} && -d $self->{archive})
   {
       my $archive = $self->{archive};
       my @archive_files = ();

       opendir(my $dh, $archive);
       foreach my $af (readdir($dh))
       {
           my @parts = split(/\./,$af);
           next if lc($parts[0]) ne lc($f);
           shift(@parts);
           my $ok = 1;
           foreach my $p (@parts)
           {
               $ok = 0 if $p =~ /^20\d{12}$/ && $1 > $self->name;
           }
           push(@archive_files,$archive."/".$af) if $ok;
       }
       closedir($dh);
       if( @archive_files )
       {
           $file->archive_files(\@archive_files);
       }
   }
   return $file;
}

package LINZ::BdeRepository;

use fields qw{ path datasets level level0 level5 archive };

our $bde_root = '\\\\bde_server\\bde_data';

sub set_bde_root
{
   my($root) = @_;
   $bde_root = $root;
}

sub new
{
   my ($class, $root, $level,$archive) = @_;
   my $self = fields::new($class);
   $self->{path} = $root || $bde_root;
   $self->{archive} = $archive;
   $self->{datasets} = undef;
   $self->{level} = defined($level) ? $level : '?';
   die "Invalid BDE repository path $self->{path}\n" if ! -d $self->{path};
   return $self;
}

sub path { return $_[0]->{path}; }

sub get_level
{
    my ($self,$level) = @_;
    die "Invalid level requested\n" if $level ne '0' && $level ne '5';

    my $field = "level".$level;
    return $self->{$field} if exists $self->{$field};
    my $dir = $self->path."/level_".$level;
    die "Level $level directory $dir doesn't exist\n" if ! -d $dir;
    my $archive = $self->path."/level_".$level."_archive";
    $archive = undef if ! -d $archive;
    $self->{$field} = new LINZ::BdeRepository($dir,$level,$archive);
    return $self->{$field};
}

sub level0 { return $_[0]->get_level('0'); }
sub level5 { return $_[0]->get_level('5'); }

sub datasets
{
   my ($self) = @_;
   if( ! $self->{datasets} )
   {
      my $p = $self->{path};
      opendir(my $dh, $p);
      my @dirs = sort grep { /^\d{14}$/ && -d "$p/$_" } readdir($dh);
      closedir($dh);
      my @datasets = 
          map { new LINZ::BdeDataset($self->{path}."/".$_,$self->{level},$self->{archive}) } @dirs;
      $self->{datasets} = \@datasets;
   }
   return wantarray ? @{$self->{datasets}} : $self->{datasets};
}

sub dataset
{
    my($self,$name) = @_;
    my ($d) = grep {$_->name eq $name} $self->datasets;
    return $d;
}

sub before 
{
    my ($self,$start) = @_;
    my @d = grep { $_->name lt $start } $self->datasets;
    my $copy = new LINZ::BdeRepository($self->{path},$self->{level},$self->{archive});
    $copy->{datasets} = \@d;
    return $copy;
}

sub after 
{
    my ($self,$start) = @_;
    my @d = grep { $_->name gt $start } $self->datasets;
    my $copy = new LINZ::BdeRepository($self->{path},$self->{level},$self->{archive});
    $copy->{datasets} = \@d;
    return $copy;
}

sub last_dataset { return $_[0]->datasets->[-1]; }

1;

__END__

=head1 NAME

LINZ::Bde -- Reads and parses Landonline BDE files

=head1 SYNOPSIS

  use LINZ::Bde;

  # LINZ::BdeFile functions

  LINZ::BdeFile::set_bde_path("c:/bde_data/level0/20100301230530");
  LINZ::BdeFile::set_srid_lon_offset(4167,160.0);

  $bdefile = LINZ::BdeFile->open("par1");
  $bdefile->copy( $outputfile );
  $bdefile->copy( $outputfile, 
        config=>$cfg, 
        output_fields=>'id|shape',
        filter=>'status=AUTH type!=STRING',
        add_file=>'/archive/l0/zajb.l0.1.gz+/archive/l0/zajb.l0.2.gz',
        use_archive=>0,
        append=>0,
        compress=>0,
        keep_log=>1,
        );

  $bdefile = LINZ::BdeFile->open("par1");
  $bdefile->output_fields(qw/id area status shape/);

  print "Selected fields ",join(", ",$bdefile->output_fields),"\n";
  print "Reading table ",$bdefile->table," from ",$bdefile->path,"\n";
  print "Table fields ",join(", ",$bdefile->fields),"\n";
  print "Start time ",$bdefile->start_time,"\n";
  print "End time ",$bdefile->end_time,"\n";

  while(1)
  {
     eval {
       last if ! $bdefile->next;
       print join("\t",@{$bdefile->data}),"\n";
     };
     if( $@ )
     {
       print "Error: $@\n";
     }
  }
  $bdefile->close;

  # LINZ::BdeRepository/LINZ::BdeDataset functions

  $bde = new LINZ::BdeRepository;
  $bde = new LINZ::BdeRepository("\\\\bde_server\\bde_data");
  $bde = new LINZ::BdeRepository("\\\\bde_server\\bde_data\\l5",'5');
  $l0 = $bde->level0;
  $l5 = $bde->level5;
  $datasets = $l5->datasets;
  $subset = $l5->after("20091012");
  $subset = $l5->before("20091012");
  $dataset = $l5->dataset("20091012030201");
  $last_dataset = $l5->last_dataset;
  $new_datasets = $l5->after("20091012")->datasets;

  $dataset = new LINZ::BdeDataset("c:/bde_data/l5/20091012034112",'5');
  $level = $dataset->level;
  $files = $dataset->files;
  $ok = $dataset->has_file("wrk1");
  $bdefile = $dataset->open("wrk1");

=head1 ABSTRACT

The Bde  module is used to read bulk-data extract files generated by Landonline.  These are pipe delimited
files of data.  The data in each file is preceded by header text defining the nature of the extract and 
the table and fields in the data.  The module allows for reading the files in text or compressed mode. 
It also modifies geometry properties, adding an SRID to the beginning and optionally offsetting the longitude.

The module also includes classes to manage a BDE repository.  This consists
of subfolders level_0 and level_5 for the full and incremental update files.
Each folder contains subfolders for each update, which are named according to 
the time of the update as YYYYMMDDhhmmss.  Within each folder the files for
each table are named xxx.crs.gz, where xxx is a code for the table.

The repository may also contain subfolders level_0_archive and level_5_archive.
These contain additional data files which contain data that is part of the
table, but is not included in the files retrieved from Landonline.  These hold
data from the tables that is assumed to never change (historic data), and for 
efficiency is not included in the extract.  These files are called xxx.yyy where
yyy is an arbitrary continuation of the file name.  If yyy ends ".gz" the file
is assumed to be gzipped.  If it contains a string ".YYYYMMDDhhmmss." then it 
will be included with updates after that date.

=head1 Description

=head2 Class methods

The following class functions are provided:

=over

=item LINZ::BdeFile::set_bde_path( path )

Sets a default directory in which to find BDE files. 

=item LINZ::BdeFile::set_srid_lon_offset( srid, [lon_offset] )

Sets the geometry SRID and the longitude offset applied to geometry fields.  These default to 4167 and 160.0.

=item LINZ::BdeFile::set_encoding( encoding )

Sets the encoding used to read the file.  The default is cp-1252.

=back

When a new LINZ::BdeFile is created it uses the class properties currently in effect.  Subsequent changes do not affect the reader.

=head2 Instance methods

=over 

=item $bdefile = LINZ::BdeFile->new( file, [fields] )

Open a reader on a file.  The filename can be a full file name.  If not the reader will try appending .crs and .crs.gz, and try prepending the current bde path.  
Optionally this can include a hash ref of field definitions, each entry of which contains a space separated field name 
and type.  The type is ignored, apart from if it contains "geometry", in which case the geometry fixes will be applied.
This will die if the file cannot be found.
Note that some files have incorrect fields defined in the file header.  These 
are defined in the $override_columns variable.  Currently those defined are 
ct01 and ct02.

=item $f = $bdefile->name

=item $f = $bdefile->path

Returns the name and full path of the file being read

=item $t = $bdefile->table

Returns the name of the table contained in the file

=item $s = $bdefile->start_time

=item $e = $bdefile->end_time

Returns the start and end time specified for the extract

=item $c = $bdefile->fields

Returns an array or array reference to a list of field names

=item $bdefile->archive_files(files)

Retrieve or specify the archive files to include with the 
data set.  If the file is opened from a BdeRepository/BdeDataset
then this will be automatically populated.

=item $bdefile->output_fields(fields)

Choose which fields are to be read from the file.  
The default is to read all fields.  I<fields> can be 
an array or array reference of field names.  Any names not matched 
in the file are silently ignored.  Matching field
names is case insensitive.  The function can be called without 
any parameters to return the list of selected fields.

=item $bdefile->next

Moves the reader on to the next field.  Returns 1 if successful, 0 if the end of file is reached.  Data format errors
will generate an exception.

=item $d = $bdefile->data

Returns an array reference of the selected fields.  This can only be called after C<< $bdefile->next >>.

=item $bdefile->close

Releases the file handle used by the reader

=item $result = $bdefile->copy

Copies the data from the BDE file to another file, applying transformations 
defined by the configuration.  Uses the bde_copy.exe program.  $result is 
a hash containing:

=over

=item success 

True or false

=item status 

A text value representing the reuslt status

=item nrec 

The number of records

=item nerrors 

The number of errors

=item errors 

An array reference for a list of errors

=item warnings 

An array reference for a list of warnings

=item fields 

An array reference for a list of output fields

=back

=cut
