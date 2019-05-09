#pragma once

#include <istream>
#include <vector>
#include <fastuidraw/painter/data/painter_dashed_stroke_params.hpp>

void
read_dash_pattern(std::vector<fastuidraw::PainterDashedStrokeParams::DashPatternElement> &pattern_out,
                  std::istream &input_stream);
