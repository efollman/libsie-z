#!/usr/bin/perl

use strict;

my $not_documentation = 0;

while (<>) {
    chomp;
    s {SIE_DECLARE\(([^\)]+)\)\s+} {
        my $type = $1;
        if ($1 =~ /\*$/) {
            "$type";
        } else {
            "$type ";
        }
    }e;
    if (m(^#ifndef SIE_DOCUMENTATION)) {
        $not_documentation = 1;
    } elsif (m(^#endif /\* SIE_DOCUMENTATION \*/)) {
        $not_documentation = 0;
    } elsif ($not_documentation) {
        next;
    } elsif (m(([^\s\/\*]+))) {
        if (m(^\s*[\s\/]\*\s(.*?)(\s*\*\/)?$)) {
            print "$1\n";
        } else {
            print "    $_\n";
        }
    } else {
        print "\n";
    }
}
