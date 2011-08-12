#!/usr/local/bin/perl
#===============================================================================
#
# PROGRAM:             %M%
#
# VERSION:             %I%
#
# WHAT STRING:         %W%
#
# DESCRIPTION:         Script converts a binary grid file to an ASCII 
#                      representation of the data.  The corresponding script
#                      makegrid.pl can convert the file back to a binary
#                      grid file.
#
# PARAMETERS:          grid_file  The name of the grid file to dump
#                      dump_file  The name of the file created (default is
#                                 to dump to standard output)
#                      -h         Optional switch - if present only header data
#                                 is listed.
#                      -i         Values are represented as integers rather than
#                                 double values
#                      
# DEPENDENCIES:        
#
# MODIFICATION HISTORY
# NAME                 DATE        DESCRIPTION
# ===================  ==========  ============================================
# Chris Crook          22/06/1999  Created
#===============================================================================

# Read the optional switch

use strict;
use FindBin;
use lib $FindBin::Bin.'/perllib';
use GridFile;

# Get options

use Getopt::Std;

my %opts;
my %options = (format=>'ASCII', real=>1);

getopts("iIhH",\%opts);
$options{headeronly} = 1 if $opts{'h'} || $opts{'H'};
$options{real} = 0 if $opts{'i'} || $opts{'I'};

# Test that the file name is present (ie at least one remaining argument)

@ARGV || die <<EOD;
Parameters: [-h] [-i] grid_file dump_file

-h specifies dump header only
-i specifies values are dumped as integers

EOD

# Convert the grid
GridFile::Convert($ARGV[0],$ARGV[1] || '-',map { "$_=$options{$_}"} keys %options);
