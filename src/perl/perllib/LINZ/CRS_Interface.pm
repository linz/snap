#===============================================================================
# Module:            CRS_Interface.pm
#
# Version:           $Id: CRS_Interface.pm,v 1.5 2001/06/11 19:11:45 cms Exp $  
#
# Description:       Module for writing CRS interface files using a file 
#                    defining valid record formats.
#
# Defines:           Packages: 
#                      CRS_Field
#                      CRS_Record
#                      CRS_Interface
#
# Dependencies:      Uses the following modules: 
#                      FileHandle  
#
# History
# $Log: CRS_Interface.pm,v $
# Revision 1.5  2001/06/11 19:11:45  cms
# Fixed Y3K bug !!!!
#
# Revision 1.4  2000/07/24 23:24:04  cms
# Changed mandatory fields in CRS_Interface.cfg to handle non-contract data.
# Fixed handling of quote characters in strings .. quotes are duplicated in
# the output string.
#
# Revision 1.3  2000/03/29 03:13:12  cms
# Building lib module
#
#===============================================================================
#
# Perl module to create CRS Interface format files.  
#
# This module takes an interface specification defining a number of 
# record formats.  This is used to create a CRS_Interface object which 
# is associated with an output filehandle.  The definition file consists 
# of a number of Header, Record, and Field definitions.  
#
# Field definitions relate to the preceding Record definition.  
# The header definitions are concatenated.
#
# The format of the header record is
#
#  interface type=xxxx attribute=value ...
#
# where the available attributes are:
#   type - transaction type eg SURG
#   version  - format version, default 1.0
#   user_id - default null
#   user_name - default null (unknown if id and name not supplied)
#   software - default perl
#   software_version - default 1.0
#
# This must be followed by a list of record types that are required for
# the header type, as in
#
#  use type1 type2 type3 ...
#
# The format of the record record (!) is
#
#  record type
#
# where type is the current type of the record
#
# The format of the field record is
#
#  field code type required default
#
# where
#   code      is the code used to identify the field to perl
#   type      is the field type (defined below)
#   required  defines whether the field is mandatory (M) or not.
#   default   defines a default value for the field.
#
# type can be one of
#    N##      number (integer) with up to ## digits
#    F        floating point number
#    C##      character string of up to ## characters
#    S/v1/v2  system code with values v1, v2, ..
#    A        autonumber field (increments immediately after each write)
#    D        date field
#    T        date/time field
#
# required can be one of
#    M        mandatory
#    C        conditional
#    O        optional
#    N        not applicable
#
#=================================================================
#
# The CRS_Interface object provides the following methods
#
# create a CRS_Interface file
#
#    $crsi = new CRS_Interface( $filename, $def, attrib=>value, attrib=>value )
#
#    creates a CRS interface file and writes the header record.
#    Here attrib can be any of the header attributes listed above
#          $filename is the name of the file to be created
#          $def is a reference to an array containing the interface definition
#
#  $crsr = $crsi->record($code)
#
#    returns a CRS_Record object for the record of type defined by $code
#
#  $crsi->close
#
#    closes the file, writing record count and trailer information.  This
#    is called by default when the object is destroyed.
#
#======================================================================
#
# The CRS_Record object defines the following methods which are used
# externally (note - the constructor is only used when an interface
# definition is processed.
#
#  $crsr->set( code=>value, code=>value, code=>value, ... )
#
#    Sets one or more data fields to the specified value
#
#  $crsr->set_default( code=>value, code=>value, code=>value, ... )
#
#    Sets the default values for one or more data fields to the specified value
#
#  $crsr->clear( code=>value,... )
#
#    Clears all fields to default values, then sets the specified values
#
#  $value = $crsr->field(code)
#
#    Returns the current value of the field specified by the code
#
#  $id = $crsr->write( code=>value, code=>value )
#
#    Writes the specifed values to the record then writes it the the file
#    and clears the record.  If any fields are type 'A' (autonumber), then
#    they are incremented after writing.  The function returns the value of
#    the last autonumber record in the record (this is the value as written,
#    not the new value held after it is written).
#
#===============================================================================

use strict;
use FileHandle;
   
#===============================================================================
#
#   Class:       CRS_Field
#
#   Description: The CRS_Field object manages the formatted output of 
#                data to the interface file for each supported type.  It
#                also manages setting default values, and auto-increment
#                fields.
#
#                  $field = new CRS_Field($record, $code, $type, $required, $default)
#                  $object->clear()
#                  $object->write($file)
#
#===============================================================================

package LINZ::CRS_Interface::CRS_Field;

use vars qw/%valid_types %months/;

%valid_types = ('N'=>1, 'F'=>1, 'C'=>1, 'S'=>1, 'I'=>1, 'D'=>1, 'T'=>1);
%months = ('JAN'=>1,'FEB'=>2,'MAR'=>3,'APR'=>4,'MAY'=>5,'JUN'=>6,
           'JUL'=>7,'AUG'=>8,'SEP'=>9,'OCT'=>10,'NOV'=>11,'DEC'=>13);


#===============================================================================
#
#   Method:       new
#
#   Description:  $field = new CRS_Field($record, $code, $type, $required, $default)
#
#   Parameters:   $record     The CRS record object to which the field belongs
#                 $code       The field code (id used to reference it)
#                 $type       The field type (see %valid_types above)
#                 $required   True if the field is mandatory
#                 $default    The default value for the field
#
#   Returns:      $field      The CRS_Field object
#
#   Globals:
#
#===============================================================================

sub new {
  my ($class,$record,$code,$type,$required,$default) = @_;
  # Reference types not handled in this script
  $type = 'N10' if $type =~ /^R\:\w+$/i;
  my ($typecode,$length) = $type =~ /(.)(.*)/;
  $typecode = uc($typecode);
  my ($valid, $value);
  &CRS_Interface::Die("Invalid type specified for field $code of record $record\n")
     if ! $valid_types{$typecode};
  if( $typecode eq 'S') {
     foreach (split(/\//,$length)) { ${$valid}{uc($_)} = 1 if $_ ne ''; }
     $length = 0;
     }
  if( $type eq 'I' && $default eq '' ) { $default = 1; }
  if( $type eq 'N' || $type eq 'I' || $type eq 'F') {$default += 0; }
  $value = $default;
  my $self = { record=>$record, code=>$code, type=>$typecode, 'length'=>$length,
     valid=>$valid, value=>$value, default=>$default, required=>$required };
  return bless $self, $class;
  }


#===============================================================================
#
#   Method:       clear
#
#   Description:  Resets the field to its default value
#                 $field->clear()
#
#   Parameters:  
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub clear {
  my ($self) = @_;
  $self->{value} = $self->{default} if $self->{type} ne 'I';
  }


#===============================================================================
#
#   Method:       write
#
#   Description:  Writes the field as formatted data to a file and 
#                 increments autoincrement fields.
#
#                 $object->write($file)
#
#   Parameters:   $file      The output file handle 
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub write {
  my ($self, $file) = @_;
  my ($record,$code,$value,$type,$length,$valid,$required) =
        @{$self}{'record','code','value','type','length','valid','required'};
  if( $value eq '' || ! defined($value) ) {
      &LINZ::CRS_Interface::Die("Mandatory field $code missing for record $record\n") if $required eq 'M';
  }
  elsif( $required eq 'N' ) {  # Skip not applicable fields without notifying user...
  }
  elsif( $type eq 'N' || $type eq 'I' ) {
      &LINZ::CRS_Interface::Die("Invalid number $value for field $code in record $record\n")
        if $value !~ /[+-]?\d+/;
      print $file $value;
      }
  elsif( $type eq 'F' ) {
      &LINZ::CRS_Interface::Die("Invalid number $value for field $code in record $record\n")
        if $value !~ /[+-]?\d+(\.\d*)?(e[+-]?\d+)?/;
      print $file $value;
      }
  elsif( $type eq 'C' ) {
      &LINZ::CRS_Interface::Die("Character string $value too long for field $code in record $record\n")
        if length($value) > $length;
      $value =~ s/\"/\"\"/g;
      print $file "\"$value\"";
      }
  elsif( $type eq 'S' ) {
      $value = uc($value);
      &LINZ::CRS_Interface::Die("Invalid system code $value for field $code in record $record\n")
        if ! ${$valid}{$value};
      print $file $value;
      }
  elsif( $type eq 'D' ) {
      my ($valid,$yr,$mo,$dy,$hr,$mi,$sc) = &SplitDate($value);
      &LINZ::CRS_Interface::Die("Invalid date format $value for field $code in record $record\n") if ! $valid;
      printf $file "%02d/%02d/%4d",$dy,$mo,$yr;
      }
  elsif( $type eq 'T' ) {
      my ($valid,$yr,$mo,$dy,$hr,$mi,$sc) = &SplitDate($value);
      &LINZ::CRS_Interface::Die("Invalid date/time format $value for field $code in record $record\n") if ! $valid;
      printf $file "%4d-%02d-%02d %2d:%02d:%02d",$yr,$mo,$dy,$hr,$mi,$sc;
      }

  $self->{value}++ if $type eq 'I';
  }


#===============================================================================
#
#   Subroutine:   SplitDate
#
#   Description:  Interprets a string defining a date. This is as general
#                 as possible, but prefers 4 digit years (to avoid ambiguity
#                 between year and day) and prefers YMD formats.  Will not
#                 work with American format dates (DMY).  The date can
#                 optionally be followed by a time.
#
#                 ($valid,$year,$month,$day,$hour,$min,$sec) 
#                                 = &CMS_Field::SplitDate($string)
#
#   Parameters:   $string    The date to interpret
#
#   Returns:      $valid     True if the string could be interpreted
#                 $year      The year (4 digit)
#                 $month     The month (1-12)
#                 $day       The day
#                 $hour      The hour (if defined)
#                 $min       The minutes
#                 $sec       The seconds
#
#   Globals:
#
#===============================================================================

sub SplitDate {
  my $datestring = shift;
  return 0 if $datestring !~ /^\s*(\d+)\s*(\D)\s*(\d+|[A-Za-z]+)\s*\2\s*(\d+)(?:\s+(\d+)\s*(\D)\s*(\d+)(?:\s*\6\s*(\d+\.?\d*))?)?\s*$/;
  my ($f1,$f2,$f3,$f4,$f5,$f6) = ($1,$3,$4,$5,$7,$8);
  if($f3 > 31) { my $tmp = $f1; $f1 = $f3; $f3 = $tmp; }
  if($f2 !~ /\d+/) { $f2 = $months{uc($f2)};}
  $f1 += 1900 if $f1 > 70 && $f1 < 200;  # Kludge for 2 digit dates - not Y2K
  my $valid = 1;
  $valid = 0 if $f1 < 1000;
  $valid = 0 if $f2 < 1 || $f2 > 12;
  $valid = 0 if $f3 < 1 || $f3 > 31;
  $valid = 0 if $f4 < 0 || $f4 > 24;
  $valid = 0 if $f5 < 0 || $f5 > 60;
  $valid = 0 if $f6 < 0 || $f5 > 60;
  return ($valid,$f1,$f2,$f3,$f4,$f5,$f6);
}

   
#===============================================================================
#
#   Class:       CRS_Record
#
#   Description: The CRS_Record object manages a record type in a CRS file.
#            
#                Defines the following routines:
#                  $record = new CRS_Record($code, $fh)
#                  $record->clear(%set)
#                  $record->write(%values)
#                  $record->field($code)
#                  $record->set(%values)
#                  $record->set_default(%values)
#                  $record->add_field($code, $type, $required, $default)
#
#===============================================================================

package LINZ::CRS_Interface::CRS_Record;


#===============================================================================
#
#   Method:       new
#
#   Description:  Creates the record object.
#                 $record = new CRS_Record($code, $fh)
#
#   Parameters:   $code     The code defining the record type (as used in
#                           the interface file and the configuration file.
#                 $fh       The file handle of the interface object to which
#                           the record is to be written
#
#   Returns:      $record   The CRS_Record object
#
#   Globals:
#
#===============================================================================

sub new {
  my($class,$record, $fh )= @_;
  my $self = { record=>uc($record), count=>0, field_index=>{}, fields=>[], fh=>$fh };
  return bless $self, $class;
}


#===============================================================================
#
#   Method:       clear
#
#   Description:  Resets the fields of a record to default values, and 
#                 optionally sets field values.
#
#                 $object->clear(%set)
#
#   Parameters:   %set        An optional hash defining fields to set and
#                             the values to set them to.
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub clear {
  my($self,%set)=@_;
  foreach (@{$self->{fields}}) { $_->clear; }
  $self->set( %set ) if %set;
  }


#===============================================================================
#
#   Method:       write
#
#   Description:  Writes the interface record to the file optionally setting
#                 some field values first.  Returns the auto-increment id of
#                 the record written, if it contains an auto-increment field
#
#                  $id = $record->write(%values)
#
#   Parameters:   %values     A hash defining fields to set and their new
#                             values.
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub write {
  my ($self, %values ) = @_;
  $self->set(%values) if %values;
  my $result =  $self->{id_field} ? $self->{id_field}->{value} : undef;
  my $fh = $self->{fh};
  print $fh $self->{record};
  foreach (@{$self->{fields}}) { print $fh ","; $_->write($fh); }
  print $fh "\n";
  $self->{count}++;
  $self->clear;
  return $result;
  }


#===============================================================================
#
#   Method:       field
#
#   Description:  Returns the current value of a specified field in the
#                 record. For autoincrement fields this is the value that
#                 will be used for the next record written.
#                   $value = $record->field($code)
#
#   Parameters:   $code       The field code
#
#   Returns:      $value      The current value of the field
#
#   Globals:
#
#===============================================================================

sub field {
  my ($self, $code) = @_;
  my ($record, $field_index) = @{$self}{'record','field_index'};
  $code = uc($code);
  my $field = $field_index->{$code};
  &LINZ::CRS_Interface::Die("Invalid field $code request for record $record\n",join("\n",sort keys %$field_index) ) if ! defined $field;
  return $field->{value};
}


#===============================================================================
#
#   Method:       set
#
#   Description:  Sets the values of fields in the record
#                  $record->set(%values)
#
#   Parameters:   %values     A hash containing field codes and values to set
#                             the fields to.
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub set {
  my ($self, %values) = @_;
  my ($record, $field_index) = @{$self}{'record','field_index'};
  my ($code,$value);
  while( ($code,$value) = each(%values) ) {
     $code = uc($code);
     my $field = $field_index->{$code};
     &LINZ::CRS_Interface::Die("Invalid field $code request for record $record\n",join("\n",sort keys %$field_index) ) if ! defined $field;
     $field->{value} = $value;
     }
  }



#===============================================================================
#
#   Method:       set_default
#
#   Description:  Sets the default values of one or more fields
#                  $record->set_default(%values)
#
#   Parameters:   %values     A hash defining fields and the default values
#                             to set them to
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub set_default {
  my ($self, %values) = @_;
  my ($record, $field_index) = @{$self}{'record','field_index'};
  my ($code,$value);
  while( ($code,$value) = each(%values) ) {
     $code = uc($code);
     my $field = $field_index->{$code};
     &LINZ::CRS_Interface::Die("Invalid field $code request for record $record\n",join("\n",sort keys %$field_index)) if ! defined $field;
     $field->{default} = $value;
     }
  }


#===============================================================================
#
#   Method:       add_field
#
#   Description:  Adds a field definition to the record.
#                   $record->add_field($code, $type, $required, $default)
#
#   Parameters:   $code       The code used to identify the field
#                 $type       The type of field
#                 $required   True if the field is mandatory
#                 $default    The default value for the field
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub add_field {
  my ($self,$code,$type,$required,$default) = @_;
  $code = uc($code);
  my $field = new LINZ::CRS_Interface::CRS_Field $self->{record}, $code, $type, $required, $default;
  push( @{$self->{fields}}, $field );
  $self->{field_index}->{$code} = $field;
  $self->{id_field} = $field if $field->{type} eq 'I';
}

   
#===============================================================================
#
#   Class:       CRS_Interface
#
#   Description: Defines the following routines:
#                  $interface = new CRS_Interface($reqtype, $file, $definition, %attribs)
#                  $record = $interface->record($record)
#
#===============================================================================

package LINZ::CRS_Interface;


#===============================================================================
#
#   Method:       new
#
#   Description:  Creates a CRS interface object
#
#                  $interface = new CRS_Interface($reqtype, $file, 
#                                                  $definition, %attribs)
#
#   Parameters:   $reqtype    The interface type as defined by interface
#                             records in the file.
#                 $file       The name of the interface file to create
#                 $definition The definition of the interface as an array
#                             reference to an array of strings. This will
#                             be read from the configuration file if it is
#                             not defined.
#                 %attribs    File header attributes
#
#   Returns:      $interface  The CRS_Interface object
#
#   Globals:
#
#===============================================================================

sub new {
    my( $class, $reqtype, $file, $definition, %attribs ) = @_;
    my $fh = new FileHandle ">$file";
    &Die("Cannot create CRS interface file $file\n") if ! $fh;
    my %records;
    my $current_record;
    my $record_name;
    my %header= ( 'VERSION'=>'1.0', 'SOFTWARE'=>'perl', 'SOFTWARE_VERSION'=>'1.0');
    my $inttype;
    my %userec;
    $reqtype = uc($reqtype);
    my $section;

    $definition = &default_definition if ! $definition;

    foreach (@$definition) {
        s/\r?\n$//;
        s/\#.*//;  # Skip comments.
        my($rectype,$data)=split(' ',$_,2);
        
        $rectype = uc($rectype);
        if( $rectype eq 'INTERFACE') {
            $section = 'INTERFACE';
            $current_record = undef;
            $data =~ /^\s*(\S+)\s*(.*)$/;
            $inttype = uc($1);
            $data = $2;
            if( $inttype eq $reqtype ) { 
               $header{TYPE} = $inttype;
               while( $data =~ /(\S+)\s*\=\s*(\S+)/g ) { $header{uc($1)} = $2; }
               }
        }
        elsif( $rectype eq 'USE' ) {
            &Die("Cannot specify USE without an INTERFACE definition\n")
               if $section ne 'INTERFACE';
            if( $inttype eq $reqtype ) {
               foreach (split(' ',$data)) { $userec{uc($_)} = 1; }
               }
        }
        elsif( $rectype eq 'RECORD' ) {
            $section = 'RECORD';
            $current_record = undef;
            $data =~ /(\w+)/;
            $record_name = uc($1);
            if( $userec{$record_name} ) {
               delete $userec{$record_name};
               $current_record = new LINZ::CRS_Interface::CRS_Record $record_name, $fh;
               $records{$data} = $current_record;
               }
        }
        elsif( $rectype eq 'FIELD' && $current_record ) {
            my ($code,$type,$req,$default) = split(' ',$data);
            $code = uc($code);
            &Die("Cannot define field $code before a record is defined\n") 
                if $section ne 'RECORD';
            &Die("Data missing for field $code in record $record_name\n") 
                if ! $type;
            $current_record->add_field( $code, $type, $req, $default );
        }
    }
    foreach (keys %attribs) { $header{uc($_)} = $attribs{$_} };
    $header{'USERNAME'}='unknown' if $header{'USERNAME'} eq '' && $header{'USERID'} eq '';
    my($sc,$mi,$hr,$dy,$mo,$yr) = localtime;
    $yr += 1900;
    $header{'DATE'} = sprintf("%4d-%02d-%02d %02d:%02d:%02d",$yr,$mo+1,$dy,$hr,$mi,$sc);

    &Die("Interface Header type missing\n") if $header{'TYPE'} eq '';

    print $fh "HEDR,",join(',', 
         @header{'TYPE','VERSION','USERID','USERNAME','DATE','SOFTWARE','SOFTWARE_VERSION'})
         ,",\n";
    my $self = { file=>$file, records=>\%records, fh=>$fh };
    return bless $self,$class;
}


#===============================================================================
#
#   Subroutine:   default_definition
#
#   Description:  Loads the interface file definition from a file with
#                 the same name as the module but with extension .cfg
#
#   Parameters:   
#
#   Returns:      $def      A reference to an array of strings 
#
#   Globals:
#
#===============================================================================

sub default_definition {
    my $deffile = (caller())[1];
    $deffile =~ s/\.pm$/.cfg/;
    local (*DEF);
    open(DEF,$deffile) || return undef;
    my @def = <DEF>;
    close(DEF);
    return \@def;
}


#===============================================================================
#
#   Method:       record
#
#   Description:  Returns a CRS_Record object for the specified record type
#                   $record = $interface->record($type)
#
#   Parameters:   $type      The record type
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub record {
    my($self,$record) = @_;
    $record = uc($record);
    my $rc = $self->{records}->{$record};
    Die("Interface record $record not defined\n") if ! $rc;
    return $rc;
}


#===============================================================================
#
#   Subroutine:   close
#
#   Description:  Writes a count of each record type in the file, writes
#                 the file trailer, and closes the file.
#
#                 $interface->close
#
#   Parameters:   
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub close {
    my $self = shift;
    my $fh = $self->{fh};
    return if ! $fh;
    foreach (sort keys %{$self->{records}}) {
         print $fh "CONT,",$self->{records}->{$_}->{'record'},",",
                           $self->{records}->{$_}->{count},"\n"; }
    print $fh "TRLR\n";
    $fh->close;
    $self->{fh} = undef;
}


#===============================================================================
#
#   Subroutine:   DESTROY
#
#   Description:  Ensures that the close routine is called to terminate
#                 the interface file correctly.
#
#   Parameters:   
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub DESTROY {
    my $self = shift;
    $self->close;
}


#===============================================================================
#
#   Subroutine:   Die
#
#   Description:  A local version of die that can be modified to log errors.
#
#   Parameters:   &CRS_Interface::Die(@messages)
#
#   Returns:      
#
#   Globals:
#
#===============================================================================

sub Die {
    my @message = @_;
    die @message;
}


1;
