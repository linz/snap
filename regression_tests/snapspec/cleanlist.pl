#!/bin/perl -pi.bak
BEGIN { @ARGV = map glob, @ARGV; exit if ! @ARGV}
s/^snapspec version \d+\.\d+\.\d+/snapspec version 0.0.0/;
s/^Run at\s+\d+\-\w{3}\-\d{4}\s+\d+\:\d+\:\d+/Run at 1-JAN-2000 00:00:00/;
s/^SNAP run time\:\s+\d+\-\w{3}\-\d{4}\s+\d+\:\d+\:\d+/SNAP run time: 1-JAN-2000 00:00:00/;
s/(took|total)\s+\d+\.\d+\s+seconds/\1 0.00 seconds/g;
s/in\//in\\/g;
