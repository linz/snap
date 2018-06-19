#!/usr/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Script converts an ASCII representation of grid data
#                      to a binary grid file which can be loaded into CRS for
#                      use by the dbl4_utl_grid.c routines.
#
# PARAMETERS:          def_file   The name of the file defining the grid
#                      grd_file   The name of the binary grid file generated.
#                      
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook          22/06/1999  Created
#===============================================================================

use strict;
use Getopt::Std;
use FindBin;
use lib $FindBin::Bin.'/perllib';
use GridFile;

my %opts;
getopts('F:f:',\%opts);
my $forcefmt = $opts{f} || $opts{F};

my %options = ();
$options{format} = $forcefmt if $forcefmt;

@ARGV==2 || die "Syntax: [-f format] grid_def_file grid_file\n";

# Convert the grid

GridFile::Convert($ARGV[0],$ARGV[1], map { "$_=$options{$_}"} keys %options);
