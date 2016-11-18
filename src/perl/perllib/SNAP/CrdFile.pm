# Perl module for managing SNAP data files...
# Currently has near zero functionality... only works for simple GPS
# baseline files, only reads, ...

use strict;
use FileHandle;
use LINZ::Geodetic::CoordSysList;
use vars qw/$ENV/;

package SNAP::Station;

sub new {
   my( $class, $code, $crd, $grv, $order, $name, $attr ) = @_;
   $attr = {} if ! $attr;
   $name = $code if ! $name;
   my $self = { code=>uc($code), coord=>$crd, grav=>$grv, order=>$order, name=>$name, attr=>$attr };
   return bless $self, $class;
   }

sub code { return $_[0]->{code}; }
sub name { return $_[0]->{name}; }
sub coord { return wantarray ? @{$_[0]->{coord}} : $_[0]->{coord}; }
sub grav { return wantarray ? @{$_[0]->{grav}} : $_[0]->{grav}; }
sub order {return $_[0]->{order}; }
sub attr {return $_[0]->{attr}->{lc($_[1])}; }
sub attrs { return keys %{$_[0]->{attr}}; }

package SNAP::CrdFile;

our $csfile;
our $cslist;

sub setcoordsyslist {
   my( $class, $pcslist ) = @_;
   $pcslist = $class if ref($class) eq 'LINZ::Geodetic::CoordSysList';
   $cslist = $pcslist;
   }

sub _basepath {
   my $filepath = $1 if $0 =~ /(^.*[\/\\:])/;
   return $filepath;
   }

sub _coordsyslist {
   if( $cslist ) { return $cslist };
   $csfile = $ENV{'COORDSYSDEF'};
   $csfile = _basepath().'coordsys.def' if $csfile eq '';
   return undef if ! -r $csfile;
   $cslist = newFromCoordSysDef LINZ::Geodetic::CoordSysList( $csfile );
   return $cslist; 
   }

sub open {
   my($class,$file) = @_;
   my $fh = new FileHandle( $file );
   die "Cannot open SNAP coordinate file $file\n" if ! $fh;
   return undef if ! $fh;
   my $title = $fh->getline;
   chomp $title;
   my $cslist = _coordsyslist();
   die "Coordinate system list or coordsys.def file not defined\n" if ! $cslist;
   my $cscode = $fh->getline;
   $cscode =~ s/\s//g;
   my $cs = $cslist->coordsys($cscode);
   die "Invalid coordinate system code $cscode in SNAP coordinate file $csfile\n" if ! $cslist;
   my $dataline = $fh->getline;
   my $geoidinfo = 1;
   my $deflinfo = 1;
   my $orderinfo = 0;
   my $ellheights = 0;
   my $isdegrees = 0;
   my @attrib = ();
   if($dataline =~ /^\s*(options|no_geoid)(\s|$)/i) {
       foreach my $option (split(' ',lc($dataline)) ) {
          if( $option eq 'options' ) { 
             $geoidinfo = 0;
             $deflinfo = 0;
             $orderinfo = 0;
             }
          elsif( $option eq 'orthometric_heights' ) {
             $ellheights = 0;
             }
          elsif( $option eq 'ellipsoidal_heights' ) {
             $ellheights = 1;
             }
          elsif( $option eq 'geoid' ) {
             $geoidinfo = 1;
             $deflinfo = 1;
             }
          elsif( $option eq 'no_geoid' ) {
             $geoidinfo = 0;
             $deflinfo = 0;
             }
          elsif( $option eq 'geoid_heights' ) {
             $geoidinfo = 1;
             }
          elsif( $option eq 'no_geoid_heights' ) {
             $geoidinfo = 0;
             }
          elsif( $option eq 'deflections' ) {
             $deflinfo = 1;
             }
          elsif( $option eq 'no_deflections' ) {
             $deflinfo = 0;
             }
          elsif( $option eq 'station_orders' ) {
             push(@attrib,'order');
             $orderinfo = 1;
             }
          elsif( $option eq 'no_station_orders' ) {
             $orderinfo = 0;
             }
          elsif( $option eq 'degrees' ) {
             $isdegrees = 1;
             }
          elsif( $option =~ /^c\=(.*)$/ ) {
             push(@attrib,$1);
             }
          }
       $dataline = $fh->getline;
       }

   my $isproj = $cs->type eq &LINZ::Geodetic::PROJECTION;
   my $isgeod = $cs->type eq &LINZ::Geodetic::GEODETIC;

   my @stations;
   my $re = '^\\s*(\\S+)';
   my $renumber = '([+-]?\\d+\\.?\\d*)';
   my $redms = '(\\d{1,3})\s+([0-5]?\\d)\s+'.$renumber;
   if( $isgeod && ! $isdegrees ) {
      $re .= '\\s+'.$redms.'\\s*([nNsS])\\s+'.$redms.'\\s*([eEwW])\\s+'.$renumber;
      }
   else {
      $re .= ('\\s+'.$renumber) x 3;
      $re .= '()' x 6;
      }
   if( $deflinfo ) {
      $re .= ('\\s+'.$renumber) x 2;
      }
   else {
      $re .= '()' x 2;
      }
   if( $geoidinfo ) {
      $re .= ('\\s+'.$renumber);
      }
   else {
      $re .= '()';
      }

   $re .= '(?:\\s+|$)';

   my $nattr = scalar(@attrib);

   for( ; $dataline; $dataline = $fh->getline ) {
       $dataline =~ s/\!.*$//;
       $dataline =~ s/\s*$//;
       next if $dataline eq '';
       die sprintf("Invalid data at line %d of %s\n", 
          $fh->input_line_number,$file) if $dataline !~ /$re/;
       my $code = $1;
       my @crd = ();
       if( $isgeod && ! $isdegrees) {
          $crd[0] = ($2+$3/60.0+$4/3600.0); $crd[0] = -$crd[0] if uc($5) eq 'S';
          $crd[1] = ($6+$7/60.0+$8/3600.0); $crd[1] = -$crd[1] if uc($9) eq 'W';
          $crd[2] = $10;
          }
       elsif( $isproj ) {
          $crd[0] = $3;
          $crd[1] = $2;
          $crd[2] = $4;
          }
       else {
          $crd[0] = $2;
          $crd[1] = $3;
          $crd[2] = $4;
          }
       my $grv = [$11,$12,$13];
       my (@attval) = split(' ',$',$nattr+1);
       my $name = $attval[$nattr] || $code;
       my $attr = {};
       for my $i (0..$nattr-1)
       {
           $attr->{$attrib[$i]} = $attval[$i];
       }
       my $order = $attr->{order};
       my $crd = $cs->coord(@crd);
       my $st = new SNAP::Station( $code, $crd, $grv, $order, $name, $attr );
       push( @stations, $st );
       }
   $fh->close;
 
   my $self = {
         name => $file,
         title => $title,
         cslist => $cslist,
         coordsys => $cs,
         stations => \@stations,
         geoidinfo => $geoidinfo,
         ellheights => $ellheights
         };
       

   return bless $self,$class;
   }

sub _error {
   my $self = shift;
   die join('',@_,"\n");
   }



sub _codeindex {
   my ($self) = @_;
   if( ! $self->{codeindex} ) {
      my $codeindex = {};
      foreach (@{$self->{stations}}) { $codeindex->{$_->{code}} = $_ };
      $self->{codeindex} = $codeindex;
      }
   return $self->{codeindex}; 
   }

sub coordsyslist { return ref($_[0]) ? $_[0]->{cslist} : _coordsyslist(); }

sub filename { return $_[0]->{name}; }
sub title { return $_[0]->{title}; }
sub coordsys { return $_[0]->{coordsys}; }
sub station { return $_[0]->_codeindex()->{uc($_[1])}; }
sub stations { return wantarray ? @{$_[0]->{stations}} : $_[0]->{stations}; }

# Deprecated capitalised version

sub SetCoordsysList { setcoordsyslist( @_ ); }
sub SetCoordSysList { setcoordsyslist( @_ ); }
sub Open { return SNAP::CrdFile::open(@_); }
sub Stations { return stations(@_); }
sub Station { return station(@_); }


1;

__END__

=head1 NAME

SNAP::CrdFile -- Reads a SNAP coordinate file

=head1 SYNOPSIS

  use SNAP::CrdFile;

  my $crds = SNAP::CrdFile->open('myfile.crd');

  print "Title: ",$crds->title,"\n";
  print "Coordinate System: ",$crds->coordsys->code,"\n";
  
  print "Stations\n";
  foreach my $s ($crds->stations)
  {
     print "\nCode: ",$s->code,"\n";
     print "  Name: ",$s->name,"\n";
     print "  Coords: ",join(",",$s->coord),"\n";
     print "  defLat,defLon,Und: ",join(",",$s->grav),"\n";
     print "  Order: ",$s->order,"\n";
    

     foreach my $a ($s->attrs)
     {
        print "  Attribute $a: ",$s->attr($a),"\n";
     }
      
  }

  # Get station by code
  my $s1 =  $s('ABCD');

=head1 ABSTRACT

The SNAP::CrdFile module is used to read SNAP coordinate files, extracting a list
of stations.

To read the file it needs to access a list of coordinate system definitions, in order to
process the coordinate system code in the SNAP file header.  The coordinate systems are 
usually read from a coordsys.def file.  By default this is located either using the 
value of an environment variable COORDSYSDEF, or if that is not found, in the same
folder as the calling program.

Alternatively the coordinate system list can be set with the setcoordsyslist function.
That is:

  use LINZ::Geodetic::CoordSysList;

  my $list = LINZ::Geodetic::CoordSysList::newFromCoordSysDef('mycoordsys.def');
  SNAP::CrdFile->setcoordsyslist($list);

  my $crds = SNAP::Crdfile->open($cslist);
