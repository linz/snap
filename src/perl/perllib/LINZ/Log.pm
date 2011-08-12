################################################################################
#
# $Id: Log.pm 1202 2011-05-04 21:52:46Z jpalmer $
#
# LINZ Log Perl package
#
# Copyright 2011 Crown copyright (c)
# Land Information New Zealand and the New Zealand Government.
# All rights reserved
#
# This program is released under the terms of the new BSD license. See the 
# LICENSE file for more information.
#
################################################################################
package LINZ::Log;

use strict;
use warnings;

use FileHandle;
use POSIX qw(strftime);
use File::Basename qw(fileparse);
use File::Copy;

=head1 NAME

LINZ::Log - configuration file management

Module to preform simple logging to file.

=head2 SYNOPSIS

    use LINZ::Log;

    my $log = LINZ::Log->new('file.log', {level => 'DEBUG'});
    
    $log->info("Some information");
    $log->warn("Warning message");
    $log->die("Error doing something. Stop script") if $fatal;
    $log->debug("Debugging information");
    
    $log->close();


=head1 Description

=over

=item $log = new LINZ::Log( $log_file, $options )

Sets up a new logfile and optionally rotates the an existing files if required

C<$options> is a hash reference that can include logging options. The options are
as follows:

=over 4

=item *

level - The logging output level. can be 'DEBUG', 'INFO', 'WARN', 'ERROR'.
Defaults to 'INFO'.

=item *

rotate_log - Rotate log files after they reach a certain size 0 or 1. Defaults
to 1.

=item *

rotate_size -  If rotate_log is 1 then, the maximum log size before it's rotated.
Size in bytes. Defaults to 1mb.

=back

=item $log->debug($message)

Log debug message to logfile.

=item $log->info($message)

Log information message to log file.

=item $log->error($message)

Log error message to log file.

=item $log->warn($message)

Log warning message to log file.

=item $log->die($message)

Log error message to log file and exit programme.

=item $log->close

Close the log file for writing.

=back

=cut

sub new
{
    my ($class, $filepath, @opts) = @_;
    my $options;
    if ( ref($opts[0]) eq 'HASH')
    {
      $options = $opts[0];
    }
    else
    {
        my %opthash =  @opts;
        $options = \%opthash;  
    }
    my $level = $options->{level} || 'INFO';
    my $rotate = $options->{rotate_log} || 1;
    my $rotate_size = $options->{rotate_size} || (1024 * 1024);
    my %level_map = (
        DEBUG => 0,
        INFO  => 1,
        WARN  => 2,
        ERROR => 3
    );
    $level = uc($level);
    if (!exists $level_map{$level})
    {
        die "Level $level is invalid. Should be one of: "
            . join( ', ', keys %level_map);
    }
    if ($rotate && (-e $filepath && -s $filepath > $rotate_size))
    {
        my ($basename, $path, $suffix) = fileparse($filepath, qr/\.[^.]*/);
        opendir(DIR, $path) || die "Can't open dir $path\n";
        my $last_rot_number = 1;
        foreach my $file (readdir(DIR))
        {
            next if $file !~ /^$basename\_(\d\d)$suffix$/;
            my $file_number = $1;
            if ($file_number >= $last_rot_number)
            {
                $last_rot_number = $file_number+1;
            }
        }
        if ($last_rot_number < 1 || $last_rot_number > 99)
        {
            $last_rot_number = 1;
        }
        closedir(DIR);
        my $rotate_name = sprintf("%s_%.3d%s", $basename, $last_rot_number, $suffix);
        my $rotate_path = File::Spec->catdir($path, $rotate_name);
        move($filepath, $rotate_path) || die "Can't rotate file from $filepath to $rotate_path\n";
    }
    my $fh = FileHandle->new(">>$filepath") || die "Can't open log file $filepath\n";
    my $self = {
        file        => $filepath,
        fh          => $fh,
        level       => $level_map{$level},
        level_map   => \%level_map,
    };
    bless $self, $class;
}

sub DESTROY
{
    my $self = shift;
    if ($self->{fh})
    {
        $self->close();
    }
}

sub debug
{
    _write(@_, 'DEBUG');
}

sub info
{
    _write(@_, 'INFO');
}

sub error
{
    _write(@_, 'ERROR');
}

sub warn
{
    _write(@_, 'WARN');
}


sub die
{
    my $self = shift;
    my $message = shift;
    $self->error($message);
    die "$message\n";
}

sub close
{
    my $self = shift;
    $self->{fh}->close();
    $self->{fh} = undef;
}

sub _write
{
    my $self = shift;
    my $message = shift;
    my $level = shift;
    return if ($self->{level_map}->{$level} < $self->{level});
    my $time = strftime("%Y/%m/%d %H:%M:%S: ", localtime());
    $self->{fh}->print("$time $level: $message\n");
}

1;
