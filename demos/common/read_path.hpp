#ifndef FASTUIDRAW_DEMO_READ_PATH_HPP
#define FASTUIDRAW_DEMO_READ_PATH_HPP

#include <string>
#include <fastuidraw/path.hpp>

/* Read path data from an std::string and append that
   data to path. The format of the input is:

   [ marks the start of a closed outline
   ] marks the end of a closed outline
   { marks the start of an open outline
   } marks the end of an open outline
   [[ marks the start of a sequence of control points
   ]] marks the end of a sequence of control points
   arc marks an arc edge, the next value is the angle in degres
   value0 value1 marks a coordinate (control point of edge point)
 */
void
read_path(fastuidraw::Path &path, const std::string &source,
          std::string *dst_cpp_code = nullptr);

#endif
