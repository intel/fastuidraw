#pragma once

#include <istream>
#include <fastuidraw/colorstop.hpp>

/* Read a color stop from an std::stream. The file is formatted as:
    stop_time red green blue alpha
    stop_time red green blue alpha
    .
    .

where each stop_time is a floating point value in the range [0.0, 1.0]
and the value red, green, blue and alpha are integers in the range [0,255]
 */
void
read_colorstops(fastuidraw::ColorStopSequence &seq, std::istream &input);
