#!/bin/bash

# Adapted from: create_cpp_from_string_resource.sh of WRATH:
#
# Copyright 2013 by Nomovok Ltd.
# Contact: info@nomovok.com
# This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with
# this file, You can obtain one at
# http://mozilla.org/MPL/2.0/.

# Usage:
#   create_cpp_from_string_resource inputfile resourcename outputpath
#     inputfile input file
#     resourcename resource name to fetch the resource from fastuidraw::fetch_static_resource()
#     outputpath directory to place generated cpp file, name of file is $(resourcename).cpp
#
set -e

input_filename="$1"
source_file_name="$2.cpp"
output_directory="$3"
if [ -z "$output_directory" ]; then
    output_directory="."
fi
source_file_name="$output_directory/$source_file_name"

# Contents of the magick perl program taken 
# from: decode_file_chars_to_numbers.pl of WRATH 
# (same license as above)
perl_magick_program='open(FILE, "$ARGV[0]") or die("Unable to open file"); @data = <FILE>; foreach $line (@data) { @ASCII = unpack("C*", $line); foreach (@ASCII) { print "$_,"; } }'

echo -e "#include <fastuidraw/util/static_resource.hpp>\n" > "$source_file_name"
echo -e "namespace { \n\tconst uint8_t values[]={ " >> "$source_file_name"
perl -e "$perl_magick_program" "$1" >> "$source_file_name" || exit 1
echo -e " 0 };\n" >> "$source_file_name"
echo -e " fastuidraw::static_resource R(\"$2\", fastuidraw::const_c_array<uint8_t>(values, sizeof(values)));\n " >> "$source_file_name"
echo -e "\n}\n" >> "$source_file_name"
