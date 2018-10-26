#pragma once

#include <string>
#include <vector>
#include <fastuidraw/path.hpp>

void
extract_path_info(const fastuidraw::Path &path,
                  std::vector<fastuidraw::vec2> *out_pts,
                  std::vector<fastuidraw::vec2> *out_crl_pts,
                  std::vector<fastuidraw::vec2> *out_arc_center_pts,
                  std::string *path_text);
