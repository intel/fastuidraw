#pragma once

#include <string>
#include <cairo.h>
#include "vec2.hpp"

/* Read path data from an std::string and append that
   data to path. The format of the input is:

   [ marks the start of an outline
   ] marks the end of an outline
   [[ marks the start of a sequence of control points
   ]] marks the end of a sequence of control points
   arc marks an arc edge, the next value is the angle in degres
   value0 value1 marks a coordinate (control point of edge point)
 */
cairo_path_t*
read_path(cairo_t *cr, const std::string &source);
