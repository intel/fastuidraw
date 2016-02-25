#!/usr/bin/perl

# Adapted from: decode_file_chars_to_numbers.pl of WRATH:
#
# Copyright 2013 by Nomovok Ltd.
# Contact: info@nomovok.com
# This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with
# this file, You can obtain one at
# http://mozilla.org/MPL/2.0/.

# open file
open(FILE, "$ARGV[0]") or die("Unable to open file");

# read file into an array
@data = <FILE>;

foreach $line (@data)
{
    @ASCII = unpack("C*", $line);
    foreach (@ASCII) {
        print "$_,";
    }
}
