#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Perl module to pack data in a binary format, independent
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

package LINZ::Geodetic::Util::Packer;

sub bigendian {
  return pack("n",1) eq pack("s",1);
  }

sub new {
  my ($class,$packbigendian) = @_;
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
              bigendian=>$bigendian
             };
  return bless $self, $class;               
  }

#===============================================================================
#
#   SUBROUTINE:   char
#
#   DESCRIPTION:  Converts input short to packed data matching the file
#                 endian-ness
#
#   PARAMETERS:   A list of char
#
#   RETURNS:      The packed data
#
#
#===============================================================================

sub char {
   my $self = shift;
   return pack('C*',@_);
}


#===============================================================================
#
#   SUBROUTINE:   short
#
#   DESCRIPTION:  Converts input short to packed data matching the file
#                 endian-ness
#
#   PARAMETERS:   A list of shorts
#
#   RETURNS:      The packed data
#
#
#===============================================================================

sub short {
   my ($self,@data) = @_;
   my $result;
   if( $self->{swapbytes} ) {
      $result = reverse pack('s*', reverse @data);
      }
   else {
      $result = pack('s*',@data);
      }
   return $result;
}

#===============================================================================
#
#   SUBROUTINE:   long
#
#   DESCRIPTION:  Converts input longs to packed data matching the file
#                 endian-ness
#
#   PARAMETERS:   A list of longs
#
#   RETURNS:      The packed data
#
#
#===============================================================================

sub long {
   my ($self,@data) = @_;
   my $result;
   if( $self->{swapbytes} ) {
      $result = reverse pack('l*', reverse @data);
      }
   else {
      $result = pack('l*',@data);
      }
   return $result;
}

#===============================================================================
#
#   SUBROUTINE:   double
#
#   DESCRIPTION:  Converts input doubles to packed data matching the file
#                 endian-ness
#
#   PARAMETERS:   A list of doubles
#
#   RETURNS:      The packed data
#
#===============================================================================

sub double {
   my ($self,@data) = @_;
   my $result;
   if( $self->{swapbytes} ) {
      $result = reverse pack('d*', reverse @data);
      }
   else {
      $result = pack('d*',@data);
      }
   return $result;
}

#===============================================================================
#
#   SUBROUTINE:   string
#
#   DESCRIPTION:  Packs strings in the format used by the file.  Adds a null
#                 byte then stores with a preceding short count in the files
#                 endian-ness.
#
#   PARAMETERS:   A list of strings
#
#   RETURNS:      The packed strings
#
#===============================================================================

sub string {
   my $self = shift;
   my ($s, $ss, $result);
   for $ss (@_) {
      $s = $ss;
      $s .= "\0";
      $result .= $self->short(length($s)).$s;
      }
    return $result;
}

1;
