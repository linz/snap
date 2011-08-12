################################################################################
#
# $Id: Config.pm 1236 2011-05-15 20:23:41Z ccrook $
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


package LINZ::Config;

=head1 NAME

LINZ::Config - configuration file management

Module to load configuration information from a configuration file.  
Includes support for alternative configurations and testing.

=head2 SYNOPSIS

  use LINZ::Config;

  my $cfg = new LINZ::Config;
  my $cfg = new LINZ::Config( \%options );
  my $cfg = new LINZ::Config( _configpath=>'~/config/myconfig.cfg' );

  my $value = $cfg->some_item
  $value = $cfg->some_item('default_value');
  $value= $cfg->get('some_item');
  $value= $cfg->get('some_item','default_value');

  print "Ok for $item\n" if $cfg->has($item);

  foreach my $k ( $cfg->keys ){ ... }

  $cfg->reload( \%options );

=head1 Description

=over

=item $cfg = new LINZ::Config( \%options );

Loads the configuration (see following sections for details).  

C<options> is a hash reference that can include configuration items 
to override the values read from the configuration.  The special items
_configpath and _configextra override the default configuration location
and provide for loading a second configuration file (see below).

=item $value = $cfg->some_item

=item $value = $cfg->some_item($default_value)

Options to read a configuration value.  If the default value is provided
then the configuration item will be created.  If it is not provided, then
an exception will be thrown if the configuration item is not already 
defined.

=item $cfg->reload( \%options );

Reloads the configuration with alternative options.

=back

=head2 Location of the configuration file(s)

The default configuration file name is based on the name of the calling
module, with the extension replaced with .cfg.  This may be overridden
by including C<{ _configpath=>path }> in the \%options hash.  The config
path specified may include both directory and or filename components. A 
leading "~" will be replaced with the directory of the calling module.  If
it starts with "/" or "\" it is treated as an absolute path, otherwise it
is relative to the path of the calling module.

Once the base configuration file name is determined, to say "mymodule.cfg", 
additional configuration options may be taken from a configuration defined
by the _configextra option.  For example if this is "000",
then the options will be read from "mymodule.cfg.000".

The configuration will also load "mymodule.cfg.test" if it exists. 

The extra and test configuration files can overwrite values from the 
previous files, but cannot create new items.

=head2 Configuration file format

Each item is defined by a line

  name value

in the file, where the value may include whitespace.  Leading and
trailing whitespace will be omitted.  Long values can be entered
using a "perlish" type inline format

  name <<EOL
  ...
  ..
  EOL

where EOL is a string of non-whitespace characters, and the final
line is contains just that string with possible leading and trailing
whitespace.

Configuration item names are case sensitive unless the _case_sensitive=>0
option is used when the configuration is loaded.

Configuration is read from the default configuration file, followed by
the optional extra configuration file, followed by the test configuration
file if it exists.  Each file can override the configuration settings of 
previously loaded files.

When all the files have been loaded additional settings may be read 
from the options supplied in the constructor.  After that all the string 
substitutions are applied as follows.

The values may include substitution strings formatted as {name}, which
will be replaced with the value corresponding to the configuration item
name. This is done recursively, so that ultimately all replacement strings
are removed.  Finally any string {{ or }} are replaced with { or }.

The configuration items should not start with "_" - this is reserved for
special configuration items.  These include the following:

   _runtime      yyyymmddhhmmss
   _runtimestr   yyyy-mm-dd hh:mm:ss
   _year         yyyy
   _month        mm
   _day          dd
   _hour         hh
   _minute       mm
   _second       ss
   _configdir    the directory of the configuration files
   _homedir      the directory of the calling module
   _config_file  the configuration file loaded
   _hostname     the hostname on which the config is loaded

=cut

use vars qw/$AUTOLOAD/;

sub new
{
  my ($class, @opts) = @_;

  my $cfg = bless {}, $class;

  my %opthash =  @opts;
  my $options = ref($opts[0]) eq 'HASH' ? $opts[0] : \%opthash;

  $cfg->{_case_sensitive} = 
       ! defined($options->{_case_sensitive}) || $options->{_case_sensitive} 
       ? 1 : 0;
  my $frame = 0;
  if( $class ne 'LINZ::Config' )
  {
      $frame++ while (caller($frame))[0] ne $class;
      $frame++;
  }
  my $cfgf = (caller($frame))[1];
  $cfgf = (caller(0))[1] if ! $cfgf;
  $cfgf =~ s/\\/\//g;
  $cfgf =~ s/\.[^\.\/]*$/.cfg/;
  $cfgf =~ /^(?:(.*)[\/])?(.*)$/;
  my($p0,$p1) = ($1,$2);  
  $p0 = "." if ! $p0;
  $cfg->{_homedir} = $p0;
  $cfg->{_configdir} = $p0;
  my $hostname;
  eval
  {
  	require Sys::Hostname;
	$hostname = Sys::Hostname::hostname();
  };
  $hostname = $ENV{'HOSTNAME'} if ! $hostname;
  $cfg->{_hostname} = $hostname;

  if( $options && exists($options->{_configpath}))
  {
     my $cp = $options->{_configpath};
     $cp =~ s/\\/\//g;
     $cp = $p0.$1 if $cp =~ /^\~(\/.*)$/;
     $cp .= '/' if -d $cp;
     $cp =~ /^(?:(.*)[\/])?(.*)$/;
     my($c0,$c1) = ($1,$2);
     $c0 = $p0 if ! $c0;
     $c1 = $p1 if ! $c1;
     $cfgf = $c0.'/'.$c1;
     $cfg->{_configdir} = $c0;
  }
  $cfg->_loadDates();
  $cfg->_loadConfiguration($cfgf,$options);
  return $cfg;
}
  
sub reload
{
   my ($cfg,$options) = @_;
   $cfg->_loadConfiguration($cfg->{_config_file},$options);
}

# To avoid it being autoloaded
sub DESTROY
{
}

sub AUTOLOAD
{
   my( $self, $default ) = @_;
   my $var = $AUTOLOAD;
   $var =~ s/.*\:\://;
   return $self->get($var,$default);
}

sub has
{
    my($self,$var) = @_;
    $var = lc($var) if ! $self->{_case_sensitive};
    return exists $self->{$var};
}
    
sub get
{
   my ($self,$var,$default) = @_;
   $var = lc($var) if ! $self->{_case_sensitive};
   return $self->{$var} if exists $self->{$var};
   die "Configuration item \"$var\" is missing\n" if ! defined $default;
   $self->{$var} = $default; 
   return $default;
}

sub keys
{
    my($self) = @_;
    my @keys =  grep { ~ /^_/ } CORE::keys %$self;
    return wantarray ? @keys : \@keys;
}

sub _get2
{
    my( $self, $var ) = @_;
    return $ENV{$1} if $var =~ /^env\:(.+)$/;
    return $self->get($var);
}

sub _loadDates
{
   my($self) = @_;
   return $self->{date} if exists $self->{date};
   my($sc,$mn,$hr,$dy,$mo,$yr) = localtime();
   $mo++;
   $yr+=1900;
   $self->{_runtime} = sprintf("%04d%02d%02d%02d%02d%02d",$yr,$mo,$dy,$hr,$mn,$sc);
   $self->{_runtimestr} = sprintf("%04d-%02d-%02d %02d:%02d:%02d",$yr,$mo,$dy,$hr,$mn,$sc);
   $self->{_year} = sprintf("%04d",$yr);
   $self->{_month} = sprintf("%02d",$mo);
   $self->{_day} = sprintf("%02d",$dy);
   $self->{_hour} = sprintf("%02d",$hr);
   $self->{_minute} = sprintf("%02d",$mn);
   $self->{_second} = sprintf("%02d",$sc);

   return $self->{date};
}

sub _loadConfiguration
{
  my($cfg,$cfgf,$options) = @_;
  my $extra = {};
  foreach my $k (CORE::keys %$cfg){ delete $cfg->{$k} if $k !~ /^_/; }
  $cfg->_loadFile($cfgf);
  my $cfgext = $options->{_configextra};
  $cfg->_loadFile($cfgf.".".$cfgext) if $cfgext;
  my $cfgft = $cfgf.".test";
  $cfg->_loadFile($cfgft,$extra) if -e $cfgft;

  if($options)
  { 
      foreach my $k (CORE::keys %$options)
      { 
          $cfg->{$k} = $options->{$k} if $k !~ /^_/;
          delete $extra->{$k};
      } 
  }

  foreach my $k (CORE::keys %$cfg)
  {
     my $v = $cfg->{$k};
     my $i = 10;
     my $v0 = $v;
     while( $i-- && $v =~ s/(\{\{(?:env\:)?\w+\}\})|{((?:env\:)?\w+)\}/$1 ? $1 : $cfg->_get2($2)/ieg )
     {
        last if $v eq $v0;
        $v0 = $v;
     };
     $cfg->{$k} = $v;
  }

  foreach my $k (CORE::keys %$cfg)
  {
     my $v = $cfg->{$k};
     $v =~ s/\{\{/{/g;
     $v =~ s/\}\}/}/g;
     $cfg->{$k} = $v;
  }
  # Remove extra items not in base configuration
  foreach my $k (CORE::keys %$extra) { delete $cfg->{$k}; }


  $cfg->{_config_file} = $cfgf;
  $cfg->{_confg_extra} = $cfgext;
}

sub _loadFile
{
  my ($cfg,$cfgf,$extra) = @_;
  open(my $f, $cfgf ) || die "Cannot open configuration file $cfgf: $!\n";
  while( my $rec = <$f>)
  {
     next if $rec =~ /^\s*(\#|$)/;
     my ($k,$v) = ($1,$2) if $rec =~ /^\s*(\S+)(?:\s+(\S.*?))?\s*$/;

     # Configuration items starting with _ are reserved
     die "Invalid configuration item name \"$k\" in $cfgf\n"
        if $k =~ /^_/;

     if( $v =~ /^\<\<\s*(\w+)$/ )
     {
	my $re = "^\\s*$1\\s*\$";
	$v = '';
        while( my $l = <$f> )
        {
	    last if $l =~ /$re/;
	    $v .= $l;
	}	
     }
     # Track extra parameters created by configuration add ons so that
     # they can be removed from the configuration after substitution

     $k = lc($k) if ! $cfg->{_case_sensitive};
     $extra->{$k} = 1 if $extra && ! exists($cfg->{$k});
     $cfg->{$k} = $v;
  }
  close($f);
}

1;
