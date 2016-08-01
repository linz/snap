#!/usr/bin/perl -pi.bak

s/(\"SourcePath\"\s*\=\s*\"\d+\:)(?:\\\\|[a-z]\:)[\w\\]*?\\\\((?:src|ms)\\\\)/$1..\\\\..\\\\..\\\\$2/i;

