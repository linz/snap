# Perl module for managing SNAP data files...
# Currently has near zero functionality... only works for simple GPS
# baseline files, only reads, ...

use strict;
use FileHandle;


package SNAP::ObsSet;

sub new {
   my( $class,$from,$fromhgt, $obs, $cvr) = @_;
   my $self = { from=>$from, fromhgt=>$fromhgt, obs=>$obs, pnt=>0, cvr=>($cvr || []) };
   return bless $self, $class;
   }

sub from { return $_[0]->{fromstn}; }
sub fromhgt { return $_[0]->{fromhgt}; }
sub obs { return wantarray ? @{$_[0]->{obs}} : $_[0]->{obs}; }
sub cvr { return $_[0]->{cvr}; }

package SNAP::Obs;

sub new 
{
    my( $class ) = @_;
    return bless {}, $class;
}

sub from { return $_[0]->{fromstn}; }
sub fromhgt { return $_[0]->{fromhgt}; }
sub to { return $_[0]->{tostn}; }
sub tohgt { return $_[0]->{tohgt}; }
sub date { return $_[0]->{date}; }
sub type { return $_[0]->{type}; }
sub values { return @{$_[0]->{value}}; }
sub value { return wantarray ? $_[0]->values : $_[0]->{value}; }
sub errors { return @{$_[0]->{error}}; }
sub error { return wantarray ? $_[0]->errors : $_[0]->{error}; }
sub attrs { return keys %{$_[0]->{classification}}; }
sub attr { return $_[0]->{classification}->{$_[1]}; }
sub projection { return $_[0]->{projection}; }
sub errtype { return $_[0]->{errtype}; }
sub rejected { return $_[0]->{rejected}; }
sub filename { return $_[0]->{filename}; }
sub lineno { return $_[0]->{lineno}; }

package SNAP::DataFile;

my $datatypes = {
   'SD' => { nval=>1, nerr=>1, grouped=>0, bearing=>0, distance=>1, projection=>0, errmult=>1.0 },
   'HD' => { nval=>1, nerr=>1, grouped=>0, bearing=>0, distance=>1, projection=>0, errmult=>1.0 },
   'ED' => { nval=>1, nerr=>1, grouped=>0, bearing=>0, distance=>1, projection=>0, errmult=>1.0 },
   'DR' => { nval=>1, nerr=>1, grouped=>1, bearing=>0, distance=>0, projection=>0, errmult=>1.0 },
   'HA' => { nval=>0, nerr=>1, grouped=>1, bearing=>0, distance=>0, projection=>0, errmult=>1.0/3600.0 },
   'ZD' => { nval=>0, nerr=>1, grouped=>0, bearing=>0, distance=>0, projection=>0, errmult=>1.0/3600.0 },
   'AZ' => { nval=>0, nerr=>1, grouped=>0, bearing=>1, distance=>0, projection=>0, errmult=>1.0/3600.0 },
   'PB' => { nval=>0, nerr=>1, grouped=>0, bearing=>1, distance=>0, projection=>1, errmult=>1.0/3600.0 },
   'LV' => { nval=>1, nerr=>1, grouped=>0, bearing=>0, distance=>0, projection=>0, errmult=>1.0 },
   'LN' => { nval=>1, pnt=>1, nerr=>1, grouped=>0, bearing=>0, distance=>0, projection=>0, errmult=>1.0/3600.0 },
   'LT' => { nval=>1, pnt=>1, nerr=>1, grouped=>0, bearing=>0, distance=>0, projection=>0, errmult=>1.0/3600.0 },
   'EH' => { nval=>1, pnt=>1, nerr=>1, grouped=>1, bearing=>0, distance=>0, projection=>0, errmult=>1.0 },
   'OH' => { nval=>1, pnt=>1, nerr=>1, grouped=>1, bearing=>0, distance=>0, projection=>0, errmult=>1.0 },
   'GB' => { nval=>3, nerr=>0, grouped=>0, bearing=>0, distance=>0, projection=>0, errmult=>1.0 },
   'GPS' => { nval=>3, nerr=>0, grouped=>0, bearing=>0, distance=>0, projection=>0, errmult=>1.0 },
   };

my $cvr_types = {
   FULL => [6,0,1],
   CORRELATION => [6,3,2],
   DIAGONAL => [3,3,0],
   ENU => [3,3,0]
   };

my %months = qw/JAN 01 FEB 02 MAR 03 APR 04 MAY 05 JUN 06 JUL 07 AUG 08 SEP 09 OCT 10 NOV 11 DEC 12/;

sub open {
   my($class,$file) = @_;
   my $fh = new FileHandle( $file );
   return undef if ! $fh;
   my $title = $fh->getline;
   chomp $title;
   my $self = { name=>$file, 
                fh=>$fh, 
                dataspec=>[], 
                title=>$title, 
                angleformat=>3,
                gps_error_type=>'FULL' };

   $self->{ngpscvr} = $cvr_types->{$self->{gps_error_type}};

   return bless $self,$class;
   }

sub _error {
   my $self = shift;
   die join('',@_,"\n");
   }

sub parseDate
{
   my $srcdate=join(' ',@_);
   my @date=split(/\W/,$srcdate);
   my $mon=$date[1];
   $mon=$months{uc(substr($mon,0,3))} if $mon !~ /\d\d/;
   $date[1]=$mon;
   if( $date[2] =~ /\d\d\d\d/ )
   {
      my $y=$date[2];
      $date[2]=$date[0];
      $date[2]=$y;
   }
   my $datestr=$date[0].'-'.$date[1].'-'.$date[2];
   die "Invalid date $srcdate\n" if $datestr !~ /^(19|20)\d\d\-[01]\d-[0123]\d$/;
   return $datestr;
}

sub filename { return $_[0]->{name}; }
sub title { return $_[0]->{title}; }
sub close 
{ 
    my($self) = @_;
    close($self->{fh}) if $self->{fh};
    $self->{fh} = undef;
}

sub next {
   my ($self) = @_;
   my( $fh, $dataspec ) = @$self{'fh','dataspec'};
   my $line;
   my $grouped = $self->{grouped};
   my $heights = $self->{heights};
   my $fromstn = $self->{fromstn};
   my $fromhgt = $self->{fromhgt};
   my $tostn = undef;
   my $tohgt = undef;
   my $obs = [];
   my $cvr = undef;
   my $ncvr = 0;
   my $pnt = $self->{pnt};

   while( 1 ) {
      last if ! ($line = $self->{nextline} || $fh->getline);
      delete $self->{nextline};

      # Blanks and comments

      $line =~ s/\!.*//;
      next if $line =~ /^\s*$/;

      while( $line =~ /\s\&\s*$/ ) { $line = $`.' ',$fh->getline; }

      if( $line =~ /^\s*\#/ ) {
          if( @$obs ) {
             $self->{nextline} = $line;
             last;
             }

          my ($def,@values) = split(' ',$');
          $def = lc($def);
          if( $def eq 'data' ) {
              my $datafield = undef;
              my $datatype = undef;
              $self->{data}=[];
              $grouped = $self->{grouped} = 0;
              $heights = $self->{heights} = 1;
              foreach my $field (@values) {
                 if( uc($field) eq 'GROUPED' ) {
                    $grouped = $self->{grouped} = 1;
                    $datafield = undef;
                    }
                 elsif( uc($field) eq 'NO_HEIGHTS' ) {
                    $heights = $self->{heights} = 0;
                    $datafield = undef;
                    }
                 elsif( exists($datatypes->{uc($field)}) ) {
                    my $typecode = uc($field);
                    $typecode = 'GB' if $typecode eq 'GPS';
                    $datatype = $datatypes->{uc($field)};
                    $datafield = { type=>uc($field), 
                                   fields=>[],
                                   datatype=>$datatype,
                                 };
                    push(@{$self->{data}},$datafield);
                    $grouped = $self->{grouped} = 1 if $datatype->{grouped};
                    $pnt = $self->{pnt} = $datatype->{pnt};
                    }
                 elsif( $datafield &&
                        ($field =~ /^(value|error|time|distance_scale_error|refraction_coefficient|bearing_orientation_error)$/i ||
                         exists ($self->{classification}->{lc($field)}))) {
                    push( @{$datafield->{fields}}, $field );
                    $datafield->{goterror} = 1 if lc($field) eq 'error';
                    }
                 else {
                    $self->_error( "Invalid data command in ",$self->{name},
                                  " at line ",$fh->input_line_number );
                    }
                 }

              my $col = 0;
              my $nangles = 0;
              my $gpsvar = 0;
              foreach my $dataitem (@{$self->{data}}) {
                 my $fields = $dataitem->{fields};
                 unshift(@$fields,'value') if ! grep {/^value$/i} @$fields;
                 }
              }
           elsif( $def eq 'projection' ) {
              $self->{projection} = uc($values[0]);
              }
           elsif( $def eq 'bearing_orientation_error' ) {
              $self->{classification}->{bearing_orientation_error} = uc($values[0]);
              }
           elsif( $def eq 'distance_scale_error' ) {
              $self->{classification}->{distance_scale_error} = uc($values[0]);
              }
           elsif( $def eq 'gps_error_type' ) {
              $self->{gps_error_type} = uc($values[0]);
              $self->{ngpscvr} = $cvr_types->{$self->{gps_error_type}};
              }
           elsif( $def eq 'reference_frame' ) {
              $self->{classification}->{reference_frame} = uc($values[0]);
              }
           elsif( $def eq 'classify' ) {
              if( @values == 2 ) { unshift(@values,'*'); }
              my @codes = split(/\//,uc($values[0]));
              my $classification = lc($values[1]);
              my $value = uc($values[2]);
              my %lookup = map { ($_,$value) } @codes;
              $self->{classification}->{$classification} = \%lookup;
              }
           elsif( $def eq 'classification' ) {
              $self->{classification}->{lc($values[0])} = {};
              }
           elsif( $def eq 'date' ) {

              $self->{date} = parseDate(@values);
              }
           elsif( $def eq 'dms_angles' ) {
              $self->{angleformat} = 3;
              }
           elsif( $def eq 'hp_angles' ) {
              $self->{angleformat} = 2;
              }
           elsif( $def eq 'deg_angles' ) {
              $self->{angleformat} = 1;
              }
           elsif( $def =~ /^(ha|zd|az|lt|ln)\_error$/ ) {
              my $etype = uc($1);
              $self->{default_error}->{$etype}->{sec} = $values[0] 
                    if $values[1]=~/^sec/i;
              }
           elsif( $def =~ 'ds_error' ) {
              $self->{default_error}->{DS}->{lc($values[1])} = $values[0]; 
              $self->{default_error}->{DS}->{lc($values[3])} = $values[2];
              }
           elsif( $def =~ 'lv_error' ) {
              $self->{default_error}->{LV}->{lc($values[1])} = $values[0]; 
              }
           elsif( $def =~ 'gps_enu_error' ) {
              $self->{default_error}->{GB}->{lc($values[3])} = [@values[0..2]];
              $self->{default_error}->{GB}->{lc($values[7])} = [@values[4..6]];
              }
           next;
           }
      
      my $rejected = ($line =~ s/^\s*\*//);
      my @data = split(' ',$line);
      my $grouphdr = $grouped && (scalar(@data) <= (1 + $heights)) && ! $pnt;
      my $colno = 0;
      if( ! $grouped || $grouphdr ) {
         if( @$obs ) {
             $self->{nextline} = $line;
             last;
             }
         $fromstn = $self->{fromstn} = $data[$colno++];
         $fromhgt = $self->{fromhgt} = $heights ? $data[$colno++] : 0.0;
         next if $grouphdr;
         }

      if( ! $pnt )
      {
         $tostn = $data[$colno++];
         $tohgt = $heights ? $data[$colno++] : 0.0;
      }
      else
      {
         $tostn='';
         $tohgt=0.0;
      }

      foreach my $dataitem (@{$self->{data}}) {
          my $fields = $dataitem->{fields};
          my $datatype = $dataitem->{datatype};
          if( $data[$colno] eq '-') { $colno++; next; }
          my $o = new SNAP::Obs;
          push(@$obs, $o);
          $o->{fromstn} = $fromstn;
          $o->{fromhgt} = $fromhgt;
          $o->{filename} = $self->filename;
          $o->{lineno} = $fh->input_line_number;
          $o->{tostn} = $tostn;
          $o->{tohgt} = $tohgt;
          $o->{date} = $self->{date};
          $o->{type} = $dataitem->{type};
          $o->{classification} = {};
          $o->{errtype} = '';
          $o->{rejected} = '';
          $o->{projection} = '';

          my $obsrejected = 0;
          foreach my $f (@$fields) { 
             if( $f eq 'value' ) {
                if( $data[$colno] eq '*' ) { $colno++; $obsrejected = 1; }
                elsif( $data[$colno] =~ s/^\*// ) { $obsrejected = 1; }
                my $nval = $datatype->{nval};
                my @value;
                if( $nval == 0 ) {
                   if( $self->{angleformat} == 3 ) {
                      $value[0] = $data[$colno] + 
                                   $data[$colno+1]/60 + 
                                   $data[$colno+2]/3600;
                      $colno+=3;
                      }
                   elsif ( $self->{angleformat} == 2 ) {
                      my $angle = $data[$colno++];
                      $angle .= '.' if $angle !~ /\./;
                      $angle .= '00000';
                      if( $angle =~ /^(\d+)\.(\d\d)(\d\d)(\d+)$/ ) {
                         $angle = $1 + $2/60 + ($3.'.'.$4)/3600;
                         }
                      $value[0] = $angle;
                      }
                   else {
                      $value[0] = $data[$colno++];
                      }
                   if( $o->{type} =~ /^(ln|lt)$/i ) {
                      my $hem = $data[$colno++];
                      $value[0] = - $value[0] if $hem =~ /^[ws]$/i;
                      }
                   }
                else {
                   @value = @data[$colno .. $colno+$nval-1];
                   $colno += $nval;
                   }
                $o->{value} = \@value;
                if( ! $dataitem->{goterror} ) {
                   if( $datatype->{distance} ) {
                      my $dserr = $self->{default_error}->{DS};
                      $o->{error} = [sqrt(($value[0]*$dserr->{ppm}*1.0e-6)**2+
                                         ($dserr->{mm}*1.0e-3)**2)];
                      }
                   elsif( $o->{type} eq 'LV' ) {
                      $o->{error} = [$self->{default_error}->{LV}->{mm}*0.001];
                      }
                   elsif( $o->{type} eq 'GB' ) {
                      if( ! $grouped ) {
                          my $gberr = $self->{default_error}->{GB};
                          my $len = sqrt($value[0]**2+$value[1]**2+$value[2]**2);
                          my @error = map { sqrt(($gberr->{ppm}->[$_]*$len*1.0e-6)**2
                                                +($gberr->{mm}->[$_]*1.0e-3)**2) }
                                      (0..2);
                          $o->{error} = \@error;
                          $o->{errtype} = 'ENU';
                          }
                      }
                   else {
                      my $type = $o->{type};
                      $type = 'AZ' if $type eq 'PB';
                      my @error;
                      $error[0] = $self->{default_error}->{$type}->{sec}/3600.0
                         if exists($self->{default_error}->{$type});
                      $o->{error} = \@error;
                      }
                   }
                }
             elsif ( $f eq 'error' ) {
                my $nerr = $datatype->{nerr};
                my $errmult = $datatype->{errmult};
                if( $nerr == 0 ) {
                   $ncvr++ if $grouped;
                   $nerr = $self->{ngpscvr}->[$grouped ? 1 : 0];
                   $o->{errtype} = $self->{gps_error_type};
                   }
                if( $nerr ) {
                  my @error = map {$_ *= $errmult} @data[$colno .. $colno+$nerr-1];
                  $o->{error} = \@error;
                  $colno += $nerr;
                  }
                }
             else {
                $o->{classification}->{$f} = uc($data[$colno++]);
                }
             }
          foreach my $c (keys %{$self->{classification}} ) {
             next if exists $o->{classification}->{$c};
             next if ! defined( $self->{classification}->{$c} );
             next if $c eq 'bearing_orientation_error' && ! $datatype->{bearing};
             next if $c eq 'distance_scale_error' && ! $datatype->{distance};
             my $cv = $self->{classification}->{$c};
             if( ref($cv) ) {
                my $v = $cv->{$o->{type}} || $cv->{'*'};
                next if ! $v;
                $cv = $v;
                }
             $o->{classification}->{$c} = $cv;
             }
          $o->{projection} = $self->{projection} if $datatype->{projection};
          $o->{rejected} = $rejected || $obsrejected;
          }
      last if @$obs && ! $grouped;
      }

   return undef if ! @$obs;

   # Read GPS covariance data...

   if( $self->{nextline} =~ /^\s*\#\s*end_set\s*$/ ) {
      delete $self->{nextline};
      my $errtype = $self->{ngpscvr}->[2];
      if( $errtype && $ncvr ) {
        $ncvr *= 3;
        $ncvr = (($ncvr+2-$errtype)*($ncvr+1-$errtype))/2;
        $cvr = [];
        while( @$cvr < $ncvr && ($line=$fh->getline) ) {
           push(@$cvr, split(' ',$line) );
           }
        }
      }

   # Return the resulting observation

   return new SNAP::ObsSet( $fromstn, $fromhgt, $obs, $cvr, );
   }

sub allobs 
{
    my ($self) = @_;
    my $obs = [];
    while( my $set = $self->next ) { push(@$obs,$set); }
    return wantarray ? @$obs : $obs;
}


# Deprecated capitalised version

sub Open { return  SNAP::DataFile::open(@_); }
sub NextObs { return SNAP::DataFile::next(@_); }

1;

=head1 NAME

SNAP::DataFile -- Read a SNAP observation file

=head1 SYNOPSYS

  use SNAP::DataFile;

  my $of = SNAP::DataFile->open('myobsfile.dat');
  print "File: ",$of->filename,"\n";
  print "Title: ",$of->title,"\n";

  while( my $obsset = $of->next )
  {
      print "\nObservation set\n";

      foreach my $o ($obsset->obs)
      {
        print "\nObservation:\n";
        print "  file: ",$o->file,"\n";
        print "  lineno: ",$o->lineno,"\n";
        print "  from: ",$o->from,"\n";
        print "  fromhgt: ",$o->fromhgt,"\n";
        print "  to: ",$o->to,"\n";
        print "  tohgt: ",$o->tohgt,"\n";
        print "  date: ",$o->date,"\n";
        print "  type: ",$o->type,"\n";
        print "  values: ",join(' ',$o->values),"\n";
        print "  errors: ",join(' ',$o->errors),"\n";
        foreach my $a ( $o->attrs )
        {
           print "  attr $a: ",$o->attr($a),"\n";
        }
        print "  projection: ",$o->projection,"\n";
        print "  errtype: ",$o->errtype,"\n";
        print "  rejected: ",$o->rejected,"\n";
         
      }

      print "Covariance: ",join(" ",@{$obsset->cvr}),"\n";
  }

  # Alternatively read all remaining obs with the allobs function
  # After the next loop has completed this will return an empty array

  my @obs = $of->allobs;

  $of->close();

=head1 ABSTRACT

Note: This module is in a permanent status of "under development".  Features
are added as required!

Currently known areas of incompleteness:

=over

=item *

Handling of errors

=item *

Most attributes like reference_frame, projection, etc are handled as attributes, without regard for observation data type.

=item *

Systematic errors - not handled at all

=back

This module does not read the entire observation file, instead it provides an
iterator to read it sequentially.
