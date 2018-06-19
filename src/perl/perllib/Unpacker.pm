#!/usr/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Perl module to unpack data in a binary format, independent
#                      of the host architecture.  (In fact limited to platforms
#                      it will work on, but is fine for Solaris and Intel
#                      platforms.  Output format can be specified to be either
#                      big or little endian.
#
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook          03/02/2004  Created
#===============================================================================


use strict;

package Unpacker;

sub bigendian {
  return pack("n",1) eq pack("s",1);
  }

sub new {
  my ($class,$packbigendian,$fh) = @_;
  my ($bigendian,$swapbytes);

  $bigendian = &bigendian();

  if( $packbigendian ) {
     $swapbytes = ! $bigendian;
     }
  else {
     $swapbytes = $bigendian;
     }
  
  my $self = {swapbytes=>$swapbytes,
              packbigendian=>$packbigendian,
              bigendian=>$bigendian,
              fh=>$fh
             };
  return bless $self, $class;               
  }

# Signed byte

sub byte {
   my ($self,$buffer,$nchar)  = @_;
   $nchar = 1 if ! $nchar;
   return unpack('c'.$nchar,$buffer);
}

# Unsigned byte

sub char {
   my ($self,$buffer,$nchar)  = @_;
   $nchar = 1 if ! $nchar;
   return unpack('C'.$nchar,$buffer);
}


sub short {
   my ($self, $buffer, $nshort) = @_;
   $nshort = 1 if ! $nshort;
   my @result;
   if( $self->{swapbytes} ) {
      @result = reverse unpack('s'.$nshort, reverse $buffer );
      }
   else {
      @result = unpack('s'.$nshort,$buffer);
      }
   return wantarray ? @result : $result[0];
}

sub long {
   my ($self, $buffer, $nlong) = @_;
   $nlong = 1 if ! $nlong;
   my @result;
   if( $self->{swapbytes} ) {
      @result = reverse unpack('l'.$nlong, reverse $buffer );
      }
   else {
      @result = unpack('l'.$nlong,$buffer);
      }
   return wantarray ? @result : $result[0];
}

sub double {
   my($self,$buffer,$ndouble) = @_;
   $ndouble = 1 if ! $ndouble;
   my @result;
   if( $self->{swapbytes} ) {
      @result = reverse unpack('d'.$ndouble, reverse $buffer );
      }
   else {
      @result = unpack('d'.$ndouble,$buffer);
      }
   return wantarray ? @result : $result[0];
}


sub read_byte {
   my($self,$nchar) = @_;
   $nchar = 1 if ! $nchar;
   my $buffer;
   my $fh = $self->{fh};
   read($fh,$buffer,$nchar) == $nchar || die "Error reading file\n";
   return $self->byte($buffer,$nchar);
   }


sub read_char {
   my($self,$nchar) = @_;
   $nchar = 1 if ! $nchar;
   my $buffer;
   my $fh = $self->{fh};
   read($fh,$buffer,$nchar) == $nchar || die "Error reading file\n";
   return $self->char($buffer,$nchar);
   }

sub read_short {
   my($self,$nshort) = @_;
   $nshort = 1 if ! $nshort;
   my $buffer;
   my $fh = $self->{fh};
   read($fh,$buffer,$nshort*2) == $nshort*2 || die "Error reading file\n";
   return $self->short($buffer,$nshort);
   }

sub read_long {
   my($self,$nlong) = @_;
   $nlong = 1 if ! $nlong;
   my $buffer;
   my $fh = $self->{fh};
   read($fh,$buffer,$nlong*4) == $nlong*4 || die "Error reading file\n";
   return $self->long($buffer,$nlong);
   }

sub read_double {
   my($self,$ndouble) = @_;
   $ndouble = 1 if ! $ndouble;
   my $buffer;
   my $fh = $self->{fh};
   read($fh,$buffer,$ndouble*8) == $ndouble*8 || die "Error reading file\n";
   return $self->double($buffer,$ndouble);
   }

sub read_string {
  my($self,$nstring) = @_;
  $nstring = 1 if ! $nstring;
  my @strings;
  my $fh = $self->{fh};
  for my $i (1..$nstring) {
    my ($len) = $self->read_short;
    my $string;
    read($fh,$string,$len) == $len || die "Error reading file\n";
    $string =~ s/\0//g;
    push(@strings,$string);
    }
  return wantarray ? @strings : $strings[0];
  }

1;
